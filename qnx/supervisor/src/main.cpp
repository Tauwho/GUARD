// guard-supervisor (B3, #40): QNX adapter around guard::Supervisor (core/).
//  - C1 server: name_attach("guard/supervisor"); MsgReceive(Proposal) -> MsgReply(Decision).
//    The main thread runs at 10 and inherits the planner's priority for each proposal (ADR 0006).
//  - C6 reader at 30 on /dev/sim/obs/supervisor; after each observation the active lease is
//    re-checked (checks 8-11) and revoked on failure. A 20 ms monitor pulse at 30 re-checks too,
//    so a silent feed (F4) still leads to a revoke.
//  - C2 client of /dev/guard/gate: register, lease (5 ms TimerTimeout, ack rule), revoke.
//  - Map, config and zone state come from /dev/sim/world (guard-sim). No safety logic here.
//   guard-supervisor [--keep-root]
#include "guard/codec.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/inputs.hpp"
#include "guard/qnx/ipc.hpp"
#include "guard/qnx/priv.hpp"
#include "guard/qnx/sched.hpp"
#include "guard/qnx/timer.hpp"
#include "guard/qnx/tlm.hpp"
#include "guard/qnx/world_status.hpp"
#include "guard/scenario.hpp"
#include "guard/supervisor.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <devctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

