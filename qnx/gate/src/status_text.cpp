#include "status_text.hpp"

#include "guard/reason_codes.hpp"

#include <cstdio>

namespace guard::gate {

const char *mode_name(GateMode mode) {
    static const char *names[] = {"INIT", "LATCHED", "READY", "MOVING", "STOPPING", "STOPPED"};
    auto i = static_cast<unsigned>(mode);
    return i < 6 ? names[i] : "INIT";
}

const char *latch_name(LatchCause cause) {
    static const char *names[] = {"NONE",        "BOOT",        "SUPERVISOR_LOST", "EPOCH_CHANGED",
                                  "ZONE_BREACH", "SENSOR_LOST", "MOTOR_TIMEOUT",   "INTERNAL"};
    auto i = static_cast<unsigned>(cause);
    return i < 8 ? names[i] : "INTERNAL";
}

std::size_t format_status(const GateStatus &s, const MotorCmd &motor, std::uint64_t ticks,
                          std::uint64_t late_ticks, std::uint64_t extra_unauthorized, char *out,
                          std::size_t size) {
    std::size_t n = 0;
    auto put = [&](const char *fmt, auto... args) {
        if (n < size) {
            int w = std::snprintf(out + n, size - n, fmt, args...);
            n += w > 0 ? static_cast<std::size_t>(w) : 0;
            n = n < size ? n : size - 1;
        }
    };
    put("version=1\nmode=%s\nepoch=%u\nlease_id=%llu\nlease_seq=%llu\n", mode_name(s.mode), s.epoch,
        static_cast<unsigned long long>(s.lease_id), static_cast<unsigned long long>(s.lease_seq));
    put("lease_remaining_ns=%lld\nlatch_cause=%s\nrecovery_unmet_mask=0x%08x\n",
        static_cast<long long>(s.lease_remaining_ns), latch_name(s.latch), s.recovery_unmet_mask);
    put("v_cmd=%.3f\nw_cmd=%.3f\nticks=%llu\nlate_ticks=%llu\n", motor.v, motor.w,
        static_cast<unsigned long long>(ticks), static_cast<unsigned long long>(late_ticks));
    for (unsigned code = 500; code < 600; ++code) {
        auto count = s.counters[code];
        if (code == static_cast<unsigned>(Reason::GATE_UNAUTHORIZED_CLIENT))
            count += extra_unauthorized;
        if (count != 0) {
            auto name = reason_name(static_cast<std::uint16_t>(code));
            put("count.%.*s=%llu\n", static_cast<int>(name.size()), name.data(),
                static_cast<unsigned long long>(count));
        }
    }
    return n;
}

} // namespace guard::gate
