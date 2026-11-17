// guard-gate (B2, #39): QNX adapter around guard::Gate (core/). Single-threaded dispatch
// (ADR 0015) serving /dev/guard/gate (C2, C3, B0/B1 passthru) and /dev/guard/status (C4); a
// 10 ms tick and a per-lease one-shot expiry pulse at priority 35 drive Gate::tick, whose
// MotorCmd goes to /dev/sim/motor (C5) under a 5 ms TimerTimeout. No safety logic here.
//   guard-gate [--mode B0|B1|B2] [--keep-root]
#include "guard/codec.hpp"
#include "guard/gate_fsm.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/priv.hpp"
#include "guard/qnx/resmgr.hpp"
#include "guard/qnx/sched.hpp"
#include "guard/qnx/timer.hpp"
#include "guard/qnx/tlm.hpp"
#include "guard/reason_codes.hpp"
#include "status_text.hpp"

#include <sys/trace.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

namespace {
using namespace guard;
constexpr int kPrio = 35;
constexpr std::uint64_t kTickNs = 10'000'000, kMotorTimeoutNs = 5'000'000;
constexpr uid_t kSup[] = {1103, 0}, kOp[] = {1106, 0}, kPlan[] = {1104, 0};

struct State {
    explicit State(const Config &config) : gate(config) {}
    Gate gate;
    qnx::ResMgr rm;
    qnx::PulseTimer tick_timer, expiry_timer;
    MotorCmd motor{};
    const void *sup_ocb = nullptr;
    int motor_fd = -1;
    std::uint64_t ticks = 0, late_ticks = 0, unauthorized = 0;
    std::int64_t last_tick = -1;
    GateMode last_mode = GateMode::init;
    std::array<std::uint64_t, 700> last_counters{};
    qnx::TlmWriter tlm;
    resmgr_connect_funcs_t status_connect{};
    resmgr_io_funcs_t status_io{};
    iofunc_attr_t status_attr{};
    struct Snap {
        const void *ocb = nullptr;
        std::size_t len = 0;
        char text[2048];
    } snaps[8];
} *g;

std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

// One gate_transition (C8) per mode change. The reason is the gate counter that moved, picked
// the same way as the host harness (host/replay.cpp).
void log_mode() {
    auto s = g->gate.status(0);
    if (s.mode != g->last_mode) {
        std::printf("guard-gate: %s -> %s (latch %s, epoch %u)\n", gate::mode_name(g->last_mode),
                    gate::mode_name(s.mode), gate::latch_name(s.latch), s.epoch);
        unsigned reason = static_cast<unsigned>(Reason::GATE_STANDSTILL);
        if (s.mode == GateMode::ready && g->last_mode == GateMode::latched)
            reason = static_cast<unsigned>(Reason::GATE_RECOVERED);
        if (s.mode == GateMode::moving)
            reason = s.experiment == Mode::guarded
                         ? static_cast<unsigned>(Reason::GATE_LEASE_ACCEPTED)
                         : static_cast<unsigned>(Reason::GATE_PASSTHRU_ACCEPTED);
        if (s.mode == GateMode::stopping || s.mode == GateMode::latched)
            for (unsigned code = 506; code <= 516; ++code)
                if (code != 511 && code != 512 && code != 513 && code != 514 &&
                    s.counters[code] > g->last_counters[code])
                    reason = code;
        char payload[256];
        auto name = reason_name(static_cast<std::uint16_t>(reason));
        std::snprintf(payload, sizeof payload,
                      "\"from\":\"%s\",\"to\":\"%s\",\"reason\":\"%.*s\",\"epoch\":%u,"
                      "\"lease_id\":%llu,\"latch_cause\":\"%s\",\"unmet_mask\":%u",
                      gate::mode_name(g->last_mode), gate::mode_name(s.mode),
                      static_cast<int>(name.size()), name.data(), s.epoch,
                      static_cast<unsigned long long>(s.lease_id), gate::latch_name(s.latch),
                      s.recovery_unmet_mask & 31u);
        g->tlm.emit(now_ns(), "gate_transition", payload);
        if (reason == static_cast<unsigned>(Reason::GATE_LEASE_EXPIRED))
            trace_logf(qnx::kTraceExpiry, "expiry lease %llu",
                       static_cast<unsigned long long>(s.lease_id));
        g->last_mode = s.mode;
    }
    g->last_counters = s.counters;
}

// C5: one MotorCmd per tick, bounded by 5 ms. Undelivered while MOVING latches MOTOR_TIMEOUT.
void send_motor() {
    std::array<std::uint8_t, sizeof(MotorCmd)> wire{};
    int rc = ETIMEDOUT;
    if (g->motor_fd < 0)
        g->motor_fd = open("/dev/sim/motor", O_RDWR);
    if (g->motor_fd >= 0 && encode(g->motor, wire)) {
        struct sigevent ev;
        SIGEV_UNBLOCK_INIT(&ev);
        std::uint64_t ns = kMotorTimeoutNs;
        TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
        rc = devctl(g->motor_fd, DCMD_SIM_MOTOR, wire.data(), wire.size(), nullptr);
        if (rc == EBADF) {
            close(g->motor_fd);
            g->motor_fd = -1;
        }
    }
    if (rc != EOK)
        g->gate.motor_timeout();
}

void do_tick() {
    auto now = now_ns();
    if (g->last_tick >= 0 && now - g->last_tick > static_cast<std::int64_t>(kTickNs * 3 / 2))
        ++g->late_ticks;
    g->last_tick = now;
    ++g->ticks;
    // v_meas: the gate's own last command (no C6 feed to the gate in v1.2); see
    // docs/decisions/B.md.
    g->motor = g->gate.tick(now, g->motor.v, g->motor.w);
    send_motor();
    log_mode();
}

int on_tick(message_context_t *, int, unsigned, void *) {
    do_tick();
    return 0;
}

int errno_of(DecodeError e) {
    switch (e) {
    case DecodeError::none:
        return EOK;
    case DecodeError::unsupported_version:
        return ENOTSUP;
    case DecodeError::wrong_size:
        return EMSGSIZE;
    case DecodeError::invalid_value:
    case DecodeError::schema:
        return EINVAL;
    default:
        return EBADMSG;
    }
}

int allowed(resmgr_context_t *ctp, const uid_t *uids, std::size_t n) {
    int rc = qnx::require_euid(ctp->info.scoid, uids, n);
    if (rc == EPERM)
        ++g->unauthorized;
    return rc;
}

template <class T> int reply_struct(resmgr_context_t *ctp, io_devctl_t *msg, const T &value) {
    std::array<std::uint8_t, sizeof(T)> wire{};
    if (!encode(value, wire))
        return EINVAL;
    std::memcpy(_DEVCTL_DATA(msg->o), wire.data(), wire.size());
    std::memset(&msg->o, 0, sizeof msg->o);
    msg->o.nbytes = wire.size();
    return _RESMGR_PTR(ctp, &msg->o, sizeof msg->o + wire.size());
}

template <class T> int read_struct(io_devctl_t *msg, T &out) {
    if (msg->i.nbytes < sizeof(T))
        return EMSGSIZE;
    return errno_of(
        decode(static_cast<const std::uint8_t *>(_DEVCTL_DATA(msg->i)), sizeof(T), out));
}

int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, void *ocb) {
    int st = iofunc_devctl_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
    if (st != _RESMGR_DEFAULT)
        return st;
    auto now = now_ns();
    int rc = EOK;
    switch (msg->i.dcmd) {
    case DCMD_GUARD_REGISTER: {
        if ((rc = allowed(ctp, kSup, 2)) != EOK)
            return rc;
        RegisterReply reply;
        if (!g->gate.register_supervisor(reply))
            return EPERM; // B0/B1: the core counted GATE_UNAUTHORIZED_CLIENT
        g->sup_ocb = ocb;
        log_mode();
        return reply_struct(ctp, msg, reply);
    }
    case DCMD_GUARD_LEASE: {
        Lease lease;
        if ((rc = allowed(ctp, kSup, 2)) != EOK || (rc = read_struct(msg, lease)) != EOK)
            return rc;
        auto ack = g->gate.prepare_lease(lease, now);
        if (ack.reason != static_cast<std::uint16_t>(Reason::GATE_LEASE_ACCEPTED))
            return reply_struct(ctp, msg, ack);
        // ADR 0017: reply in this handler; install only if the reply was delivered.
        std::array<std::uint8_t, sizeof(LeaseAck)> wire{};
        encode(ack, wire);
        std::memcpy(_DEVCTL_DATA(msg->o), wire.data(), wire.size());
        std::memset(&msg->o, 0, sizeof msg->o);
        msg->o.nbytes = wire.size();
        bool delivered = MsgReply(ctp->rcvid, EOK, &msg->o, sizeof msg->o + wire.size()) != -1;
        if (delivered)
            trace_logf(qnx::kTraceLease, "lease %llu",
                       static_cast<unsigned long long>(lease.lease_id));
        if (g->gate.commit_lease(lease.lease_seq, now, delivered) && lease.expires_ns > now)
            g->expiry_timer.arm_oneshot(static_cast<std::uint64_t>(lease.expires_ns - now));
        log_mode();
        return _RESMGR_NOREPLY;
    }
    case DCMD_GUARD_REVOKE: {
        Revoke revoke;
        if ((rc = allowed(ctp, kSup, 2)) != EOK || (rc = read_struct(msg, revoke)) != EOK)
            return rc;
        rc = g->gate.revoke(revoke) ? EOK : EINVAL;
        log_mode();
        return rc;
    }
    case DCMD_GUARD_RECOVER: {
        RecoverRequest request;
        if ((rc = allowed(ctp, kOp, 2)) != EOK || (rc = read_struct(msg, request)) != EOK)
            return rc;
        // Pose and observation conditions come from guard-sim in B3; true until then.
        auto reply = g->gate.recover(request, {true, true, true});
        log_mode();
        return reply_struct(ctp, msg, reply);
    }
    case DCMD_GUARD_PASSTHRU: {
        PassthruCmd cmd;
        if ((rc = allowed(ctp, kPlan, 2)) != EOK || (rc = read_struct(msg, cmd)) != EOK)
            return rc;
        auto reason = static_cast<unsigned>(g->gate.passthrough(cmd, now));
        log_mode();
        return reason == 505 ? EPERM : reason == 200 ? EINVAL : EOK;
    }
    default:
        return ENOSYS;
    }
}

