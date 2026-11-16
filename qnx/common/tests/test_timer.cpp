#include "guard/qnx/ipc.hpp"
#include "guard/qnx/timer.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <errno.h>
#include <sys/neutrino.h>

using namespace guard::qnx;

namespace {
constexpr std::uint64_t kMs = 1000000u;
constexpr int kCode = _PULSE_CODE_MINAVAIL + 1;

// Waits up to `limit_ms` for one timer pulse. Returns true if one arrived.
bool wait_pulse(int chid, std::uint64_t limit_ms) {
    struct _pulse pulse {};
    std::uint64_t ns = limit_ms * kMs;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, nullptr, &ns, nullptr);
    return MsgReceivePulse(chid, &pulse, sizeof pulse, nullptr) == 0 && pulse.code == kCode;
}

struct Channel {
    int chid = ChannelCreate(_NTO_CHF_PRIVATE);
    int coid = -1;
    Channel() { connect_self(chid, coid); }
    ~Channel() {
        ConnectDetach(coid);
        ChannelDestroy(chid);
    }
};
} // namespace

TEST_CASE("PulseTimer validates its arguments") {
    Channel ch;
    PulseTimer timer;
    CHECK(timer.arm_periodic(10 * kMs) == EINVAL); // not created
    CHECK(timer.create(ch.coid, kCode, 0) == EINVAL);
    CHECK(timer.create(ch.coid, kCode, 64) == EINVAL);
    REQUIRE(timer.create(ch.coid, kCode, 35) == EOK);
    CHECK(timer.create(ch.coid, kCode, 35) == EBUSY);
    CHECK(timer.arm_periodic(0) == EINVAL);
    CHECK(timer.arm_oneshot(0) == EINVAL);
}

TEST_CASE("periodic PulseTimer delivers repeated pulses until disarmed") {
    Channel ch;
    PulseTimer timer;
    REQUIRE(timer.create(ch.coid, kCode, 35) == EOK);
    REQUIRE(timer.arm_periodic(10 * kMs) == EOK);
    int got = 0;
    for (int i = 0; i < 5; ++i) {
        got += wait_pulse(ch.chid, 1000) ? 1 : 0;
    }
    CHECK(got == 5);
    REQUIRE(timer.disarm() == EOK);
    while (wait_pulse(ch.chid, 30)) {
        // drain pulses already queued before the disarm
    }
    CHECK_FALSE(wait_pulse(ch.chid, 100));
}

TEST_CASE("one-shot PulseTimer fires once; re-arming replaces the deadline") {
    Channel ch;
    PulseTimer timer;
    REQUIRE(timer.create(ch.coid, kCode, 35) == EOK);
    REQUIRE(timer.arm_oneshot(20 * kMs) == EOK);
    CHECK(wait_pulse(ch.chid, 1000));
    CHECK_FALSE(wait_pulse(ch.chid, 100));
    REQUIRE(timer.arm_oneshot(500 * kMs) == EOK);
    REQUIRE(timer.arm_oneshot(20 * kMs) == EOK); // replaces the 500 ms deadline
    CHECK(wait_pulse(ch.chid, 200));
    CHECK_FALSE(wait_pulse(ch.chid, 600));
}
