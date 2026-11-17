// guard_supervisor_check (B3, #40): on-target check of guard-supervisor with the real
// guard-sim and guard-gate from the image. Run as root:
//   /tmp/guard_supervisor_check /tmp/guard-supervisor /system/guard/scenarios/F0.yaml
// Plays operator (root: C7 load/start, C3 recover) and planner (uid 1104: C1 proposals).
// Prints CHECK lines (PASS/FAIL) and reason names only, no timing values.
#include "guard/codec.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/ipc.hpp"
#include "guard/qnx/tlm.hpp"
#include "guard/reason_codes.hpp"

#include <cerrno>
#include <cstdio>
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
bool gate_has(const char *s) { return slurp("/dev/guard/status").find(s) != std::string::npos; }
bool wait_gate(const char *s, long ms) {
    for (long t = 0; t < ms; t += 10, sleep_ms(10))
        if (gate_has(s))
            return true;
    return false;
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
    r.request_id = static_cast<std::uint64_t>(now_ns());
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
int wait_child(pid_t p) {
    int st = 0;
    waitpid(p, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : 255;
}

// ---- planner side (runs as uid 1104 in a child) ----
struct Planner {
    int coid = -1, feed = -1;
    std::uint64_t seq = 0, id = 0;
    bool next_obs(Observation &o) {
        std::uint8_t buf[464];
        return read(feed, buf, sizeof buf) == 464 &&
               decode(buf, sizeof buf, o) == DecodeError::none;
    }
    Proposal make(const Observation &o, double v) {
        Proposal p;
        p.proposal_id = ++id;
        p.planner_id = 1;
        p.seq = ++seq;
        p.based_on = {o.obs_id, o.seq, o.stamp_ns, o.map_version, 0};
        p.created_ns = now_ns();
        p.v_cmd = v;
        return p;
    }
    // Returns the errno of MsgSend; on EOK fills d.
    int send(const std::uint8_t *bytes, std::size_t len, Decision &d) {
        std::uint8_t reply[sizeof(Decision)] = {};
        int rc = qnx::send_bounded(coid, bytes, len, reply, sizeof reply, 50'000'000); // C1 bound
        if (rc == EOK && decode(reply, sizeof reply, d) != DecodeError::none)
            rc = EBADMSG;
        return rc;
    }
    int send(const Proposal &p, Decision &d) {
        std::array<std::uint8_t, sizeof(Proposal)> w{};
        encode(p, w);
        return send(w.data(), w.size(), d);
    }
};
const char *name(std::uint16_t r) { return reason_name(r).data(); }
constexpr std::uint16_t R(Reason r) { return static_cast<std::uint16_t>(r); }

int planner_child() {
    seteuid(1104);
    Planner pl;
    pl.feed = open("/dev/sim/obs/planner", O_RDONLY);
    if (qnx::open_named("guard/supervisor", pl.coid) != EOK || pl.feed < 0) {
        check("planner: name_open guard/supervisor and planner feed", false);
        return g_failed;
    }
    Observation o;
    Decision d;
    check("planner: observation from the planner feed", pl.next_obs(o));
    check("C1 small step -> AUTHORIZED, lease issued",
          pl.send(pl.make(o, 0.02), d) == EOK && d.verdict == 1 && d.lease_id > 0 &&
              d.primary_reason == R(Reason::OK_AUTHORIZED));
    std::printf("  decision: %s epoch %u\n", name(d.primary_reason), d.epoch);
    check("gate MOVING on the lease", wait_gate("mode=MOVING", 100));
    auto replay = pl.make(o, 0.02);
    replay.seq = 1;
    check("C1 replayed seq -> REJ_SEQ_REPLAY", pl.send(replay, d) == EOK && d.verdict == 2 &&
                                                   d.primary_reason == R(Reason::REJ_SEQ_REPLAY));
    std::printf("  decision: %s\n", name(d.primary_reason));
    pl.next_obs(o);
    auto unknown = pl.make(o, 0.02);
    unknown.based_on.obs_id = o.obs_id + 1000;
    check("C1 unknown obs_id -> REJ_UNKNOWN_OBSERVATION",
          pl.send(unknown, d) == EOK && d.verdict == 2 &&
              d.primary_reason == R(Reason::REJ_UNKNOWN_OBSERVATION));
    std::printf("  decision: %s\n", name(d.primary_reason));
    pl.next_obs(o);
    check("C1 v_cmd 1.3 > v_max -> REJ_SPEED_ENVELOPE",
          pl.send(pl.make(o, 1.3), d) == EOK && d.verdict == 2 &&
              d.primary_reason == R(Reason::REJ_SPEED_ENVELOPE));
    std::printf("  decision: %s\n", name(d.primary_reason));
    pl.next_obs(o);
    auto bad = pl.make(o, 0.02);
    std::array<std::uint8_t, sizeof(Proposal)> w{};
    encode(bad, w);
    w[4] = 2; // version 2
    check("C1 version 2 -> Decision REJ_SCHEMA", pl.send(w.data(), w.size(), d) == EOK &&
                                                     d.verdict == 2 &&
                                                     d.primary_reason == R(Reason::REJ_SCHEMA));
    std::uint8_t tiny[4] = {0x47, 0x44, 2, 0};
    check("C1 4-byte message -> EBADMSG", pl.send(tiny, sizeof tiny, d) == EBADMSG);
    pl.next_obs(o);
    Observation old = o;
    sleep_ms(300);
    check("C1 300 ms old observation -> REJ_STALE_OBSERVATION",
          pl.send(pl.make(old, 0.0), d) == EOK && d.verdict == 2 &&
              d.primary_reason == R(Reason::REJ_STALE_OBSERVATION));
    std::printf("  decision: %s\n", name(d.primary_reason));
    return g_failed;
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
        std::printf("usage: guard_supervisor_check <guard-supervisor> <scenario>\n");
        return 2;
    }
    check("image runs guard-sim and guard-gate",
          access("/dev/sim/world", F_OK) == 0 && access("/dev/guard/gate", F_OK) == 0);
    WorldLoad l;
    l.seed = 1000;
    l.mode = 2;
    std::strncpy(l.scenario_path.data(), argv[2], l.scenario_path.size() - 1);
    WorldPause stop;
    stop.pause = 1;
    world(DCMD_SIM_WORLD_PAUSE, stop); // end any earlier run
    check("C7 load F0", world(DCMD_SIM_WORLD_LOAD, l) == EOK);

    pid_t sup = -1;
    char *sup_argv[] = {argv[1], nullptr};
    if (posix_spawn(&sup, argv[1], nullptr, nullptr, sup_argv, environ) != 0) {
        std::printf("spawn guard-supervisor failed\n");
        return 1;
    }
    for (int i = 0; i < 300 && access("/dev/name/local/guard/supervisor", F_OK) != 0; ++i)
        sleep_ms(10);
    check("name_attach guard/supervisor", access("/dev/name/local/guard/supervisor", F_OK) == 0);
    check("C2 supervisor registered with the gate (epoch >= 1)",
          wait_gate("epoch=", 1000) && !gate_has("epoch=0\n"));
    WorldPause go;
    go.pause = 0;
    check("C7 start", world(DCMD_SIM_WORLD_PAUSE, go) == EOK);
    sleep_ms(200);
    check("C3 recover -> GATE_RECOVERED", recover() == 512);

    pid_t intruder = fork();
    if (intruder == 0) {
        seteuid(1105);
        int coid = -1;
        Proposal p;
        std::array<std::uint8_t, sizeof(Proposal)> w{};
        encode(p, w);
        std::uint8_t r[sizeof(Decision)];
        _exit(qnx::open_named("guard/supervisor", coid) == EOK &&
                      qnx::send_bounded(coid, w.data(), w.size(), r, sizeof r, 50'000'000) == EPERM
                  ? 0
                  : 1);
    }
    check("C1 from uid 1105 (not planner) -> EPERM", wait_child(intruder) == 0);

    pid_t planner = fork();
    if (planner == 0)
        _exit(planner_child());
    g_failed += wait_child(planner);

    // F4: the supervisor feed goes quiet; the monitor re-check revokes the lease.
    wait_gate("mode=READY", 3000);
    pid_t mover = fork();
    if (mover == 0) {
        seteuid(1104);
        Planner pl;
        pl.feed = open("/dev/sim/obs/planner", O_RDONLY);
        qnx::open_named("guard/supervisor", pl.coid);
        Observation o;
        Decision d;
        pl.seq = 1000;
        _exit(pl.next_obs(o) && pl.send(pl.make(o, 0.02), d) == EOK && d.verdict == 1 ? 0 : 1);
    }
    check("lease before F4 -> AUTHORIZED", wait_child(mover) == 0);
    WorldInject f4;
    f4.fault = 4;
    f4.duration_ms = 1000;
    check("C7 inject F4 (supervisor feed paused)", world(DCMD_SIM_WORLD_INJECT, f4) == EOK);
    qnx::TlmReader tlm;
    tlm.open("supervisor");
    sleep_ms(400);
    char line[qnx::kTlmLine];
    std::size_t n = 0;
    int decisions = 0, leases = 0, stale_revokes = 0;
    while (tlm.next(line, sizeof line, n)) {
        std::string s(line, n);
        decisions += s.find("\"kind\":\"decision\"") != std::string::npos;
        leases += s.find("\"kind\":\"lease\"") != std::string::npos;
        stale_revokes +=
            s.find("\"kind\":\"revoke\",\"payload\":{\"reason\":\"REVOKE_SENSOR_HEALTH") !=
            std::string::npos;
    }
    std::printf("telemetry: decision %d, lease %d, revoke(sensor health) %d\n", decisions, leases,
                stale_revokes);
    check("F4: monitor re-check revokes (REVOKE_SENSOR_HEALTH)", stale_revokes >= 1);
    check("C8 ring: one decision per readable proposal (7)", decisions == 7);
    bool stopped = wait_gate("latch_cause=SENSOR_LOST", 3000) && gate_has("mode=LATCHED");
    check("gate stops and latches SENSOR_LOST on the revoke", stopped);
    if (!stopped)
        std::printf("%s", slurp("/dev/guard/status").c_str());

    world(DCMD_SIM_WORLD_PAUSE, stop);
    kill(sup, SIGTERM);
    waitpid(sup, nullptr, 0);
    std::printf("RESULT %s (%d failed)\n", g_failed ? "FAIL" : "PASS", g_failed);
    return g_failed ? 1 : 0;
}
