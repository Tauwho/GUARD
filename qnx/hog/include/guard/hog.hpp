// qnx/hog (B-10, #44): CPU hog for fault F2 (docs/architecture.md: hog at priority 32).
// Test image only. No GUARD decisions.
#pragma once

#include <cstdint>

namespace guard::hog {

struct Options {
    int prio = 32;                 // SCHED_FIFO 1..63
    std::uint32_t duration_ms = 0; // > 0
    int cpu = -1;                  // pin to this CPU index, or -1 for no pinning
};

// Parses `guard-hog <prio> <ms> [-c <cpu>]`. Returns EOK or EINVAL.
int parse(int argc, char **argv, Options &opt);

// Pins (if asked), switches the calling thread to SCHED_FIFO `prio`, and spins until
// `duration_ms` of CLOCK_MONOTONIC time has passed. Returns EOK or errno.
int run(const Options &opt);

} // namespace guard::hog