int io_close_ocb(resmgr_context_t *ctp, void *reserved, void *ocb) {
    if (ocb == g->sup_ocb) {
        g->sup_ocb = nullptr;
        g->gate.supervisor_lost(); // C2 session rule (spike S3: closes even after kill -9)
        log_mode();
    }
    return iofunc_close_ocb_default(ctp, reserved, static_cast<iofunc_ocb_t *>(ocb));
}

// C4: one snapshot per open, taken at the first read at offset 0.
int status_read(resmgr_context_t *ctp, io_read_t *msg, void *ocbv) {
    auto *ocb = static_cast<iofunc_ocb_t *>(ocbv);
    int st = iofunc_read_verify(ctp, msg, ocb, nullptr);
    if (st != EOK)
        return st;
    State::Snap *snap = nullptr;
    for (auto &s : g->snaps)
        if (s.ocb == ocb)
            snap = &s;
    if (snap == nullptr) {
        for (auto &s : g->snaps)
            if (s.ocb == nullptr && snap == nullptr)
                snap = &s;
        if (snap == nullptr)
            return EAGAIN;
        snap->ocb = ocb;
        snap->len = gate::format_status(g->gate.status(now_ns()), g->motor, g->ticks, g->late_ticks,
                                        g->unauthorized, snap->text, sizeof snap->text);
    }
    auto off = static_cast<std::size_t>(ocb->offset);
    std::size_t n = off >= snap->len ? 0 : snap->len - off;
    n = n < msg->i.nbytes ? n : msg->i.nbytes;
    ocb->offset += n;
    _IO_SET_READ_NBYTES(ctp, n);
    return _RESMGR_PTR(ctp, snap->text + off, n);
}

