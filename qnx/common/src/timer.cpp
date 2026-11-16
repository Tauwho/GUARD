#include "guard/qnx/timer.hpp"

#include "guard/qnx/sched.hpp"

#include <errno.h>
#include <signal.h>
#include <sys/neutrino.h>

namespace guard::qnx {

namespace {
struct timespec to_timespec(std::uint64_t ns) {
    struct timespec ts {};
    ts.tv_sec = static_cast<time_t>(ns / 1000000000u);
    ts.tv_nsec = static_cast<long>(ns % 1000000000u);
    return ts;
}
} // namespace

PulseTimer::~PulseTimer() {
    if (created_) {
        timer_delete(id_);
    }
}

int PulseTimer::create(int coid, int code, int prio) {
    if (created_) {
        return EBUSY;
    }
    if (prio < 1 || prio > kMaxPriority) {
        return EINVAL;
    }
    struct sigevent ev;
    SIGEV_PULSE_INIT(&ev, coid, prio, code, 0);
    if (timer_create(CLOCK_MONOTONIC, &ev, &id_) == -1) {
        return errno;
    }
    created_ = true;
    return EOK;
}

int PulseTimer::arm(std::uint64_t first_ns, std::uint64_t period_ns) {
    if (!created_) {
        return EINVAL;
    }
    struct itimerspec its {};
    its.it_value = to_timespec(first_ns);
    its.it_interval = to_timespec(period_ns);
    return timer_settime(id_, 0, &its, nullptr) == -1 ? errno : EOK;
}

int PulseTimer::arm_periodic(std::uint64_t period_ns) {
    return period_ns == 0 ? EINVAL : arm(period_ns, period_ns);
}

int PulseTimer::arm_oneshot(std::uint64_t delay_ns) {
    return delay_ns == 0 ? EINVAL : arm(delay_ns, 0);
}

int PulseTimer::disarm() { return arm(0, 0); }

} // namespace guard::qnx
