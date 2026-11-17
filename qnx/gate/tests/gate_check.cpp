// guard_gate_check (B2, #39): on-target check of guard-gate. Run as root:
//   /tmp/guard_gate_check /tmp/guard-gate
// Serves a fake /dev/sim/motor, starts guard-gate, then acts as supervisor and operator.
// Prints CHECK lines (PASS/FAIL), status snapshots without timing fields, and reason codes.
#include "guard/codec.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/resmgr.hpp"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace {
using namespace guard;
constexpr std::int64_t kMs = 1'000'000;
std::atomic<double> g_motor_v{-1};
std::atomic<int> g_motor_cmds{0};
int g_failed = 0;

std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}
void sleep_ms(long ms) {
    struct timespec ts {
        ms / 1000, (ms % 1000) * 1'000'000L
    };
    nanosleep(&ts, nullptr);
}
void check(const char *name, bool ok) {
    std::printf("CHECK %-44s %s\n", name, ok ? "PASS" : "FAIL");
    g_failed += ok ? 0 : 1;
}

// Fake guard-sim motor server (C5): records the last command.
int motor_devctl(resmgr_context_t *ctp, io_devctl_t *msg, void *ocb) {
    int st = iofunc_devctl_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
    if (st != _RESMGR_DEFAULT)
        return st;
    MotorCmd cmd;
    if (msg->i.dcmd != DCMD_SIM_MOTOR || msg->i.nbytes < sizeof cmd ||
        decode(static_cast<const std::uint8_t *>(_DEVCTL_DATA(msg->i)), sizeof cmd, cmd) !=
            DecodeError::none)
        return EINVAL;
    g_motor_v = cmd.v;
    g_motor_cmds++;
    return EOK;
}
void *motor_server(void *) {
    static qnx::ResMgr rm;
    rm.io().devctl = motor_devctl;
    if (rm.attach("/dev/sim/motor", 0666) != EOK) {
        std::printf("fake sim: attach failed\n");
        return nullptr;
    }
    for (;;)
        rm.run_once();
}

