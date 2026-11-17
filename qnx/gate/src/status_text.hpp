// C4 /dev/guard/status text (contracts/messages.md section 5).
#pragma once

#include "guard/gate_fsm.hpp"

#include <cstddef>
#include <cstdint>

namespace guard::gate {

const char *mode_name(GateMode mode);
const char *latch_name(LatchCause cause);

// Writes the key=value snapshot into out (at most 2048 bytes). `extra_unauthorized` adds the
// adapter's own GATE_UNAUTHORIZED_CLIENT count (uid denials happen before the core sees a
// call). Returns the length written.
std::size_t format_status(const GateStatus &status, const MotorCmd &motor, std::uint64_t ticks,
                          std::uint64_t late_ticks, std::uint64_t extra_unauthorized, char *out,
                          std::size_t size);

} // namespace guard::gate
