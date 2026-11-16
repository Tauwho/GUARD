#include "replay.hpp"
#include "guard/gate_fsm.hpp"
#include "guard/planner.hpp"
#include "guard/supervisor.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>

namespace guard::host {
namespace {
const char *mode_name(GateMode mode) {
    const char *names[] = {"INIT", "LATCHED", "READY", "MOVING", "STOPPING", "STOPPED"};
    return names[static_cast<unsigned>(mode)];
}
class Telemetry {
  public:
    explicit Telemetry(std::ostream &output) : output_(output) { output_ << std::setprecision(17); }
    void begin(std::int64_t now, const char *kind) {
        output_ << "{\"v\":1,\"t_ns\":" << now << ",\"src\":\"harness\",\"seq\":" << sequence_++
                << ",\"kind\":\"" << kind << "\",\"payload\":{";
    }
    std::ostream &out() { return output_; }
    void end() {
        output_ << "}}\n";
        if (!output_)
            throw std::runtime_error("telemetry write failed");
    }

  private:
    std::ostream &output_;
    std::uint64_t sequence_ = 0;
};
} // namespace
ReplayResult replay(const MapSpec &map, const ScenarioSpec &scenario, Config config,
                    std::uint64_t seed, std::ostream &output) {
    if (!valid(config) || !std::isfinite(scenario.duration) || scenario.duration <= 0 ||
        scenario.duration > 600 || scenario.rate_hz == 0 || 1000 % config.obs_rate_hz ||
        1000 % config.t_tick_ms)
        throw std::runtime_error("invalid replay configuration");
    World world(map, scenario);
    Plant plant(config, map.start);
    Planner planner(map, config);
    Supervisor supervisor(config);
    Gate gate(config);
    Telemetry telemetry(output);
    ReplayResult result;
    RegisterReply registration;
    const bool guarded = config.mode == Mode::guarded;
    if (guarded && !gate.register_supervisor(registration))
        throw std::runtime_error("register failed");
    Lease active;
    Lease zombie;
    std::uint64_t lease_sequence = 0;
    bool boot = guarded, dead = false, previous_contact = false, previous_zone = false;
    std::array<bool, 9> faults{};
    GateMode previous_mode = gate.status(0).mode;
    auto previous_counters = gate.status(0).counters;
    telemetry.begin(0, "run_start");
    telemetry.out() << "\"scenario\":\"F" << scenario.id << "\",\"seed\":" << seed
                    << ",\"mode\":\"B" << static_cast<unsigned>(config.mode)
                    << "\",\"model\":\"virtual-time\"";
    telemetry.end();
    const auto duration = static_cast<std::int64_t>(scenario.duration * 1000);
    for (std::int64_t ms = 0; ms <= duration; ++ms) {
        const auto now = ms * 1'000'000;
        if (!world.update(now))
            throw std::runtime_error("invalid world timeline");
        for (unsigned fault = 1; fault <= 8; ++fault) {
            const bool enabled = world.active_fault(fault, now) != nullptr;
            if (enabled != faults[fault]) {
                telemetry.begin(now, "fault");
                telemetry.out() << "\"fault\":\"F" << fault << "\",\"phase\":\""
                                << (enabled ? "start" : "end") << "\"";
                telemetry.end();
                if (fault == 3 && enabled) {
                    dead = true;
                    gate.supervisor_lost();
                }
                if (fault == 7 && enabled && guarded) {
                    zombie = active;
                    if (!gate.register_supervisor(registration))
                        throw std::runtime_error("replacement registration failed");
                    supervisor = Supervisor(config);
                }
                if (fault == 7 && !enabled && zombie.lease_id) {
                    const auto ack = gate.lease(zombie, now);
                    telemetry.begin(now, "lease");
                    telemetry.out()
                        << "\"lease_id\":" << zombie.lease_id << ",\"epoch\":" << zombie.epoch
                        << ",\"reason\":\"" << reason_name(ack.reason) << "\"";
                    telemetry.end();
                }
                faults[fault] = enabled;
            }
        }
        if (ms > 0 && ms % config.t_tick_ms == 0) {
            if (!plant.step(now, world.scene()))
                throw std::runtime_error("plant step failed");
            const auto truth = plant.ground_truth();
            result.minimum_margin = std::min(result.minimum_margin, truth.clearance);
            for (unsigned event = 0; event < 2; ++event) {
                const bool enabled = event == 0 ? truth.contact : truth.zone_breach;
                bool &previous = event == 0 ? previous_contact : previous_zone;
                if (enabled && !previous) {
                    if (event == 0)
                        ++result.contacts;
                    else
                        ++result.zone_breaches;
                    telemetry.begin(now, "sim_event");
                    telemetry.out()
                        << "\"code\":\"" << (event == 0 ? "SIM_CONTACT" : "SIM_ZONE_BREACH")
                        << "\",\"margin_m\":" << truth.clearance;
                    telemetry.end();
                }
                previous = enabled;
            }
        }
        if (ms % (1000 / config.obs_rate_hz) == 0) {
            auto observation = plant.sample(now, world.scene());
            const auto *dropout = world.active_fault(4, now);
            if (!(dropout && dropout->supervisor_feed) && !dead &&
                !supervisor.observe(observation, now))
                throw std::runtime_error("supervisor observation failed");
            observation.feed = 1;
            if (!(dropout && !dropout->supervisor_feed) && !planner.observe(observation))
                throw std::runtime_error("planner observation failed");
            telemetry.begin(now, "observation");
            telemetry.out() << "\"obs_id\":" << observation.obs_id << ",\"seq\":" << observation.seq
                            << ",\"feed\":\"planner\",\"x\":" << observation.x
                            << ",\"y\":" << observation.y << ",\"theta\":" << observation.theta
                            << ",\"v_meas\":" << observation.v_meas
                            << ",\"w_meas\":" << observation.w_meas
                            << ",\"map_version\":" << observation.map_version
                            << ",\"sensor_ok\":true,\"obstacles\":[";
            for (unsigned i = 0; i < observation.obstacle_count; ++i) {
                if (i)
                    telemetry.out() << ',';
                const auto &obstacle = observation.obstacles[i];
                telemetry.out() << "{\"x\":" << obstacle.x << ",\"y\":" << obstacle.y
                                << ",\"r\":" << obstacle.r << '}';
            }
            telemetry.out() << ']';
            telemetry.end();
        }
        if (!guarded || (!dead && !faults[2])) {
            if (guarded && gate.status(now).mode == GateMode::moving) {
                const auto reason = supervisor.recheck(active, world.scene(), now);
                if (reason) {
                    Revoke revoke;
                    revoke.lease_id = active.lease_id;
                    revoke.epoch = registration.epoch;
                    revoke.reason = reason;
                    gate.revoke(revoke);
                    telemetry.begin(now, "revoke");
                    telemetry.out() << "\"reason\":\"" << reason_name(reason)
                                    << "\",\"lease_id\":" << active.lease_id;
                    telemetry.end();
                }
            }
            PlannerOptions options;
            options.cruise_v = scenario.cruise_v;
            options.rate_hz = scenario.rate_hz;
            options.lease_req_ms = scenario.lease_req_ms;
            if (scenario.planner == ScenarioSpec::Planner::shortcut)
                options.mode = PlannerMode::shortcut;
            if (scenario.planner == ScenarioSpec::Planner::aggressive)
                options.mode = PlannerMode::aggressive;
            if (const auto *fault = world.active_fault(1, now)) {
                options.mode = PlannerMode::lag;
                options.lag_ms = fault->lag_ms;
            }
            if (const auto *fault = world.active_fault(5, now)) {
                options.mode = PlannerMode::aggressive;
                options.cruise_v = fault->v_cmd;
            }
            if (const auto *fault = world.active_fault(8, now)) {
                options.mode = PlannerMode::flood;
                options.rate_hz = fault->rate_hz;
            }
            Proposal proposal;
            if (!boot && planner.propose(now, options, proposal)) {
                ++result.proposals;
                if (!guarded) {
                    PassthruCmd command;
                    command.cmd_seq = proposal.seq;
                    command.v = proposal.v_cmd;
                    command.w = proposal.w_cmd;
                    command.stamp_ns = now;
                    const auto reason = gate.passthrough(command, now);
                    if (reason != Reason::GATE_PASSTHRU_ACCEPTED)
                        ++result.rejected;
                    telemetry.begin(now, "proposal");
                    telemetry.out() << "\"cmd_seq\":" << command.cmd_seq << ",\"v\":" << command.v
                                    << ",\"w\":" << command.w << ",\"reason\":\""
                                    << reason_name(static_cast<unsigned>(reason)) << "\"";
                    telemetry.end();
                } else {
                    auto evaluation = supervisor.evaluate(proposal, world.scene(), now);
                    bool accepted = false;
                    unsigned reason = static_cast<unsigned>(evaluation.primary_reason);
                    if (evaluation.authorized) {
                        Lease lease;
                        lease.lease_id = lease.lease_seq = ++lease_sequence;
                        lease.proposal_id = proposal.proposal_id;
                        lease.epoch = registration.epoch;
                        lease.v_cmd = proposal.v_cmd;
                        lease.w_cmd = proposal.w_cmd;
                        lease.v_cap = config.v_max;
                        lease.issued_ns = now;
                        lease.expires_ns =
                            now + milliseconds(std::min(config.t_lease_ms, proposal.lease_req_ms));
                        const auto ack = gate.lease(lease, now);
                        accepted = ack.reason == 500;
                        if (accepted) {
                            active = lease;
                            telemetry.begin(now, "lease");
                            const auto *source = supervisor.latest();
                            telemetry.out()
                                << "\"lease_id\":" << lease.lease_id << ",\"epoch\":" << lease.epoch
                                << ",\"issued_ns\":" << lease.issued_ns
                                << ",\"expires_ns\":" << lease.expires_ns
                                << ",\"v_cmd\":" << lease.v_cmd << ",\"w_cmd\":" << lease.w_cmd
                                << ",\"d_req\":" << evaluation.required_distance;
                            if (source)
                                telemetry.out() << ",\"x\":" << source->x << ",\"y\":" << source->y
                                                << ",\"theta\":" << source->theta;
                            telemetry.end();
                        } else {
                            reason = static_cast<unsigned>(Reason::REJ_GATE_NOT_READY);
                            telemetry.begin(now, "lease");
                            telemetry.out() << "\"lease_id\":" << lease.lease_id << ",\"reason\":\""
                                            << reason_name(ack.reason) << "\"";
                            telemetry.end();
                        }
                    }
                    if (!accepted)
                        ++result.rejected;
                    telemetry.begin(now, "decision");
                    telemetry.out()
                        << "\"proposal_id\":" << proposal.proposal_id
                        << ",\"planner_id\":" << proposal.planner_id << ",\"verdict\":\""
                        << (accepted                        ? "AUTHORIZED"
                            : reason >= 300 && reason < 400 ? "UNVERIFIABLE"
                                                            : "REJECTED")
                        << "\",\"primary_reason\":\"" << reason_name(reason) << "\",\"reasons\":[";
                    bool comma = false;
                    for (const auto code : evaluation.reasons)
                        if (code) {
                            if (comma)
                                telemetry.out() << ',';
                            telemetry.out() << '"' << reason_name(code) << '"';
                            comma = true;
                        }
                    if (reason == static_cast<unsigned>(Reason::REJ_GATE_NOT_READY))
                        telemetry.out() << '"' << reason_name(reason) << '"';
                    telemetry.out() << "],\"lease_id\":" << (accepted ? active.lease_id : 0)
                                    << ",\"epoch\":" << registration.epoch;
                    telemetry.end();
                }
            }
        }
        if (ms % config.t_tick_ms == 0) {
            const auto motor = gate.tick(now, plant.linear_velocity(), plant.angular_velocity());
            if (!plant.command(motor, now))
                throw std::runtime_error("motor command failed");
            if (boot && ms >= config.t_tick_ms * config.standstill_ticks) {
                RecoverRequest request;
                request.request_id = 1;
                if (gate.recover(request, {true, true, true}).reason != 512)
                    throw std::runtime_error("boot recovery failed");
                boot = false;
            }
            const auto status = gate.status(now);
            if (status.mode != previous_mode) {
                unsigned reason = 511;
                if (status.mode == GateMode::ready && previous_mode == GateMode::latched)
                    reason = 512;
                if (status.mode == GateMode::moving)
                    reason = guarded ? 500 : 514;
                if (status.mode == GateMode::stopping)
                    for (unsigned code = 506; code <= 510; ++code)
                        if (status.counters[code] > previous_counters[code])
                            reason = code;
                if (status.mode == GateMode::stopping &&
                    status.counters[515] > previous_counters[515])
                    reason = 515;
                telemetry.begin(now, "gate_transition");
                telemetry.out() << "\"from\":\"" << mode_name(previous_mode) << "\",\"to\":\""
                                << mode_name(status.mode) << "\",\"reason\":\""
                                << reason_name(reason) << "\",\"epoch\":" << status.epoch;
                telemetry.end();
                previous_mode = status.mode;
            }
            previous_counters = status.counters;
        }
        result.ended_ns = now;
        if (planner.at_goal()) {
            result.goal = true;
            break;
        }
    }
    telemetry.begin(result.ended_ns, "run_end");
    telemetry.out() << "\"final_state\":\""
                    << (result.goal ? "GOAL" : mode_name(gate.status(result.ended_ns).mode))
                    << "\",\"contacts\":" << result.contacts
                    << ",\"zone_breaches\":" << result.zone_breaches
                    << ",\"proposals\":" << result.proposals << ",\"rejected\":" << result.rejected
                    << ",\"minimum_margin_m\":" << result.minimum_margin;
    telemetry.out() << ",\"time_to_goal_s\":";
    if (result.goal)
        telemetry.out() << result.ended_ns / 1e9;
    else
        telemetry.out() << "null";
    telemetry.end();
    return result;
}
} // namespace guard::host