int call(int fd, unsigned cmd, void *data, std::size_t len) {
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = 5 * kMs; // C2 client bound
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    return devctl(fd, cmd, data, len, nullptr);
}
template <class T> std::array<std::uint8_t, sizeof(T)> wire(const T &v) {
    std::array<std::uint8_t, sizeof(T)> w{};
    encode(v, w);
    return w;
}
int do_register(int fd, std::uint32_t &epoch) {
    std::uint8_t buf[24] = {};
    int rc = call(fd, DCMD_GUARD_REGISTER, buf, sizeof buf);
    RegisterReply r;
    if (rc == EOK && decode(buf, sizeof buf, r) == DecodeError::none)
        epoch = r.epoch;
    return rc;
}
int do_lease(int fd, std::uint32_t epoch, std::uint64_t seq, std::uint16_t &reason) {
    Lease l;
    l.lease_id = l.lease_seq = seq;
    l.proposal_id = seq;
    l.epoch = epoch;
    l.v_cmd = 0.5;
    l.v_cap = 1.0;
    l.issued_ns = now_ns();
    l.expires_ns = l.issued_ns + 250 * kMs;
    auto buf = wire(l);
    int rc = call(fd, DCMD_GUARD_LEASE, buf.data(), buf.size());
    LeaseAck ack;
    reason = rc == EOK && decode(buf.data(), sizeof ack, ack) == DecodeError::none ? ack.reason : 0;
    return rc;
}
int do_recover(int fd, std::uint16_t &reason) {
    RecoverRequest req;
    req.request_id = 1;
    std::uint8_t buf[24] = {};
    auto w = wire(req);
    std::memcpy(buf, w.data(), w.size());
    int rc = call(fd, DCMD_GUARD_RECOVER, buf, sizeof buf);
    RecoverReply r;
    reason = rc == EOK && decode(buf, sizeof buf, r) == DecodeError::none ? r.reason : 0;
    return rc;
}
std::string status() {
    std::string out;
    int fd = open("/dev/guard/status", O_RDONLY);
    char buf[512];
    for (ssize_t n; fd >= 0 && (n = read(fd, buf, sizeof buf)) > 0;)
        out.append(buf, static_cast<std::size_t>(n));
    if (fd >= 0)
        close(fd);
    return out;
}
bool has(const std::string &s, const char *line) { return s.find(line) != std::string::npos; }
void show(const char *label) { // print snapshot without timing-dependent fields
    std::printf("-- status %s\n", label);
    auto s = status();
    for (std::size_t a = 0, b; a < s.size(); a = b + 1) {
        b = s.find('\n', a);
        auto line = s.substr(a, b - a);
        if (!line.compare(0, 5, "mode=") || !line.compare(0, 6, "epoch=") ||
            !line.compare(0, 12, "latch_cause=") || !line.compare(0, 6, "count."))
            std::printf("   %s\n", line.c_str());
    }
}
bool wait_for(const char *line, int limit_ms) {
    for (int i = 0; i < limit_ms; i += 5, sleep_ms(5))
        if (has(status(), line))
            return true;
    return false;
}
} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    if (argc < 2 || getuid() != 0) {
        std::fprintf(stderr, "usage (as root): guard_gate_check /path/to/guard-gate\n");
        return 2;
    }
    pthread_t sim;
    pthread_create(&sim, nullptr, motor_server, nullptr);
    pid_t gate = -1;
    char *gate_argv[] = {argv[1], nullptr};
    if (posix_spawn(&gate, argv[1], nullptr, nullptr, gate_argv, environ) != 0) {
        std::printf("spawn guard-gate failed\n");
        return 1;
    }
    int fd = -1;
    for (int i = 0; i < 200 && (fd = open("/dev/guard/gate", O_RDWR)) < 0; ++i)
        sleep_ms(10);
    check("gate attached /dev/guard/gate", fd >= 0);
    show("at boot");
    check("C4 cat status: LATCHED, latch BOOT", has(status(), "mode=LATCHED\nepoch=0"));

    std::uint32_t epoch = 0;
    std::uint16_t reason = 0;
    check("C2 register gives epoch 1", do_register(fd, epoch) == EOK && epoch == 1);
    sleep_ms(100); // standstill
    check("C3 recover (root) -> GATE_RECOVERED", do_recover(fd, reason) == EOK && reason == 512);
    check("lease accepted -> GATE_LEASE_ACCEPTED",
          do_lease(fd, epoch, 1, reason) == EOK && reason == 500);
    sleep_ms(50);
    show("after lease");
    check("MOVING and sim receives v > 0", has(status(), "mode=MOVING") && g_motor_v > 0);
    check("lease expiry stops: GATE_LEASE_EXPIRED", wait_for("count.GATE_LEASE_EXPIRED=1", 400));
    // Braking from 0.5 m/s at a_brake, then 5 standstill ticks (REF-FSM R15, R16).
    check("standstill, back to READY, sim v == 0", wait_for("mode=READY", 3000) && g_motor_v == 0);
    show("after expiry");

    std::uint32_t epoch2 = 0;
    do_register(fd, epoch2);
    check("stale-epoch lease -> GATE_EPOCH_STALE",
          do_lease(fd, epoch, 2, reason) == EOK && reason == 501);
    check("replayed seq -> GATE_LEASE_REPLAY",
          do_lease(fd, epoch2, 1, reason) == EOK && reason == 502);

    pid_t intruder = fork();
    if (intruder == 0) {
        seteuid(1104);
        int f = open("/dev/guard/gate", O_RDWR);
        std::uint16_t r = 0;
        _exit(do_lease(f, epoch2, 3, r) == EPERM ? 0 : 1);
    }
    int st = 0;
    waitpid(intruder, &st, 0);
    check("uid 1104 lease -> EPERM", WIFEXITED(st) && WEXITSTATUS(st) == 0);
    check("counted GATE_UNAUTHORIZED_CLIENT", has(status(), "count.GATE_UNAUTHORIZED_CLIENT=1"));
    pid_t op_try = fork();
    if (op_try == 0) {
        seteuid(1104);
        int f = open("/dev/guard/gate", O_RDWR);
        std::uint16_t r = 0;
        _exit(do_recover(f, r) == EPERM ? 0 : 1);
    }
    waitpid(op_try, &st, 0);
    check("uid 1104 recover -> EPERM", WIFEXITED(st) && WEXITSTATUS(st) == 0);

    int ready_pipe[2];
    pipe(ready_pipe);
    pid_t sup = fork();
    if (sup == 0) { // a supervisor that registers, leases, then hangs until killed
        int f = open("/dev/guard/gate", O_RDWR);
        std::uint32_t e = 0;
        std::uint16_t r = 0;
        do_register(f, e);
        char ok = do_lease(f, e, 100, r) == EOK && r == 500 ? 'y' : 'n';
        write(ready_pipe[1], &ok, 1);
        for (;;)
            pause();
    }
    char ok = 'n';
    read(ready_pipe[0], &ok, 1);
    check("supervisor child: lease accepted, MOVING", ok == 'y' && has(status(), "mode=MOVING"));
    show("before kill -9");
    auto deadline = now_ns() + 250 * kMs + 10 * kMs; // T_lease + 1 tick
    kill(sup, SIGKILL);
    waitpid(sup, nullptr, 0);
    bool stopped = false;
    while (now_ns() <= deadline && !stopped)
        stopped = !has(status(), "mode=MOVING");
    check("kill -9: STOPPING within T_lease + 1 tick", stopped);
    show("after kill -9");
    check("then STOPPED and LATCHED, SUPERVISOR_LOST",
          wait_for("mode=LATCHED", 3000) && has(status(), "latch_cause=SUPERVISOR_LOST"));
    show("final");

    kill(gate, SIGTERM);
    waitpid(gate, nullptr, 0);
    std::printf("RESULT %s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
