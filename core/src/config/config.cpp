#include "guard/config.hpp"
#include <cmath>

namespace guard {
bool valid(const Config &c) {
    auto bounded = [](double value, double low, double high) {
        return std::isfinite(value) && value >= low && value <= high;
    };
    auto positive = [&](double value, double high) { return value > 0 && bounded(value, 0, high); };
    return static_cast<unsigned>(c.mode) <= 2 &&
           (c.mode == Mode::guarded ||
            !(c.ablate_freshness || c.ablate_stopping || c.ablate_expiry || c.ablate_epoch)) &&
           positive(c.v_max, 1.5) && positive(c.w_max, 1.5) && positive(c.a_max, 3) &&
           positive(c.a_brake, 5) && bounded(c.margin_m, 0, 2) && positive(c.r_robot_m, 2) &&
           bounded(c.standstill_v, 0, 0.5) && bounded(c.sweep_step_m, 0.01, 0.5) &&
           c.t_lease_ms >= 50 && c.t_lease_ms <= 1000 && c.t_tick_ms >= 1 && c.t_tick_ms <= 100 &&
           c.t_act_ms <= 200 && c.obs_rate_hz >= 1 && c.obs_rate_hz <= 100 &&
           c.max_obs_age_ms >= 10 && c.max_obs_age_ms <= 5000 && c.supersession_k <= 63 &&
           c.sensor_stale_ms >= 10 && c.sensor_stale_ms <= 5000 && c.standstill_ticks >= 1 &&
           c.standstill_ticks <= 100 && c.actuator_watchdog_ms >= 10 &&
           c.actuator_watchdog_ms <= 1000 && c.rate_limit_hz >= 1 && c.rate_limit_hz <= 1000 &&
           c.obs_history >= 8 && c.obs_history <= 64 && c.unverifiable_revoke_count >= 1 &&
           c.unverifiable_revoke_count <= 10 && c.heartbeat_timeout_ms >= 50 &&
           c.heartbeat_timeout_ms <= 5000 && c.route_max_age_ms >= 100 &&
           c.route_max_age_ms <= 60000;
}
} // namespace guard
