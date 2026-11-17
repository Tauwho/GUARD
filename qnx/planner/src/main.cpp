// guard-planner (B3, #41): QNX adapter around guard::Planner (core/), rule backend.
//  - C6 reader thread on /dev/sim/obs/planner feeds Planner::observe.
//  - The main loop asks Planner::propose at the scenario rate and sends each proposal to
//    guard-supervisor (C1, MsgSend bounded by a 50 ms TimerTimeout); the Decision goes back
//    to Planner::decision. In B0/B1 the command goes to the gate as DCMD_GUARD_PASSTHRU.
//  - Scenario, map, config and active faults (F5 aggressive, F8 flood) come from
//    /dev/sim/world. The planner is untrusted: priority 10, uid guard_plan (1104).
//   guard-planner [--keep-root]
#include "guard/codec.hpp"
#include "guard/planner.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/inputs.hpp"
#include "guard/qnx/ipc.hpp"
#include "guard/qnx/priv.hpp"
#include "guard/qnx/sched.hpp"
#include "guard/qnx/tlm.hpp"
#include "guard/qnx/world_status.hpp"

#include <array>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <devctl.h>
#include <fcntl.h>
#include <optional>
#include <pthread.h>
#include <semaphore.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

namespace {
using namespace guard;
constexpr int kPrio = 10;
constexpr std::uint64_t kC1TimeoutNs = 50'000'000, kGateTimeoutNs = 5'000'000;
constexpr std::int64_t kWorldEveryNs = 50'000'000;

struct State {
    pthread_mutex_t lock;
    std::optional<Planner> planner;
    ScenarioSpec scenario;
    Config config;
    qnx::WorldStatus world;
    std::int64_t world_read_ns = 0;
    int obs_fd = -1, sup_coid = -1, gate_fd = -1;
    qnx::TlmWriter tlm;
    sem_t go;
    std::uint64_t sent = 0, timeouts = 0, authorized = 0, passthru_seq = 0, run_gen = 0;
    std::int64_t backoff_until = 0; // after a failed C1 send (supervisor gone, F3/F7)
    char file[qnx::kInputMax];
} *g;

std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}
void sleep_ns(std::int64_t ns) {
    struct timespec ts {
        static_cast<time_t>(ns / 1'000'000'000), static_cast<long>(ns % 1'000'000'000)
    };
    nanosleep(&ts, nullptr);
}

void *reader(void *) {
    while (sem_wait(&g->go) == -1 && errno == EINTR) {
        // first read only after the drop to guard_plan (the sim checks the uid)
    }
    std::uint8_t buf[sizeof(Observation)];
    for (;;) {
        if (read(g->obs_fd, buf, sizeof buf) != static_cast<ssize_t>(sizeof buf)) {
            sleep_ns(10'000'000);
            continue;
        }
        Observation obs;
        if (decode(buf, sizeof buf, obs) != DecodeError::none)
            continue;
        pthread_mutex_lock(&g->lock);
        if (g->planner)
            g->planner->observe(obs);
        pthread_mutex_unlock(&g->lock);
    }
    return nullptr;
}

// New guard::Planner for each loaded run. Caller holds the lock.
void refresh_world(std::int64_t now) {
    qnx::WorldStatus s;
    if (qnx::read_world(s) != EOK)
        return;
    g->world_read_ns = now;
    bool new_run =
        std::strcmp(s.scenario_path, g->world.scenario_path) != 0 || s.seed != g->world.seed ||
        (std::strcmp(s.state, "LOADED") == 0 && std::strcmp(g->world.state, "LOADED") != 0);
    if (new_run && s.loaded()) {
        static MapSpec map;
        static ScenarioSpec scenario;
        Config config;
        std::size_t n = 0;
        bool ok = true;
        if (std::strcmp(s.config_path, "default") != 0)
            ok = qnx::read_file(s.config_path, g->file, sizeof g->file, n) == EOK &&
                 load_config({g->file, n}, config);
        ok = ok && qnx::read_file(s.map_path, g->file, sizeof g->file, n) == EOK &&
             load_map({g->file, n}, map);
        ok = ok && qnx::read_file(s.scenario_path, g->file, sizeof g->file, n) == EOK &&
             load_scenario({g->file, n}, map, scenario);
        if (ok) {
            g->config = config;
            g->scenario = scenario;
            g->planner.emplace(map, config, 1);
            ++g->run_gen;
            std::printf("guard-planner: run %s seed %" PRIu64 "\n", s.scenario_path, s.seed);
        } else {
            g->planner.reset();
            std::printf("guard-planner: cannot load inputs for %s\n", s.scenario_path);
        }
    }
    g->world = s;
}

PlannerOptions options() {
    PlannerOptions o;
    o.cruise_v = g->scenario.cruise_v;
    o.rate_hz = g->scenario.rate_hz;
    o.lease_req_ms = g->scenario.lease_req_ms;
    if (g->scenario.planner == ScenarioSpec::Planner::shortcut)
        o.mode = PlannerMode::shortcut;
    if (g->scenario.planner == ScenarioSpec::Planner::aggressive)
        o.mode = PlannerMode::aggressive;
    // F1 is the sim's delayed planner feed (C6); the planner itself stays nominal.
    std::int64_t p = 0;
    if (qnx::world_fault(g->world, 5, p)) {
        o.mode = PlannerMode::aggressive;
        o.cruise_v = p > 0 && p <= 1200 ? p / 1000.0 : 1.2;
    }
    if (qnx::world_fault(g->world, 8, p)) {
        o.mode = PlannerMode::flood;
        o.rate_hz = p > 0 && p <= 1000 ? static_cast<std::uint32_t>(p) : 1000;
    }
    return o;
}