int status_close(resmgr_context_t *ctp, void *reserved, void *ocb) {
    for (auto &s : g->snaps)
        if (s.ocb == ocb)
            s.ocb = nullptr;
    return iofunc_close_ocb_default(ctp, reserved, static_cast<iofunc_ocb_t *>(ocb));
}

int fail(const char *what, int rc) {
    std::fprintf(stderr, "guard-gate: %s: %s\n", what, std::strerror(rc));
    return 1;
}
} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    Config config;
    bool keep_root = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--mode") && i + 1 < argc) {
            const char *m = argv[++i];
            config.mode = !std::strcmp(m, "B0")   ? Mode::passthrough
                          : !std::strcmp(m, "B1") ? Mode::heartbeat
                                                  : Mode::guarded;
        } else if (!std::strcmp(argv[i], "--keep-root")) {
            keep_root = true;
        } else {
            std::fprintf(stderr, "usage: guard-gate [--mode B0|B1|B2] [--keep-root]\n");
            return 2;
        }
    }
    static State state(config);
    g = &state;
    g->last_mode = g->gate.status(0).mode;
    g->rm.io().devctl = io_devctl;
    g->rm.io().close_ocb = io_close_ocb;
    int rc, coid = -1, tick_code = 0, expiry_code = 0;
    if ((rc = g->rm.attach("/dev/guard/gate", 0666)) != EOK)
        return fail("attach /dev/guard/gate", rc);
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &g->status_connect, _RESMGR_IO_NFUNCS, &g->status_io);
    g->status_io.read = status_read;
    g->status_io.close_ocb = status_close;
    iofunc_attr_init(&g->status_attr, S_IFNAM | 0444, nullptr, nullptr);
    if ((rc = g->rm.add_path("/dev/guard/status", &g->status_connect, &g->status_io,
                             &g->status_attr)) != EOK)
        return fail("attach /dev/guard/status", rc);
    if ((rc = g->tlm.open("gate")) != EOK)
        return fail("telemetry ring", rc);
    if ((rc = g->rm.self_coid(coid)) != EOK ||
        (rc = g->rm.alloc_pulse(tick_code, on_tick, nullptr)) != EOK ||
        (rc = g->rm.alloc_pulse(expiry_code, on_tick, nullptr)) != EOK ||
        (rc = g->tick_timer.create(coid, tick_code, kPrio)) != EOK ||
        (rc = g->expiry_timer.create(coid, expiry_code, kPrio)) != EOK)
        return fail("timers", rc);
    if ((rc = qnx::set_fifo(kPrio)) != EOK)
        return fail("SCHED_FIFO", rc);
    if (!keep_root && (rc = qnx::drop_and_lock(1102, 1102, nullptr, 0)) != EOK)
        return fail("drop to guard_gate (1102)", rc);
    if ((rc = g->tick_timer.arm_periodic(kTickNs)) != EOK)
        return fail("tick", rc);
    std::printf("guard-gate: ready, uid %d, mode %s\n", static_cast<int>(geteuid()),
                gate::mode_name(g->last_mode));
    for (;;)
        if ((rc = g->rm.run_once()) != EOK)
            return fail("dispatch", rc);
}
