#include "guard/planner.hpp"
#include "guard/codec.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace guard {
namespace {
constexpr double pi = 3.14159265358979323846;
double distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
} // namespace
Planner::Planner(MapSpec map, Config config, std::uint32_t id)
    : map_(map), config_(config), id_(id) {}

bool Planner::observe(const Observation &value) {
    if (!valid(value) || value.feed != 1 ||
        (count_ && (value.obs_id <= history_[(next_ + 63) % 64].obs_id ||
                    value.stamp_ns < history_[(next_ + 63) % 64].stamp_ns)))
        return false;
    if (!count_)
        planned_map_version_ = value.map_version;
    history_[next_] = value;
    next_ = (next_ + 1) % history_.size();
    count_ = std::min(count_ + 1, history_.size());
    return true;
}
void Planner::decision(const Decision &reply) {
    if (valid(reply) && reply.proposal_id == previous_.proposal_id)
        retry_nominal_ = reply.verdict == 2 && reply.primary_reason == 212;
}
bool Planner::at_goal() const {
    if (!count_)
        return false;
    const auto &obs = history_[(next_ + 63) % 64];
    return distance({obs.x, obs.y}, {map_.goal.x, map_.goal.y}) <= 0.15;
}
bool Planner::propose(std::int64_t now, const PlannerOptions &options, Proposal &output) {
    if (!count_ || now < 0 || !valid(config_) || !id_ || !valid(map_.scene) ||
        map_.waypoint_count > map_.waypoints.size() || options.rate_hz == 0 ||
        options.rate_hz > 1000 || !std::isfinite(options.cruise_v) || options.cruise_v < 0 ||
        options.cruise_v > 1.2 || options.lease_req_ms < 50 || options.lease_req_ms > 1000 ||
        (last_proposal_ >= 0 &&
         (now < last_proposal_ || now - last_proposal_ < 1'000'000'000 / options.rate_hz)))
        return false;
    const auto &latest = history_[(next_ + 63) % 64];
    if (latest.stamp_ns > now)
        return false;
    const Observation *source = &latest;
    if (options.mode == PlannerMode::lag) {
        if (now < milliseconds(options.lag_ms))
            return false;
        source = nullptr;
        for (std::size_t i = 0; i < count_; ++i) {
            const auto &candidate = history_[(next_ + 63 - i) % 64];
            if (candidate.stamp_ns <= now - milliseconds(options.lag_ms)) {
                source = &candidate;
                break;
            }
        }
        if (!source)
            return false;
    }
    if (options.mode == PlannerMode::replay) {
        if (!previous_.seq)
            return false;
        output = previous_;
        last_proposal_ = now;
        return true;
    }
    if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        return false;
    Proposal proposal;
    proposal.proposal_id = proposal.seq = sequence_ + 1;
    proposal.planner_id = id_;
    proposal.lease_req_ms = options.lease_req_ms;
    proposal.created_ns = now;
    proposal.based_on = {source->obs_id, source->seq, source->stamp_ns, source->map_version, 0};
    if (options.mode == PlannerMode::shortcut)
        proposal.based_on.map_version = planned_map_version_;
    const Point position{source->x, source->y};
    Point target{map_.goal.x, map_.goal.y};
    if (options.mode != PlannerMode::shortcut) {
        while (waypoint_ < map_.waypoint_count &&
               distance(position, map_.waypoints[waypoint_]) <= 0.15)
            ++waypoint_;
        if (waypoint_ < map_.waypoint_count)
            target = map_.waypoints[waypoint_];
    }
    const double remaining = distance(position, target);
    const double heading = std::remainder(
        std::atan2(target.y - position.y, target.x - position.x) - source->theta, 2 * pi);
    if (remaining > 0.15) {
        proposal.w_cmd = std::clamp(2 * heading, -config_.w_max, config_.w_max);
        double desired =
            std::abs(heading) > 0.35
                ? 0
                : std::min(options.cruise_v,
                           std::sqrt(2 * config_.a_brake * std::max(0.0, remaining - 0.10)));
        // Independent local yielding rule. It uses only the cited observation.
        const double reaction =
            (config_.t_lease_ms + config_.t_tick_ms + config_.t_act_ms) / 1000.0;
        for (std::size_t i = 0; (options.mode != PlannerMode::aggressive || retry_nominal_) &&
                                i < source->obstacle_count;
             ++i) {
            const auto &obstacle = source->obstacles[i];
            const double dx = obstacle.x - position.x, dy = obstacle.y - position.y;
            const double ahead = dx * std::cos(source->theta) + dy * std::sin(source->theta);
            const double lateral =
                std::abs(-dx * std::sin(source->theta) + dy * std::cos(source->theta));
            const double radius = obstacle.r + config_.r_robot_m + config_.margin_m + 0.15;
            if (ahead >= 0 && lateral < radius) {
                const double free =
                    std::max(0.0, ahead - std::sqrt(radius * radius - lateral * lateral));
                desired =
                    std::min(desired, std::max(0.0, std::sqrt(config_.a_brake * config_.a_brake *
                                                                  reaction * reaction +
                                                              2 * config_.a_brake * free) -
                                                        config_.a_brake * reaction));
            }
        }
        proposal.v_cmd =
            std::min(desired, std::max(0.0, source->v_meas) + config_.a_max / config_.obs_rate_hz);
    }
    if (!valid(proposal))
        return false;
    retry_nominal_ = false;
    ++sequence_;
    last_proposal_ = now;
    previous_ = output = proposal;
    return true;
}
} // namespace guard
