// qnx/common: timers that deliver pulses (B-3, #37). Adapter only, no GUARD decisions.
#pragma once

#include <cstdint>
#include <time.h>

namespace guard::qnx {

// timer_create(CLOCK_MONOTONIC) with SIGEV_PULSE_INIT. Not copyable; deletes on destruction.
class PulseTimer {
  public:
    PulseTimer() = default;
    PulseTimer(const PulseTimer &) = delete;
    PulseTimer &operator=(const PulseTimer &) = delete;
    ~PulseTimer();

    // Pulses go to `coid` with `code` and `prio` (1..63). Returns EOK or errno.
    int create(int coid, int code, int prio);
    // Fires every `period_ns`, first after one period. Returns EOK or errno.
    int arm_periodic(std::uint64_t period_ns);
    // Fires once after `delay_ns`. Re-arming replaces the previous deadline.
    int arm_oneshot(std::uint64_t delay_ns);
    int disarm();

  private:
    int arm(std::uint64_t first_ns, std::uint64_t period_ns);
    timer_t id_{};
    bool created_ = false;
};

} // namespace guard::qnx
