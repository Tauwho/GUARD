#include "guard/codec.hpp"
#include "guard/reason_codes.hpp"
#include <algorithm>
#include <cmath>

namespace guard {
namespace {
bool within(double value, double low, double high) {
    return std::isfinite(value) && value >= low && value <= high;
}
bool velocities(double v, double w, double vmax = 1.2, double wmax = 1.0) {
    return within(v, -vmax, vmax) && within(w, -wmax, wmax);
}
bool known(std::uint16_t reason, unsigned low, unsigned high) {
    return reason >= low && reason <= high && reason_name(reason) != "UNKNOWN";
}
} // namespace

bool valid(const Observation &v) {
    constexpr double pi = 3.14159265358979323846;
    if (v.obs_id == 0 || v.seq == 0 || v.stamp_ns < 0 || v.map_version == 0 || v.sensor_ok > 1 ||
        v.feed > 1 || v.obstacle_count > v.obstacles.size() || !within(v.x, -100, 100) ||
        !within(v.y, -100, 100) || !within(v.theta, -pi, pi) ||
        !velocities(v.v_meas, v.w_meas, 1.5, 1.5)) {
        return false;
    }
    for (std::size_t i = 0; i < v.obstacles.size(); ++i) {
        const auto &obstacle = v.obstacles[i];
        if (i < v.obstacle_count) {
            if (!within(obstacle.x, -100, 100) || !within(obstacle.y, -100, 100) ||
                !within(obstacle.r, 0.05, 2.0)) {
                return false;
            }
        } else if (obstacle.x != 0 || obstacle.y != 0 || obstacle.r != 0) {
            return false;
        }
    }
    return true;
}
bool valid(const Proposal &v) {
    return v.proposal_id > 0 && v.planner_id > 0 && v.seq > 0 && v.lease_req_ms >= 50 &&
           v.lease_req_ms <= 1000 && v.based_on.obs_id > 0 && v.based_on.obs_seq > 0 &&
           v.based_on.obs_stamp_ns >= 0 && v.based_on.map_version > 0 &&
           v.created_ns >= v.based_on.obs_stamp_ns && velocities(v.v_cmd, v.w_cmd, 1.5, 1.5);
}
bool valid(const Decision &v) {
    if (v.verdict < 1 || v.verdict > 3 || v.decided_ns < 0 || v.expires_ns < 0 ||
        (v.reasons_mask >> 11) != 0) {
        return false;
    }
    if (v.verdict == 1) {
        return v.primary_reason == 100 && v.lease_id > 0 && v.epoch > 0 && v.reasons_mask == 0;
    }
    return known(v.primary_reason, 200, 399) && v.lease_id == 0 && v.expires_ns == 0;
}
bool valid(const RegisterReply &v) { return v.epoch > 0 && v.gate_mode <= 5 && v.reason == 0; }
bool valid(const Lease &v) {
    return v.lease_id > 0 && v.epoch > 0 && v.lease_seq > 0 && v.proposal_id > 0 &&
           velocities(v.v_cmd, v.w_cmd) && within(v.v_cap, 0, 1.2) && v.issued_ns >= 0 &&
           v.expires_ns >= v.issued_ns && v.expires_ns - v.issued_ns >= 50'000'000 &&
           v.expires_ns - v.issued_ns <= 1'000'000'000;
}
bool valid(const LeaseAck &v) {
    return v.lease_id > 0 && v.epoch > 0 && v.gate_mode <= 5 && known(v.reason, 500, 504);
}
bool valid(const Revoke &v) { return v.epoch > 0 && known(v.reason, 400, 499); }
bool valid(const PassthruCmd &v) {
    return v.cmd_seq > 0 && v.stamp_ns >= 0 && velocities(v.v, v.w);
}
bool valid(const RecoverRequest &v) { return v.request_id > 0; }
bool valid(const RecoverReply &v) {
    return v.request_id > 0 && v.gate_mode <= 5 && (v.unmet_mask >> 5) == 0 &&
           (v.reason == 512 || v.reason == 513 || v.reason == 504);
}
bool valid(const MotorCmd &v) {
    return v.cmd_seq > 0 && v.stamp_ns >= 0 && v.gate_mode <= 5 && velocities(v.v, v.w);
}
bool valid(const WorldLoad &v) {
    return v.mode <= 2 && v.ablation_mask <= 15 && (v.mode == 2 || v.ablation_mask == 0) &&
           v.scenario_path.front() == '/' &&
           std::find(v.scenario_path.begin(), v.scenario_path.end(), '\0') != v.scenario_path.end();
}
bool valid(const WorldInject &v) {
    return (v.fault == 1 || v.fault == 4 || v.fault == 5 || v.fault == 6 || v.fault == 8) &&
           v.duration_ms <= 60'000;
}
bool valid(const WorldPause &v) { return v.pause <= 1; }
} // namespace guard
