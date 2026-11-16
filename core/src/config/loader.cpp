#include "guard/buffer_json.hpp"
#include "guard/config.hpp"
#include <limits>

namespace guard {
bool load_config(std::string_view buffer, Config &output) {
    BufferJson json(buffer);
    if (!json.valid() || json.node(0).kind != BufferJson::Kind::object)
        return false;
    Config candidate;
    std::uint64_t version = 0;
    if (!json.unsigned_integer(json.member(0, "version"), version) || version != 1)
        return false;
    const auto mode = json.text(json.member(0, "mode"));
    if (mode == "B0")
        candidate.mode = Mode::passthrough;
    else if (mode == "B1")
        candidate.mode = Mode::heartbeat;
    else if (mode != "B2")
        return false;
    for (std::size_t key = 1; key < json.node(0).next; key = json.node(key + 1).next)
        if (json.text(key) != "version" && json.text(key) != "mode" && json.text(key) != "params" &&
            json.text(key) != "ablate" && json.text(key) != "limits")
            return false;
    struct DoubleParameter {
        std::string_view name;
        double Config::*field;
    };
    const DoubleParameter doubles[] = {{"v_max", &Config::v_max},
                                       {"w_max", &Config::w_max},
                                       {"a_max", &Config::a_max},
                                       {"a_brake", &Config::a_brake},
                                       {"margin_m", &Config::margin_m},
                                       {"r_robot_m", &Config::r_robot_m},
                                       {"standstill_v", &Config::standstill_v},
                                       {"sweep_step_m", &Config::sweep_step_m}};
    struct IntegerParameter {
        std::string_view name;
        std::uint32_t Config::*field;
    };
    const IntegerParameter integers[] = {
        {"t_lease_ms", &Config::t_lease_ms},
        {"t_tick_ms", &Config::t_tick_ms},
        {"t_act_ms", &Config::t_act_ms},
        {"obs_rate_hz", &Config::obs_rate_hz},
        {"max_obs_age_ms", &Config::max_obs_age_ms},
        {"supersession_k", &Config::supersession_k},
        {"sensor_stale_ms", &Config::sensor_stale_ms},
        {"standstill_ticks", &Config::standstill_ticks},
        {"actuator_watchdog_ms", &Config::actuator_watchdog_ms},
        {"rate_limit_hz", &Config::rate_limit_hz},
        {"obs_history", &Config::obs_history},
        {"unverifiable_revoke_count", &Config::unverifiable_revoke_count},
        {"heartbeat_timeout_ms", &Config::heartbeat_timeout_ms},
        {"route_max_age_ms", &Config::route_max_age_ms}};
    const auto parameters = json.member(0, "params");
    if (parameters != BufferJson::missing) {
        if (json.node(parameters).kind != BufferJson::Kind::object)
            return false;
        for (std::size_t key = parameters + 1; key < json.node(parameters).next;
             key = json.node(key + 1).next) {
            bool found = false;
            for (const auto &parameter : doubles)
                if (json.text(key) == parameter.name) {
                    if (!json.number(key + 1, candidate.*(parameter.field)))
                        return false;
                    found = true;
                }
            for (const auto &parameter : integers)
                if (json.text(key) == parameter.name) {
                    std::uint64_t value;
                    if (!json.unsigned_integer(key + 1, value) ||
                        value > std::numeric_limits<std::uint32_t>::max())
                        return false;
                    candidate.*(parameter.field) = static_cast<std::uint32_t>(value);
                    found = true;
                }
            if (!found)
                return false;
        }
    }
    const auto ablations = json.member(0, "ablate");
    if (ablations != BufferJson::missing) {
        if (candidate.mode != Mode::guarded ||
            json.node(ablations).kind != BufferJson::Kind::object)
            return false;
        for (std::size_t key = ablations + 1; key < json.node(ablations).next;
             key = json.node(key + 1).next) {
            bool value;
            if (!json.boolean(key + 1, value))
                return false;
            if (json.text(key) == "freshness")
                candidate.ablate_freshness = value;
            else if (json.text(key) == "stopping")
                candidate.ablate_stopping = value;
            else if (json.text(key) == "expiry")
                candidate.ablate_expiry = value;
            else if (json.text(key) == "epoch")
                candidate.ablate_epoch = value;
            else
                return false;
        }
    }
    const auto limits = json.member(0, "limits");
    if (limits != BufferJson::missing) {
        if (json.node(limits).kind != BufferJson::Kind::object)
            return false;
        for (std::size_t key = limits + 1; key < json.node(limits).next;
             key = json.node(key + 1).next) {
            std::uint64_t value;
            if (!json.unsigned_integer(key + 1, value))
                return false;
            if ((json.text(key) == "max_zones" && value == 32) ||
                (json.text(key) == "max_obstacles" && value == 16))
                continue;
            return false;
        }
    }
    if (!valid(candidate))
        return false;
    output = candidate;
    return true;
}
} // namespace guard
