#include "guard/supervisor.hpp"
#include <algorithm>
#include <cmath>

namespace guard {
StoppingCheck stopping_check(const Observation &observation, const Proposal &proposal,
                             const Scene &scene, const Config &config) {
    StoppingCheck result;
    if (!valid(config) || !valid(observation) || !valid(proposal) || !valid(scene)) {
        result.budget_exceeded = true;
        return result;
    }
    const double reaction =
        (std::min(proposal.lease_req_ms, config.t_lease_ms) + config.t_tick_ms + config.t_act_ms) /
        1000.0;
    const double measured = std::abs(observation.v_meas);
    const double peak =
        std::max(measured, std::min(proposal.v_cmd, measured + config.a_max * reaction));
    result.required = peak * reaction + peak * peak / (2 * config.a_brake);
    const double radius = config.r_robot_m + config.margin_m;
    const double samples = std::ceil(result.required / config.sweep_step_m) + 1;
    if (!std::isfinite(samples) || samples > 4096) {
        result.budget_exceeded = true;
        return result;
    }
    Point previous{observation.x, observation.y};
    if (swept_ground_truth(previous, previous, radius, scene).clearance <= 0) {
        result.available = -1;
        return result;
    }
    const double curvature = proposal.v_cmd > 0 ? proposal.w_cmd / proposal.v_cmd : 0;
    for (std::size_t sample = 1; sample <= static_cast<std::size_t>(samples); ++sample) {
        const double distance = sample * config.sweep_step_m;
        Point next;
        double sagitta = 0;
        if (std::abs(curvature) < 1e-10) {
            next = {observation.x + distance * std::cos(observation.theta),
                    observation.y + distance * std::sin(observation.theta)};
        } else {
            const double turn = distance * curvature;
            next = {
                observation.x +
                    (std::sin(observation.theta + turn) - std::sin(observation.theta)) / curvature,
                observation.y -
                    (std::cos(observation.theta + turn) - std::cos(observation.theta)) / curvature};
            sagitta = std::abs(1 / curvature) * (1 - std::cos(config.sweep_step_m * curvature / 2));
        }
        if (swept_ground_truth(previous, next, radius + sagitta, scene).clearance <= 0)
            return result;
        result.available = distance;
        previous = next;
    }
    return result;
}
} // namespace guard
