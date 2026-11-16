#pragma once
#include <cstdint>

namespace guard {
enum class Mode : std::uint8_t { passthrough, heartbeat, guarded };
struct Config {
    Mode mode = Mode::guarded;
    bool ablate_freshness = false;
    bool ablate_stopping = false;
    bool ablate_expiry = false;
    bool ablate_epoch = false;
    double v_max = 1.2;
    double w_max = 1.0;
    double a_max = 0.5;
    double a_brake = 1.0;
    double margin_m = 0.20;
    double r_robot_m = 0.35;
    double standstill_v = 0.02;
    double sweep_step_m = 0.05;
    std::uint32_t t_lease_ms = 250;
    std::uint32_t t_tick_ms = 10;
    std::uint32_t t_act_ms = 20;
    std::uint32_t obs_rate_hz = 20;
    std::uint32_t max_obs_age_ms = 200;
    std::uint32_t supersession_k = 2;
    std::uint32_t sensor_stale_ms = 150;
    std::uint32_t standstill_ticks = 5;
    std::uint32_t actuator_watchdog_ms = 50;
    std::uint32_t rate_limit_hz = 50;
    std::uint32_t obs_history = 64;
    std::uint32_t unverifiable_revoke_count = 3;
    std::uint32_t heartbeat_timeout_ms = 500;
    std::uint32_t route_max_age_ms = 5000;
};
constexpr std::int64_t milliseconds(std::uint32_t value) {
    return static_cast<std::int64_t>(value) * 1'000'000;
}
bool valid(const Config &config);
} // namespace guard