namespace {
using namespace guard;
constexpr int kServerPrio = 10, kMonitorPrio = 30, kMonitorCode = _PULSE_CODE_MINAVAIL + 1;
constexpr std::uint64_t kMonitorNs = 20'000'000, kGateTimeoutNs = 5'000'000;
constexpr uid_t kPlanner[] = {1104, 0};

struct State {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // default QNX mutex: priority inheritance
    Supervisor supervisor;
    Config config;
    MapSpec map;
    Scene scene;
    bool world_ready = false;
    qnx::WorldStatus world;
    int gate_fd = -1, obs_fd = -1;
    std::uint32_t epoch = 0;
    std::uint64_t lease_seq = 0;
    Lease active{};
    bool lease_active = false;
    qnx::TlmWriter tlm;
    qnx::NamedServer server;
    qnx::PulseTimer monitor;
    sem_t go;
    std::uint64_t proposals = 0, unauthorized = 0, revokes = 0;
    char file[qnx::kInputMax];
} *g;

std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

void emit(std::int64_t t, const char *kind, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void emit(std::int64_t t, const char *kind, const char *fmt, ...) {
    char payload[qnx::kTlmLine];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(payload, sizeof payload, fmt, ap);
    va_end(ap);
    if (n > 0 && static_cast<std::size_t>(n) < sizeof payload)
        g->tlm.emit(t, kind, payload);
}

const char *reason_text(unsigned code) {
    return reason_name(static_cast<std::uint16_t>(code)).data();
}

// C2: one devctl bounded by TimerTimeout (send + reply blocked).
int gate_call(unsigned cmd, void *data, std::size_t len) {
    if (g->gate_fd < 0)
        return ENOENT;
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = kGateTimeoutNs;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    return devctl(g->gate_fd, cmd, data, len, nullptr);
}

void ensure_registered() {
    if (g->epoch != 0)
        return;
    if (g->gate_fd < 0 && (g->gate_fd = open("/dev/guard/gate", O_RDWR)) < 0)
        return;
    std::uint8_t buf[sizeof(RegisterReply)] = {};
    RegisterReply reply;
    if (gate_call(DCMD_GUARD_REGISTER, buf, sizeof buf) == EOK &&
        decode(buf, sizeof buf, reply) == DecodeError::none) {
        g->epoch = reply.epoch;
        g->lease_seq = std::max(g->lease_seq, reply.last_lease_seq);
        std::printf("guard-supervisor: registered, epoch %u\n", g->epoch);
    }
}

// Caller holds the lock.
void revoke(std::uint16_t reason, std::int64_t now) {
    Revoke r;
    r.lease_id = g->active.lease_id;
    r.epoch = g->epoch;
    r.reason = reason;
    std::array<std::uint8_t, sizeof(Revoke)> wire{};
    if (encode(r, wire))
        gate_call(DCMD_GUARD_REVOKE, wire.data(), wire.size());
    g->lease_active = false;
    ++g->revokes;
    emit(now, "revoke", "\"reason\":\"%s\",\"lease_id\":%" PRIu64, reason_text(reason),
         g->active.lease_id);
}

// Re-check the active lease (checks 8-11). Caller holds the lock.
void recheck(std::int64_t now) {
    if (!g->lease_active || !g->world_ready)
        return;
    if (now >= g->active.expires_ns) {
        g->lease_active = false; // the gate expires it on its own timer
        return;
    }
    if (auto reason = g->supervisor.recheck(g->active, g->scene, now))
        revoke(reason, now);
}

// Load config and map named by the world status; new Supervisor for each loaded run.
// Caller holds the lock.
void refresh_world(std::int64_t now) {
    qnx::WorldStatus s;
    if (qnx::read_world(s) != EOK)
        return;
    bool new_run =
        std::strcmp(s.scenario_path, g->world.scenario_path) != 0 || s.seed != g->world.seed ||
        (std::strcmp(s.state, "LOADED") == 0 && std::strcmp(g->world.state, "LOADED") != 0);
    if (new_run && s.loaded()) {
        Config config;
        std::size_t n = 0;
        bool ok = true;
        if (std::strcmp(s.config_path, "default") != 0)
            ok = qnx::read_file(s.config_path, g->file, sizeof g->file, n) == EOK &&
                 load_config({g->file, n}, config);
        static MapSpec map;
        ok = ok && qnx::read_file(s.map_path, g->file, sizeof g->file, n) == EOK &&
             load_map({g->file, n}, map);
        // Ablations come from the run's WorldLoad (freshness and stopping are supervisor checks).
        if (ok) {
            config.ablate_freshness = s.ablation_mask & 1;
            config.ablate_stopping = s.ablation_mask & 2;
            g->config = config;
            g->map = map;
            g->supervisor = Supervisor(config);
            g->lease_active = false;
            g->world_ready = true;
            std::printf("guard-supervisor: world %s seed %" PRIu64 "\n", s.scenario_path, s.seed);
        } else {
            g->world_ready = false;
            std::printf("guard-supervisor: cannot load inputs for %s\n", s.scenario_path);
        }
    }
    if (g->world_ready) {
        g->scene = g->map.scene;
        for (std::size_t z = 0; z < g->scene.zone_count && z < 32; ++z)
            g->scene.zones[z].active = (s.zones_active >> z) & 1u;
        g->scene.map_version = s.map_version;
    }
    g->world = s;
    (void)now;
}

// ---- C6 reader thread (priority 30) ----
void *reader(void *) {
    sem_wait(&g->go); // first read only after the drop to guard_sup (the sim checks the uid)
    std::uint8_t buf[sizeof(Observation)];
    for (;;) {
        ssize_t n = read(g->obs_fd, buf, sizeof buf);
        auto now = now_ns();
        if (n != static_cast<ssize_t>(sizeof buf)) {
            struct timespec ts {
                0, 10'000'000
            };
            nanosleep(&ts, nullptr); // sim restarting or not up yet
            continue;
        }
        Observation obs;
        if (decode(buf, sizeof buf, obs) != DecodeError::none)
            continue;
        pthread_mutex_lock(&g->lock);
        if (g->world_ready && obs.map_version != g->scene.map_version)
            refresh_world(now);
        if (g->world_ready)
            g->supervisor.observe(obs, now);
        recheck(now);
        pthread_mutex_unlock(&g->lock);
    }
    return nullptr;
}

// ---- C1 ----
void reply_decision(int rcvid, const Decision &d) {
    std::array<std::uint8_t, sizeof(Decision)> wire{};
    if (encode(d, wire))
        MsgReply(rcvid, EOK, wire.data(), wire.size());
    else
        MsgError(rcvid, EINVAL);
}

void emit_decision(std::int64_t now, const Proposal &p, const Decision &d,
                   const SafetyEvaluation *e) {
    const char *verdict = d.verdict == 1   ? "AUTHORIZED"
                          : d.verdict == 3 ? "UNVERIFIABLE"
                                           : "REJECTED";
    char reasons[400] = "";
    std::size_t at = 0;
    if (e != nullptr)
        for (auto code : e->reasons)
            if (code && at < sizeof reasons)
                at += std::snprintf(reasons + at, sizeof reasons - at, "%s\"%s\"", at ? "," : "",
                                    reason_text(code));
    if (d.primary_reason == static_cast<std::uint16_t>(Reason::REJ_GATE_NOT_READY) &&
        at < sizeof reasons)
        std::snprintf(reasons + at, sizeof reasons - at, "%s\"%s\"", at ? "," : "",
                      reason_text(d.primary_reason));
    emit(now, "decision",
         "\"proposal_id\":%" PRIu64 ",\"planner_id\":%u,\"verdict\":\"%s\",\"primary_reason\":"
         "\"%s\",\"reasons\":[%s],\"lease_id\":%" PRIu64 ",\"epoch\":%u",
         p.proposal_id, p.planner_id ? p.planner_id : 1, verdict, reason_text(d.primary_reason),
         reasons, d.lease_id, d.epoch);
}

void handle_proposal(int rcvid, const std::uint8_t *buf, std::size_t len) {
    auto now = now_ns();
    Proposal p;
    auto err = decode(buf, len, p);
    if (err == DecodeError::bad_message) {
        MsgError(rcvid, EBADMSG);
        return;
    }
    pthread_mutex_lock(&g->lock);
    ++g->proposals;
    Decision d;
    d.decided_ns = now;
    d.epoch = g->epoch;
    if (err != DecodeError::none) { // C1: REJ_SCHEMA in a Decision
        std::memcpy(&d.proposal_id, buf + 8, len >= 16 ? sizeof d.proposal_id : 0);
        d.verdict = 2;
        d.primary_reason = static_cast<std::uint16_t>(Reason::REJ_SCHEMA);
        d.reasons_mask = 1;
        p.proposal_id = d.proposal_id;
        emit_decision(now, p, d, nullptr);
        pthread_mutex_unlock(&g->lock);
        reply_decision(rcvid, d);
        return;
    }
    d.proposal_id = p.proposal_id;
    ensure_registered();
    if (!g->world_ready || g->epoch == 0) {
        d.verdict = 3;
        d.primary_reason = static_cast<std::uint16_t>(Reason::UNV_INTERNAL);
        emit_decision(now, p, d, nullptr);
        pthread_mutex_unlock(&g->lock);
        reply_decision(rcvid, d);
        return;
    }
    auto e = g->supervisor.evaluate(p, g->scene, now);
    unsigned reason = static_cast<unsigned>(e.primary_reason);
    bool accepted = false;
    Lease lease;
    if (e.authorized) {
        lease.lease_id = lease.lease_seq = ++g->lease_seq;
        lease.proposal_id = p.proposal_id;
        lease.epoch = g->epoch;
        lease.v_cmd = p.v_cmd;
        lease.w_cmd = p.w_cmd;
        lease.v_cap = g->config.v_max;
        lease.issued_ns = now;
        lease.expires_ns = now + milliseconds(std::min(g->config.t_lease_ms, p.lease_req_ms));
        std::array<std::uint8_t, sizeof(Lease)> wire{};
        LeaseAck ack;
        int rc =
            encode(lease, wire) ? gate_call(DCMD_GUARD_LEASE, wire.data(), wire.size()) : EINVAL;
        accepted = rc == EOK && decode(wire.data(), sizeof(LeaseAck), ack) == DecodeError::none &&
                   ack.reason == static_cast<std::uint16_t>(Reason::GATE_LEASE_ACCEPTED);
        if (accepted) {
            g->active = lease;
            g->lease_active = true;
            const auto *src = g->supervisor.find(p.based_on.obs_id);
            char where[200] = "";
            if (src != nullptr)
                std::snprintf(where, sizeof where,
                              ",\"x\":%.17g,\"y\":%.17g,\"theta\":%.17g,\"source_obs_id\":%" PRIu64,
                              src->x, src->y, src->theta, src->obs_id);
            emit(now, "lease",
                 "\"lease_id\":%" PRIu64 ",\"epoch\":%u,\"issued_ns\":%" PRId64
                 ",\"expires_ns\":%" PRId64 ",\"v_cmd\":%.17g,\"w_cmd\":%.17g,\"d_req\":%.17g%s",
                 lease.lease_id, lease.epoch, lease.issued_ns, lease.expires_ns, lease.v_cmd,
                 lease.w_cmd, e.required_distance, where);
        } else {
            if (rc == ENOENT || rc == EBADF || rc == EPERM) {
                close(g->gate_fd); // gate restarted or our session is gone: register again
                g->gate_fd = -1;
                g->epoch = 0;
            }
            reason = static_cast<unsigned>(Reason::REJ_GATE_NOT_READY);
            emit(now, "lease", "\"lease_id\":%" PRIu64 ",\"reason\":\"%s\"", lease.lease_id,
                 rc == EOK ? reason_text(ack.reason) : "GATE_NOT_READY");
        }
    }
    d.verdict = accepted ? 1 : (reason >= 300 && reason < 400 ? 3 : 2);
    d.primary_reason = static_cast<std::uint16_t>(reason);
    d.reasons_mask = e.reasons_mask;
    d.lease_id = accepted ? lease.lease_id : 0;
    d.expires_ns = accepted ? lease.expires_ns : 0;
    emit_decision(now, p, d, &e);
    pthread_mutex_unlock(&g->lock);
    reply_decision(rcvid, d);
}

int fail(const char *what, int rc) {
    std::fprintf(stderr, "guard-supervisor: %s: %s\n", what, std::strerror(rc));
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
            std::fprintf(stderr, "usage: guard-supervisor [--keep-root]\n");
            return 2;
        }
    }
    static State state; // about 300 KB: static storage, never a thread stack
    g = &state;
    sem_init(&g->go, 0, 0);
    int rc, coid = -1;
    if ((rc = g->server.attach("guard/supervisor")) != EOK)
        return fail("name_attach guard/supervisor", rc);
    if ((rc = g->tlm.open("supervisor")) != EOK)
        return fail("telemetry ring", rc);
    if ((rc = qnx::connect_self(g->server.chid(), coid)) != EOK ||
        (rc = g->monitor.create(coid, kMonitorCode, kMonitorPrio)) != EOK)
        return fail("monitor timer", rc);
    if ((g->obs_fd = open("/dev/sim/obs/supervisor", O_RDONLY)) < 0)
        return fail("open /dev/sim/obs/supervisor", errno);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    struct sched_param sp {};
    sp.sched_priority = kMonitorPrio;
    pthread_attr_setschedparam(&attr, &sp);
    pthread_t tid;
    if ((rc = pthread_create(&tid, &attr, reader, nullptr)) != EOK)
        return fail("reader thread", rc);
    if ((rc = qnx::set_fifo(kServerPrio)) != EOK)
        return fail("SCHED_FIFO", rc);
    if (!keep_root && (rc = qnx::drop_and_lock(1103, 1103, nullptr, 0)) != EOK)
        return fail("drop to guard_sup (1103)", rc);
    sem_post(&g->go);
    pthread_mutex_lock(&g->lock);
    ensure_registered();
    refresh_world(now_ns());
    pthread_mutex_unlock(&g->lock);
    if ((rc = g->monitor.arm_periodic(kMonitorNs)) != EOK)
        return fail("monitor", rc);
    std::printf("guard-supervisor: ready, uid %d, epoch %u\n", static_cast<int>(geteuid()),
                g->epoch);
    union {
        struct _pulse pulse;
        std::uint8_t bytes[256];
    } msg;
    for (;;) {
        struct _msg_info info {};
        int rcvid = g->server.receive(&msg, sizeof msg, &info, 0);
        if (rcvid == -1) {
            if (errno != EINTR)
                return fail("MsgReceive", errno);
            continue;
        }
        if (rcvid == 0) {
            if (msg.pulse.code == kMonitorCode) {
                auto now = now_ns();
                pthread_mutex_lock(&g->lock);
                ensure_registered();
                refresh_world(now);
                recheck(now);
                pthread_mutex_unlock(&g->lock);
            }
            continue;
        }
        if ((rc = qnx::require_euid(info.scoid, kPlanner, 2)) != EOK) {
            ++g->unauthorized;
            MsgError(rcvid, rc);
            continue;
        }
        if (info.msglen < static_cast<decltype(info.msglen)>(sizeof(Header))) {
            MsgError(rcvid, EBADMSG);
            continue;
        }
        // Longer than the buffer: decode sees the wrong size and answers REJ_SCHEMA (C1).
        auto len = std::min<std::size_t>(static_cast<std::size_t>(info.msglen), sizeof msg.bytes);
        handle_proposal(rcvid, msg.bytes, len);
    }
}
