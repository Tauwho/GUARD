#include "guard/hog.hpp"

#include "guard/qnx/sched.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/neutrino.h>
#include <time.h>

namespace guard::hog {

namespace {
bool to_int(const char *s, long lo, long hi, long &out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    long v = std::strtol(s, &end, 10);
    if (errno != 0 || *end != '\0' || v < lo || v > hi) {
        return false;
    }
    out = v;
    return true;
}

std::uint64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000u +
           static_cast<std::uint64_t>(ts.tv_nsec);
}
} // namespace

int parse(int argc, char **argv, Options &opt) {
    long prio = 0, ms = 0, cpu = -1;
    if (argc != 3 && argc != 5) {
        return EINVAL;
    }
    if (!to_int(argv[1], 1, qnx::kMaxPriority, prio) || !to_int(argv[2], 1, 60000, ms)) {
        return EINVAL;
    }
    if (argc == 5 && (std::strcmp(argv[3], "-c") != 0 || !to_int(argv[4], 0, 31, cpu))) {
        return EINVAL;
    }
    opt.prio = static_cast<int>(prio);
    opt.duration_ms = static_cast<std::uint32_t>(ms);
    opt.cpu = static_cast<int>(cpu);
    return EOK;
}

int run(const Options &opt) {
    if (opt.duration_ms == 0) {
        return EINVAL;
    }
    if (opt.cpu >= 0) {
        unsigned mask = 1u << opt.cpu;
        if (ThreadCtl(_NTO_TCTL_RUNMASK, reinterpret_cast<void *>(mask)) == -1) {
            return errno;
        }
    }
    int rc = qnx::set_fifo(opt.prio);
    if (rc != EOK) {
        return rc;
    }
    const std::uint64_t end = now_ns() + std::uint64_t{opt.duration_ms} * 1000000u;
    volatile std::uint64_t spins = 0;
    while (now_ns() < end) {
        spins = spins + 1;
    }
    return EOK;
}

} // namespace guard::hog
