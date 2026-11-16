#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace guard {

struct Header {
    std::uint16_t magic = 0x4447;
    std::uint16_t type = 0;
    std::uint16_t version = 1;
    std::uint16_t size = 8;
    auto fields() { return std::tie(magic, type, version, size); }
};

struct Obstacle {
    double x = 0, y = 0, r = 0;
    auto fields() { return std::tie(x, y, r); }
};

struct Observation {
    Header header{0x4447, 0x0001, 1, 464};
    std::uint64_t obs_id = 0, seq = 0;
    std::int64_t stamp_ns = 0;
    double x = 0, y = 0, theta = 0, v_meas = 0, w_meas = 0;
    std::uint32_t map_version = 0;
    std::uint8_t sensor_ok = 0, obstacle_count = 0, feed = 0, reserved0 = 0;
    std::array<Obstacle, 16> obstacles{};
    auto fields() {
        return std::tie(header, obs_id, seq, stamp_ns, x, y, theta, v_meas, w_meas, map_version,
                        sensor_ok, obstacle_count, feed, reserved0, obstacles);
    }
};

struct Provenance {
    std::uint64_t obs_id = 0, obs_seq = 0;
    std::int64_t obs_stamp_ns = 0;
    std::uint32_t map_version = 0, reserved0 = 0;
    auto fields() { return std::tie(obs_id, obs_seq, obs_stamp_ns, map_version, reserved0); }
};

struct Proposal {
    Header header{0x4447, 0x0002, 1, 88};
    std::uint64_t proposal_id = 0;
    std::uint32_t planner_id = 0, lease_req_ms = 250;
    std::uint64_t seq = 0;
    Provenance based_on{};
    std::int64_t created_ns = 0;
    double v_cmd = 0, w_cmd = 0;
    auto fields() {
        return std::tie(header, proposal_id, planner_id, lease_req_ms, seq, based_on, created_ns,
                        v_cmd, w_cmd);
    }
};

struct Decision {
    Header header{0x4447, 0x0003, 1, 56};
    std::uint64_t proposal_id = 0;
    std::uint8_t verdict = 2, reserved0 = 0;
    std::uint16_t primary_reason = 200;
    std::uint32_t epoch = 0;
    std::uint64_t reasons_mask = 0, lease_id = 0;
    std::int64_t expires_ns = 0, decided_ns = 0;
    auto fields() {
        return std::tie(header, proposal_id, verdict, reserved0, primary_reason, epoch,
                        reasons_mask, lease_id, expires_ns, decided_ns);
    }
};

struct RegisterReply {
    Header header{0x4447, 0x0010, 1, 24};
    std::uint32_t epoch = 0;
    std::uint16_t gate_mode = 0, reason = 0;
    std::uint64_t last_lease_seq = 0;
    auto fields() { return std::tie(header, epoch, gate_mode, reason, last_lease_seq); }
};

struct Lease {
    Header header{0x4447, 0x0011, 1, 80};
    std::uint64_t lease_id = 0;
    std::uint32_t epoch = 0, reserved0 = 0;
    std::uint64_t lease_seq = 0, proposal_id = 0;
    double v_cmd = 0, w_cmd = 0, v_cap = 0;
    std::int64_t issued_ns = 0, expires_ns = 0;
    auto fields() {
        return std::tie(header, lease_id, epoch, reserved0, lease_seq, proposal_id, v_cmd, w_cmd,
                        v_cap, issued_ns, expires_ns);
    }
};

struct LeaseAck {
    Header header{0x4447, 0x0012, 1, 24};
    std::uint64_t lease_id = 0;
    std::uint16_t reason = 0, gate_mode = 0;
    std::uint32_t epoch = 0;
    auto fields() { return std::tie(header, lease_id, reason, gate_mode, epoch); }
};

struct Revoke {
    Header header{0x4447, 0x0013, 1, 24};
    std::uint64_t lease_id = 0;
    std::uint32_t epoch = 0;
    std::uint16_t reason = 400, reserved0 = 0;
    auto fields() { return std::tie(header, lease_id, epoch, reason, reserved0); }
};

struct PassthruCmd {
    Header header{0x4447, 0x0014, 1, 40};
    std::uint64_t cmd_seq = 0;
    double v = 0, w = 0;
    std::int64_t stamp_ns = 0;
    auto fields() { return std::tie(header, cmd_seq, v, w, stamp_ns); }
};

struct RecoverRequest {
    Header header{0x4447, 0x0020, 1, 16};
    std::uint64_t request_id = 0;
    auto fields() { return std::tie(header, request_id); }
};

struct RecoverReply {
    Header header{0x4447, 0x0021, 1, 24};
    std::uint64_t request_id = 0;
    std::uint16_t reason = 512, gate_mode = 0;
    std::uint32_t unmet_mask = 0;
    auto fields() { return std::tie(header, request_id, reason, gate_mode, unmet_mask); }
};

struct MotorCmd {
    Header header{0x4447, 0x0030, 1, 48};
    std::uint64_t cmd_seq = 0;
    double v = 0, w = 0;
    std::int64_t stamp_ns = 0;
    std::uint16_t gate_mode = 0, reserved0 = 0;
    std::uint32_t reserved1 = 0;
    auto fields() {
        return std::tie(header, cmd_seq, v, w, stamp_ns, gate_mode, reserved0, reserved1);
    }
};

struct WorldLoad {
    Header header{0x4447, 0x0040, 1, 120};
    std::uint64_t seed = 0;
    std::uint8_t mode = 2, ablation_mask = 0;
    std::uint16_t reserved0 = 0;
    std::uint32_t reserved1 = 0;
    std::array<char, 96> scenario_path{};
    auto fields() {
        return std::tie(header, seed, mode, ablation_mask, reserved0, reserved1, scenario_path);
    }
};

struct WorldInject {
    Header header{0x4447, 0x0041, 1, 24};
    std::uint8_t fault = 1;
    std::array<std::uint8_t, 3> reserved0{};
    std::uint32_t duration_ms = 0;
    std::int64_t param = 0;
    auto fields() { return std::tie(header, fault, reserved0, duration_ms, param); }
};

struct WorldPause {
    Header header{0x4447, 0x0042, 1, 16};
    std::uint8_t pause = 0;
    std::array<std::uint8_t, 7> reserved0{};
    auto fields() { return std::tie(header, pause, reserved0); }
};

static_assert(sizeof(Header) == 8);
static_assert(sizeof(Obstacle) == 24);
static_assert(sizeof(Observation) == 464);
static_assert(sizeof(Provenance) == 32);
static_assert(sizeof(Proposal) == 88);
static_assert(sizeof(Decision) == 56);
static_assert(sizeof(RegisterReply) == 24);
static_assert(sizeof(Lease) == 80);
static_assert(sizeof(LeaseAck) == 24);
static_assert(sizeof(Revoke) == 24);
static_assert(sizeof(PassthruCmd) == 40);
static_assert(sizeof(RecoverRequest) == 16);
static_assert(sizeof(RecoverReply) == 24);
static_assert(sizeof(MotorCmd) == 48);
static_assert(sizeof(WorldLoad) == 120);
static_assert(sizeof(WorldInject) == 24);
static_assert(sizeof(WorldPause) == 16);
static_assert(offsetof(Proposal, v_cmd) == 72);
static_assert(offsetof(Observation, obstacles) == 80);
static_assert(std::is_standard_layout_v<Observation> && std::is_trivially_copyable_v<Observation>);
static_assert(std::is_standard_layout_v<Proposal> && std::is_trivially_copyable_v<Proposal>);

} // namespace guard
