// guard_sim_check (B3, #38): on-target check of guard-sim. Run as root on the GUARD image:
//   /tmp/guard_sim_check /tmp/guard-sim /system/guard/scenarios/F0.yaml
// Starts guard-sim, then acts as gate (uid 1102), supervisor (1103), planner (1104) and
// operator (root). Prints CHECK lines (PASS/FAIL) and counts only, no timing values.
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
std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}
void check(const char *name, bool ok) {
    std::printf("CHECK %-52s %s\n", name, ok ? "PASS" : "FAIL");
    g_failed += ok ? 0 : 1;
}
std::string world() {
    char buf[2048];
    int fd = open("/dev/sim/world", O_RDONLY);
    if (fd < 0)
        return {};
    ssize_t n = read(fd, buf, sizeof buf);
    close(fd);
    return n > 0 ? std::string(buf, n) : std::string();
}
std::string field(const char *key) {
    auto t = world();
    char v[200];
    return qnx::kv_get(t.data(), t.size(), key, v, sizeof v) ? v : "";
}
int call(int fd, unsigned cmd, void *data, std::size_t len, std::uint64_t ms) {
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = ms * 1'000'000;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    return devctl(fd, cmd, data, len, nullptr);
}
template <class T> int world_call(unsigned cmd, const T &v) {
    std::array<std::uint8_t, sizeof(T)> w{};
    if (!encode(v, w))
        return -1;
    int fd = open("/dev/sim/world", O_RDWR);
    int rc = fd < 0 ? errno : call(fd, cmd, w.data(), w.size(), 100); // C7 bound
    close(fd);
    return rc;
}
int load(const char *path) {
    WorldLoad l;
    l.seed = 1000;
    l.mode = 2;
    std::strncpy(l.scenario_path.data(), path, l.scenario_path.size() - 1);
    return world_call(DCMD_SIM_WORLD_LOAD, l);
}
int pause_world(int p) {
    WorldPause w;
    w.pause = static_cast<std::uint8_t>(p);
    return world_call(DCMD_SIM_WORLD_PAUSE, w);
}
int inject(unsigned fault, std::uint32_t ms, std::int64_t param) {
    WorldInject i;
    i.fault = static_cast<std::uint8_t>(fault);
    i.duration_ms = ms;
    i.param = param;
    return world_call(DCMD_SIM_WORLD_INJECT, i);
}
int wait_child(pid_t p) {
    int st = 0;
    waitpid(p, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 255;
}
// Child as uid: sends one MotorCmd every 10 ms for `ms` with speed v (exit 0 = all EOK).
pid_t motor_child(uid_t uid, double v, long ms, std::uint64_t first_seq) {
    pid_t p = fork();
    if (p != 0)
        return p;
    seteuid(uid);
    int fd = open("/dev/sim/motor", O_RDWR);
    int bad = fd < 0 ? 1 : 0;
    for (long t = 0; t < ms && !bad; t += 10) {
        MotorCmd c;
        c.cmd_seq = first_seq++;
        c.v = v;
        c.stamp_ns = now_ns();
        c.gate_mode = 3;
        std::array<std::uint8_t, sizeof c> w{};
        encode(c, w);
        int rc = call(fd, DCMD_SIM_MOTOR, w.data(), w.size(), 5); // C5 bound
        bad = rc == EOK ? 0 : rc;
        sleep_ms(10);
    }
    _exit(bad > 255 ? 255 : bad);
}
// Child as uid: reads n observations from a feed into a pipe; exit code = errno of a failure.
pid_t feed_child(const char *path, uid_t uid, int n, int out, std::size_t len = 464,
                 unsigned limit_s = 3) {
    pid_t p = fork();
    if (p != 0)
        return p;
    seteuid(uid);
    alarm(limit_s);
    signal(SIGALRM, [](int) {});
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        _exit(errno);
    for (int i = 0; i < n; ++i) {
        std::uint8_t buf[464] = {};
        ssize_t got = read(fd, buf, len);
        if (got != 464)
            _exit(got < 0 ? errno : EIO);
        write(out, buf, sizeof buf);
    }
    _exit(0);
}
bool read_obs(int fd, Observation &o) {
    std::uint8_t buf[464];
    std::size_t at = 0;
    while (at < sizeof buf) {
        ssize_t n = read(fd, buf + at, sizeof buf - at);
        if (n <= 0)
            return false;
        at += static_cast<std::size_t>(n);
    }
    return decode(buf, sizeof buf, o) == DecodeError::none;
}
int count_lines(qnx::TlmReader &r, const char *needle, int &all) {
    char line[qnx::kTlmLine];
    std::size_t n = 0;
    int hits = 0;
    while (r.next(line, sizeof line, n)) {
        ++all;
        hits += std::string(line, n).find(needle) != std::string::npos ? 1 : 0;
    }
    return hits;
}
} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    if (argc != 3) {
        std::printf("usage: guard_sim_check <guard-sim> <scenario F0.yaml>\n");
        return 2;
    }
    signal(SIGALRM, [](int) {
        static const char m[] = "RESULT FAIL (check did not finish)\n";
        write(1, m, sizeof m - 1);
        _exit(1);
    });
    alarm(90); // the capture step must never hang on a blocked read
    pid_t sim = -1;
    char *sim_argv[] = {argv[1], nullptr};
    if (posix_spawn(&sim, argv[1], nullptr, nullptr, sim_argv, environ) != 0) {
        std::printf("spawn guard-sim failed\n");
        return 1;
    }
    for (int i = 0; i < 200 && access("/dev/sim/obs/planner", F_OK) != 0; ++i)
        sleep_ms(10);
    check("guard-sim attached world, motor, obs paths",
          access("/dev/sim/world", F_OK) == 0 && access("/dev/sim/motor", F_OK) == 0 &&
              access("/dev/sim/obs/supervisor", F_OK) == 0);
    check("C7 read before load: state IDLE", field("state") == "IDLE");

    pid_t p = fork();
    if (p == 0) {
        seteuid(1104);
        _exit(load(argv[2]) == EPERM ? 0 : 1);
    }
    check("C7 load as uid 1104 (planner) -> EPERM", wait_child(p) == 0);
    check("C7 load of a missing file -> ENOENT",
          load("/system/guard/scenarios/none.yaml") == ENOENT);
    check("C7 start before load -> EINVAL", pause_world(0) == EINVAL);
    check("C7 load normalized F0 (#141 buffers) -> EOK", load(argv[2]) == EOK);
    check("status LOADED, scenario F0, mode B2, seed 1000",
          field("state") == "LOADED" && field("scenario") == "F0" && field("mode") == "B2" &&
              field("seed") == "1000");
    std::printf("map_path: %s\nconfig_path: %s\n", field("map_path").c_str(),
                field("config_path").c_str());

    qnx::TlmReader tlm;
    check("telemetry ring /guard_tlm_sim readable", tlm.open("sim") == EOK);
    int lines = 0;
    count_lines(tlm, "", lines); // skip anything before this run

    check("C5 motor as root -> EPERM (gate uid only)",
          wait_child(motor_child(0, 0.0, 10, 1)) == EPERM);
    int pipes[2];
    pipe(pipes);
    check("C6 supervisor feed as root -> EPERM",
          wait_child(feed_child("/dev/sim/obs/supervisor", 0, 1, pipes[1])) == EPERM);
    check("C6 read with 100-byte buffer -> EMSGSIZE",
          wait_child(feed_child("/dev/sim/obs/supervisor", 1103, 1, pipes[1], 100)) == EMSGSIZE);

    double x0 = std::atof(field("x").c_str()), y0 = std::atof(field("y").c_str());
    check("C7 start (pause=0) -> RUNNING", pause_world(0) == EOK && field("state") == "RUNNING");
    check("C7 load while running -> EBUSY", load(argv[2]) == EBUSY);
    pid_t gate = motor_child(1102, 0.3, 1500, 1);
    int sp[2], pp[2];
    pipe(sp);
    pipe(pp);
    pid_t sup = feed_child("/dev/sim/obs/supervisor", 1103, 5, sp[1]);
    pid_t plan = feed_child("/dev/sim/obs/planner", 1104, 5, pp[1]);
    Observation a[5], b[5];
    int got = 0;
    for (int i = 0; i < 5; ++i)
        got += read_obs(sp[0], a[i]) && read_obs(pp[0], b[i]) ? 1 : 0;
    bool feeds = true, seqs = true;
    int same = 0;
    for (int i = 0; i < got; ++i) {
        feeds = feeds && a[i].feed == 0 && b[i].feed == 1;
        seqs = seqs && (i == 0 || (a[i].seq == a[i - 1].seq + 1 && b[i].seq == b[i - 1].seq + 1));
        for (int j = 0; j < got; ++j)
            same += a[i].obs_id == b[j].obs_id && a[i].stamp_ns == b[j].stamp_ns && a[i].x == b[j].x
                        ? 1
                        : 0;
    }
    check("C6 both feeds deliver 5 observations",
          wait_child(sup) == 0 && wait_child(plan) == 0 && got == 5);
    check("C6 same obs_id/stamp/pose on both feeds (>= 4 of 5)", same >= 4);
    check("C6 feed byte 0/1, seq +1 per delivery", feeds && seqs);
    check("C5 gate uid 1102: 150 MotorCmds accepted", wait_child(gate) == 0);
    double moved =
        std::hypot(std::atof(field("x").c_str()) - x0, std::atof(field("y").c_str()) - y0);
    std::printf("motor_cmds: %s\nmotor_rejected: %s\n", field("motor_cmds").c_str(),
                field("motor_rejected").c_str());
    check("plant moved under v = 0.3 (> 0.1 m)", moved > 0.1);
    check("C5 non-increasing cmd_seq -> EINVAL",
          wait_child(motor_child(1102, 0.0, 10, 5)) == EINVAL);
    sleep_ms(150);
    check("50 ms without MotorCmd: actuator watchdog brakes", field("watchdog") == "1");

    check("C7 inject F4 -> EOK", inject(4, 1000, 0) == EOK);
    check("status lists F4", field("faults").find("F4") != std::string::npos);
    pid_t s4 = feed_child("/dev/sim/obs/supervisor", 1103, 1, sp[1], 464, 1);
    pid_t p4 = feed_child("/dev/sim/obs/planner", 1104, 1, pp[1]);
    Observation o4;
    check("F4: planner feed still delivers", wait_child(p4) == 0 && read_obs(pp[0], o4));
    check("F4: supervisor read stays blocked (unblocked by alarm)", wait_child(s4) == EINTR);
    sleep_ms(1000);
    check("C7 inject F1 lag 600 ms -> EOK", inject(1, 2000, 600) == EOK);
    sleep_ms(700);
    pid_t p1 = feed_child("/dev/sim/obs/planner", 1104, 1, pp[1]);
    Observation o1;
    bool got1 = wait_child(p1) == 0 && read_obs(pp[0], o1);
    check("F1: planner observation arrives >= 500 ms old",
          got1 && now_ns() - o1.stamp_ns >= 500'000'000);
    check("C7 F2 is a process fault: WorldInject refuses it", inject(2, 0, 0) == -1);

    check("C7 stop (pause=1) -> ENDED", pause_world(1) == EOK && field("state") == "ENDED");
    check("C7 start after end without load -> EBUSY", pause_world(0) == EBUSY);
    int all = 0;
    qnx::TlmReader t2;
    t2.open("sim");
    int starts = count_lines(t2, "\"kind\":\"run_start\"", all);
    qnx::TlmReader t3, t4, t5, t6;
    t3.open("sim");
    t4.open("sim");
    t5.open("sim");
    t6.open("sim");
    int n3 = 0;
    int ends = count_lines(t3, "\"kind\":\"run_end\"", n3);
    int f4 = count_lines(t4, "\"fault\":\"F4\",\"phase\":\"start\"", n3);
    int wd = count_lines(t5, "SIM_ACTUATOR_WATCHDOG", n3);
    int obs = count_lines(t6, "\"kind\":\"observation\"", n3);
    std::printf("telemetry lines: %d (dropped %llu)\n", all,
                static_cast<unsigned long long>(t2.dropped()));
    check("C8 ring: one run_start and one run_end", starts == 1 && ends == 1);
    check("C8 ring: fault F4 start, SIM_ACTUATOR_WATCHDOG", f4 == 1 && wd >= 1);
    check("C8 ring: observation records", obs > 10);

    kill(sim, SIGTERM);
    waitpid(sim, nullptr, 0);
    std::printf("RESULT %s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