// C1: proposal -> decision. Returns the errno of MsgSend.
int send_proposal(const Proposal &p, Decision &d) {
    if (g->sup_coid < 0 && qnx::open_named("guard/supervisor", g->sup_coid) != EOK) {
        g->sup_coid = -1;
        return ENOENT;
    }
    std::array<std::uint8_t, sizeof(Proposal)> wire{};
    std::uint8_t reply[sizeof(Decision)] = {};
    if (!encode(p, wire))
        return EINVAL;
    int rc =
        qnx::send_bounded(g->sup_coid, wire.data(), wire.size(), reply, sizeof reply, kC1TimeoutNs);
    if (rc != EOK && rc != ETIMEDOUT && rc != EBADMSG) { // supervisor gone: look it up again
        name_close(g->sup_coid);
        g->sup_coid = -1;
    }
    if (rc == EOK && decode(reply, sizeof reply, d) != DecodeError::none)
        rc = EBADMSG;
    return rc;
}

// B0/B1: the planner command goes to the gate unchecked (messages.md 3.4).
void passthrough(const Proposal &p, std::int64_t now) {
    if (g->gate_fd < 0 && (g->gate_fd = open("/dev/guard/gate", O_RDWR)) < 0)
        return;
    PassthruCmd c;
    c.cmd_seq = ++g->passthru_seq;
    c.v = p.v_cmd;
    c.w = p.w_cmd;
    c.stamp_ns = now;
    std::array<std::uint8_t, sizeof c> wire{};
    if (!std::isfinite(c.v) || !std::isfinite(c.w) || !encode(c, wire))
        return;
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = kGateTimeoutNs;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    int rc = devctl(g->gate_fd, DCMD_GUARD_PASSTHRU, wire.data(), wire.size(), nullptr);
    char payload[200];
    std::snprintf(payload, sizeof payload,
                  "\"cmd_seq\":%" PRIu64 ",\"v\":%.17g,\"w\":%.17g,\"reason\":\"%s\"", c.cmd_seq,
                  c.v, c.w, rc == EOK ? "GATE_PASSTHRU_ACCEPTED" : "GATE_NOT_READY");
    g->tlm.emit(now, "proposal", payload);
}

void step() {
    auto now = now_ns();
    pthread_mutex_lock(&g->lock);
    if (now - g->world_read_ns >= kWorldEveryNs)
        refresh_world(now);
    Proposal p;
    bool ready = g->planner && g->world.running() && !g->planner->at_goal() &&
                 g->planner->propose(now, options(), p);
    bool guarded = std::strcmp(g->world.mode, "B2") == 0;
    ready = ready && now >= g->backoff_until;
    const auto gen = g->run_gen;
    pthread_mutex_unlock(&g->lock);
    if (!ready)
        return;
    if (!guarded) {
        passthrough(p, now);
        return;
    }
    Decision d;
    int rc = send_proposal(p, d); // no lock held while blocked on the supervisor
    if (rc == EOK || rc == ETIMEDOUT)
        ++g->sent;
    if (rc == ETIMEDOUT)
        ++g->timeouts; // dropped; the next proposal has a higher seq (C1)
    if (rc != EOK && rc != ETIMEDOUT)
        g->backoff_until = now + 50'000'000;
    if (rc != EOK)
        return;
    g->authorized += d.verdict == 1 ? 1 : 0;
    pthread_mutex_lock(&g->lock);
    if (g->planner && g->run_gen == gen && d.proposal_id == p.proposal_id)
        g->planner->decision(d); // never a reply from the previous run
    pthread_mutex_unlock(&g->lock);
}

int fail(const char *what, int rc) {
    std::fprintf(stderr, "guard-planner: %s: %s\n", what, std::strerror(rc));
    return 1;
}
} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    bool keep_root = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--keep-root")) {
            keep_root = true;
        } else {
            std::fprintf(stderr, "usage: guard-planner [--keep-root]\n");
            return 2;
        }
    }
    static State state;
    g = &state;
    sem_init(&g->go, 0, 0);
    pthread_mutexattr_t ma;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_setprotocol(&ma, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(&g->lock, &ma);
    int rc;
    if ((rc = g->tlm.open("planner")) != EOK)
        return fail("telemetry ring", rc);
    if ((g->obs_fd = open("/dev/sim/obs/planner", O_RDONLY)) < 0)
        return fail("open /dev/sim/obs/planner", errno);
    if ((rc = qnx::set_fifo(kPrio)) != EOK)
        return fail("SCHED_FIFO", rc);
    pthread_t tid; // inherits SCHED_FIFO 10
    if ((rc = pthread_create(&tid, nullptr, reader, nullptr)) != EOK)
        return fail("reader thread", rc);
    if (!keep_root && (rc = qnx::drop_and_lock(1104, 1104, nullptr, 0)) != EOK)
        return fail("drop to guard_plan (1104)", rc);
    sem_post(&g->go);
    std::printf("guard-planner: ready, uid %d\n", static_cast<int>(geteuid()));
    for (;;) {
        step();
        std::uint32_t hz = 20;
        pthread_mutex_lock(&g->lock);
        if (g->planner && g->world.running())
            hz = options().rate_hz;
        pthread_mutex_unlock(&g->lock);
        // Wake at twice the proposal rate; Planner::propose keeps the rate itself.
        sleep_ns(hz >= 500 ? 1'000'000 : 500'000'000 / hz);
    }
}
