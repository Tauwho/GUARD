// guard_planner_check (B3, #41): on-target check of guard-planner with the image's
// guard-sim, guard-gate and guard-supervisor. Run as root:
//   /tmp/guard_planner_check /tmp/guard-planner /system/guard/scenarios/F0.yaml
// Plays operator (C7 load/start/inject, C3 recover) and reads the supervisor's telemetry
// ring. Prints CHECK lines (PASS/FAIL) and counts only, no timing values.
#include "guard/codec.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/kv.hpp"
#include "guard/qnx/tlm.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <devctl.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/neutrino.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {
using namespace guard;
int g_failed = 0;

void sleep_ms(long ms) {
    struct timespec ts {
        ms / 1000, (ms % 1000) * 1'000'000L
    };
    nanosleep(&ts, nullptr);
}
void check(const char *name, bool ok) {
    std::printf("CHECK %-56s %s\n", name, ok ? "PASS" : "FAIL");
    g_failed += ok ? 0 : 1;
}
std::string slurp(const char *path) {
    char buf[4096];
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return {};
    ssize_t n = read(fd, buf, sizeof buf);
    close(fd);
    return n > 0 ? std::string(buf, n) : std::string();
}
std::string world_field(const char *key) {
    auto t = slurp("/dev/sim/world");
    char v[200];
    return qnx::kv_get(t.data(), t.size(), key, v, sizeof v) ? v : "";
}
int bounded(int fd, unsigned cmd, void *data, std::size_t len, std::uint64_t ms) {
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = ms * 1'000'000;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    return devctl(fd, cmd, data, len, nullptr);
}
template <class T> int world(unsigned cmd, const T &v) {
    std::array<std::uint8_t, sizeof(T)> w{};
    encode(v, w);
    int fd = open("/dev/sim/world", O_RDWR);
    int rc = fd < 0 ? errno : bounded(fd, cmd, w.data(), w.size(), 100);
    close(fd);
    return rc;
}
std::uint16_t recover() {
    int fd = open("/dev/guard/gate", O_RDWR);
    RecoverRequest r;
    r.request_id = 7;
    std::array<std::uint8_t, sizeof(RecoverReply)> buf{};
    std::array<std::uint8_t, sizeof(RecoverRequest)> w{};
    encode(r, w);
    std::memcpy(buf.data(), w.data(), w.size());
    RecoverReply reply;
    int rc = bounded(fd, DCMD_GUARD_RECOVER, buf.data(), buf.size(), 5);
    close(fd);
    return rc == EOK && decode(buf.data(), buf.size(), reply) == DecodeError::none ? reply.reason
                                                                                   : 0;
}
struct Counts {
    int decisions = 0, authorized = 0, rate_limited = 0, planner1 = 0;
};
void drain(qnx::TlmReader &r, Counts &c) {
    char line[qnx::kTlmLine];
    std::size_t n = 0;
    while (r.next(line, sizeof line, n)) {
        std::string s(line, n);
        if (s.find("\"kind\":\"decision\"") == std::string::npos)
            continue;
        ++c.decisions;
        c.authorized += s.find("\"verdict\":\"AUTHORIZED\"") != std::string::npos;
        c.rate_limited += s.find("\"primary_reason\":\"REJ_RATE_LIMIT\"") != std::string::npos;
        c.planner1 += s.find("\"planner_id\":1,") != std::string::npos;
    }
}
} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    signal(SIGALRM, [](int) {
        static const char m[] = "RESULT FAIL (check did not finish)\n";
        write(1, m, sizeof m - 1);
        _exit(1);
    });
    alarm(90);
    if (argc != 3) {
        std::printf("usage: guard_planner_check <guard-planner> <scenario>\n");
        return 2;
    }
    check("image runs guard-sim, guard-gate, guard-supervisor",
          access("/dev/sim/world", F_OK) == 0 && access("/dev/guard/gate", F_OK) == 0 &&
              access("/dev/name/local/guard/supervisor", F_OK) == 0);
    WorldPause stop, go;
    stop.pause = 1;
    go.pause = 0;
    world(DCMD_SIM_WORLD_PAUSE, stop);
    WorldLoad l;
    l.seed = 1000;
    l.mode = 2;
    std::strncpy(l.scenario_path.data(), argv[2], l.scenario_path.size() - 1);
    check("C7 load F0", world(DCMD_SIM_WORLD_LOAD, l) == EOK);

    pid_t planner = -1;
    char *pargv[] = {argv[1], nullptr};
    if (posix_spawn(&planner, argv[1], nullptr, nullptr, pargv, environ) != 0) {
        std::printf("spawn guard-planner failed\n");
        return 1;
    }
    qnx::TlmReader sup;
    check("supervisor telemetry ring readable", sup.open("supervisor") == EOK);
    Counts skip;
    drain(sup, skip); // earlier runs
    sleep_ms(300);
    double x0 = std::atof(world_field("x").c_str()), y0 = std::atof(world_field("y").c_str());
    check("C7 start", world(DCMD_SIM_WORLD_PAUSE, go) == EOK);
    sleep_ms(200);
    check("C3 recover -> GATE_RECOVERED", recover() == 512);
    sleep_ms(8000);
    Counts run;
    drain(sup, run);
    double moved = std::hypot(std::atof(world_field("x").c_str()) - x0,
                              std::atof(world_field("y").c_str()) - y0);
    std::printf("decisions %d, authorized %d\n", run.decisions, run.authorized);
    check("planner proposals reach the supervisor (planner_id 1)",
          run.decisions > 50 && run.planner1 == run.decisions);
    check("rule planner on F0: most proposals AUTHORIZED",
          run.authorized * 10 >= run.decisions * 9);
    check("robot moved along the route (> 1 m in 8 s)", moved > 1.0);
    check("no contact, no zone breach",
          world_field("contacts") == "0" && world_field("zone_breaches") == "0");

    WorldInject f8;
    f8.fault = 8;
    f8.duration_ms = 1000;
    f8.param = 1000;
    check("C7 inject F8 (planner flood 1000 Hz)", world(DCMD_SIM_WORLD_INJECT, f8) == EOK);
    sleep_ms(1500);
    Counts flood;
    drain(sup, flood);
    std::printf("during F8: decisions %d, REJ_RATE_LIMIT %d\n", flood.decisions,
                flood.rate_limited);
    check("F8: supervisor refuses the excess with REJ_RATE_LIMIT", flood.rate_limited > 0);
    sleep_ms(1000);
    Counts after;
    drain(sup, after);
    check("after F8: AUTHORIZED again", after.authorized > 0);

    world(DCMD_SIM_WORLD_PAUSE, stop);
    kill(planner, SIGTERM);
    waitpid(planner, nullptr, 0);
    std::printf("RESULT %s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
