// qnx/common: devctl command numbers from contracts/messages.md section 1.5 (contracts-v1.2).
// The data size is part of each value, so the opaque types below have the wire sizes.
#pragma once

#include <cstdint>
#include <devctl.h>

namespace guard::qnx {
struct guard_register_reply_t {
    std::uint8_t b[24];
};
struct guard_lease_t {
    std::uint8_t b[80];
};
struct guard_revoke_t {
    std::uint8_t b[24];
};
struct guard_recover_t {
    std::uint8_t b[24];
};
struct guard_passthru_t {
    std::uint8_t b[40];
};
struct guard_motor_cmd_t {
    std::uint8_t b[48];
};
struct guard_world_load_t {
    std::uint8_t b[120];
};
struct guard_world_inject_t {
    std::uint8_t b[24];
};
struct guard_world_pause_t {
    std::uint8_t b[16];
};
} // namespace guard::qnx

#define DCMD_GUARD_REGISTER __DIOF(_DCMD_MISC, 0xA0, guard::qnx::guard_register_reply_t)
#define DCMD_GUARD_LEASE __DIOTF(_DCMD_MISC, 0xA1, guard::qnx::guard_lease_t)
#define DCMD_GUARD_REVOKE __DIOT(_DCMD_MISC, 0xA2, guard::qnx::guard_revoke_t)
#define DCMD_GUARD_RECOVER __DIOTF(_DCMD_MISC, 0xA3, guard::qnx::guard_recover_t)
#define DCMD_GUARD_PASSTHRU __DIOT(_DCMD_MISC, 0xA4, guard::qnx::guard_passthru_t)
#define DCMD_SIM_MOTOR __DIOT(_DCMD_MISC, 0xB0, guard::qnx::guard_motor_cmd_t)
#define DCMD_SIM_WORLD_LOAD __DIOT(_DCMD_MISC, 0xB1, guard::qnx::guard_world_load_t)
#define DCMD_SIM_WORLD_INJECT __DIOT(_DCMD_MISC, 0xB2, guard::qnx::guard_world_inject_t)
#define DCMD_SIM_WORLD_PAUSE __DIOT(_DCMD_MISC, 0xB3, guard::qnx::guard_world_pause_t)
