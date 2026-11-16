#include "guard/supervisor.hpp"
#include <cmath>

namespace guard {
std::uint16_t Supervisor::recheck(const Lease &lease, const Scene &scene, std::int64_t now) {
    const auto *observation = latest();
    if (!observation || !observation->sensor_ok || now < observation->stamp_ns ||
        now - observation->stamp_ns > milliseconds(config_.sensor_stale_ms)) {
        unverifiable_count_ = 0;
        return static_cast<std::uint16_t>(Reason::REVOKE_SENSOR_HEALTH);
    }
    if (!valid(config_) || !valid(scene) || !valid(lease) || now < last_evaluation_) {
        ++unverifiable_count_;
        return unverifiable_count_ >= config_.unverifiable_revoke_count
                   ? static_cast<std::uint16_t>(Reason::REVOKE_UNVERIFIABLE)
                   : 0;
    }
    if (lease.v_cmd < 0 || lease.v_cmd > config_.v_max || std::abs(lease.w_cmd) > config_.w_max ||
        observation->v_meas < -0.02) {
        unverifiable_count_ = 0;
        return static_cast<std::uint16_t>(Reason::REVOKE_ENVELOPE);
    }
    Proposal proposal;
    proposal.proposal_id = lease.proposal_id;
    proposal.planner_id = 1;
    proposal.seq = 1;
    proposal.based_on = {observation->obs_id, observation->seq, observation->stamp_ns,
                         observation->map_version, 0};
    proposal.created_ns = now;
    proposal.v_cmd = lease.v_cmd;
    proposal.w_cmd = lease.w_cmd;
    // Round up sub-millisecond durations rather than understating the granted horizon.
    proposal.lease_req_ms =
        static_cast<std::uint32_t>((lease.expires_ns - lease.issued_ns + 999'999) / 1'000'000);
    Scene world = scene;
    world.obstacle_count = observation->obstacle_count;
    world.obstacles = observation->obstacles;
    const auto stopping = stopping_check(*observation, proposal, world, config_);
    if (stopping.budget_exceeded) {
        ++unverifiable_count_;
        return unverifiable_count_ >= config_.unverifiable_revoke_count
                   ? static_cast<std::uint16_t>(Reason::REVOKE_UNVERIFIABLE)
                   : 0;
    }
    unverifiable_count_ = 0;
    Scene zones = world;
    zones.wall_count = zones.obstacle_count = 0;
    const auto zone_check = stopping_check(*observation, proposal, zones, config_);
    if (zone_check.available < zone_check.required)
        return static_cast<std::uint16_t>(Reason::REVOKE_RESTRICTED_ZONE);
    if (!config_.ablate_stopping && stopping.available < stopping.required)
        return static_cast<std::uint16_t>(Reason::REVOKE_STOPPING_DISTANCE);
    return 0;
}
} // namespace guard
