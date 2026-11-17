// qnx/common: reads the guard-sim world status (`read()` on /dev/sim/world, C7) for the
// supervisor and the planner (B3, #40, #41). Adapter only, no GUARD decisions.
#pragma once

#include "guard/qnx/kv.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <unistd.h>

namespace guard::qnx {

struct WorldStatus {
    char state[16] = "";
    char scenario_path[100] = "";
    char map_path[160] = "";
    char config_path[160] = "";
    char mode[4] = "";
    char faults[256] = "";
    std::uint64_t seed = 0;
    std::uint32_t map_version = 0, zones_active = 0, ablation_mask = 0;
    bool loaded() const { return std::strcmp(state, "IDLE") != 0 && state[0] != '\0'; }
    bool running() const { return std::strcmp(state, "RUNNING") == 0; }
};

// One snapshot of /dev/sim/world, the read bounded by 5 ms. Returns EOK, the errno of\n// open/read
// (ETIMEDOUT if the sim does not answer), or EBADMSG.
inline int read_world(WorldStatus &out) {
    char text[2048];
    int fd = open("/dev/sim/world", O_RDONLY);
    if (fd < 0)
        return errno;
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = 5'000'000;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    ssize_t n = read(fd, text, sizeof text);
    int e = errno;
    close(fd);
    if (n <= 0)
        return n < 0 ? e : EBADMSG;
    auto len = static_cast<std::size_t>(n);
    char num[32];
    WorldStatus s;
    if (!kv_get(text, len, "state", s.state, sizeof s.state) ||
        !kv_get(text, len, "scenario_path", s.scenario_path, sizeof s.scenario_path) ||
        !kv_get(text, len, "map_path", s.map_path, sizeof s.map_path) ||
        !kv_get(text, len, "config_path", s.config_path, sizeof s.config_path) ||
        !kv_get(text, len, "mode", s.mode, sizeof s.mode) ||
        !kv_get(text, len, "faults", s.faults, sizeof s.faults))
        return EBADMSG;
    if (kv_get(text, len, "seed", num, sizeof num))
        s.seed = std::strtoull(num, nullptr, 10);
    if (kv_get(text, len, "map_version", num, sizeof num))
        s.map_version = static_cast<std::uint32_t>(std::strtoul(num, nullptr, 10));
    if (kv_get(text, len, "ablation_mask", num, sizeof num))
        s.ablation_mask = static_cast<std::uint32_t>(std::strtoul(num, nullptr, 10));
    if (kv_get(text, len, "zones_active", num, sizeof num))
        s.zones_active = static_cast<std::uint32_t>(std::strtoul(num, nullptr, 10));
    out = s;
    return EOK;
}

// If fault F<n> is listed as active ("F1:600,F5:1200"), sets its parameter and returns true.
inline bool world_fault(const WorldStatus &s, unsigned n, std::int64_t &param) {
    char key[8];
    std::snprintf(key, sizeof key, "F%u:", n);
    const char *at = std::strstr(s.faults, key);
    if (at == nullptr || (at != s.faults && at[-1] != ','))
        return false;
    param = std::strtoll(at + std::strlen(key), nullptr, 10);
    return true;
}

} // namespace guard::qnx
