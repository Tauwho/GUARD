// guard-sim (B3, #38): QNX adapter around guard::Plant and guard::World (core/).
// Single-threaded dispatch (ADR 0015) serving
//   /dev/sim/world             C7: devctl load/inject/pause (uid guard_op or root); read = status
//   /dev/sim/motor             C5: devctl MotorCmd (uid guard_gate only)
//   /dev/sim/obs/supervisor    C6: blocking read, delayed reply (uid guard_sup only)
//   /dev/sim/obs/planner       C6: same, uid guard_plan only
// A 10 ms tick at priority 40 steps the world and the plant; every 1/obs_rate_hz it samples
// one observation and publishes it on both feeds. No safety logic here.
//   guard-sim [--keep-root]
#include "guard/codec.hpp"
#include "guard/plant.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/inputs.hpp"
#include "guard/qnx/priv.hpp"
#include "guard/qnx/resmgr.hpp"
#include "guard/qnx/sched.hpp"
#include "guard/qnx/timer.hpp"
#include "guard/qnx/tlm.hpp"
#include "guard/scenario.hpp"

#include <array>
#include <cerrno>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <optional>
#include <time.h>
#include <unistd.h>

namespace {
using namespace guard;
constexpr int kPrio = 40;
constexpr std::uint64_t kTickNs = 10'000'000;
constexpr std::int64_t kMs = 1'000'000;
constexpr uid_t kGate[] = {1102}, kSup[] = {1103}, kPlan[] = {1104}, kOp[] = {1106, 0};

enum class Run { idle, loaded, running, ended };
const char *run_name(Run r) {
    const char *n[] = {"IDLE", "LOADED", "RUNNING", "ENDED"};
    return n[static_cast<int>(r)];
}

struct Feed {
    iofunc_attr_t attr{};
    qnx::DelayedReplies<8> waiting;
    std::uint64_t seq = 0;
    const uid_t *uids = nullptr;
};
struct Injected {
    unsigned fault = 0;
    std::int64_t from = 0, until = 0; // run-relative ns; until 0 = rest of the run
    std::int64_t param = 0;
};
struct Lagged {
    Observation obs;
    std::int64_t release = 0;
};

struct Sim {
    qnx::ResMgr rm;
    qnx::PulseTimer tick;
    qnx::TlmWriter tlm;
    Config config;
    MapSpec map;
    ScenarioSpec scenario;
    std::optional<World> world;
    std::optional<Plant> plant;
    Scene scene;
    Run run = Run::idle;
    WorldLoad load{};
    char map_path[160] = "", config_path[160] = "default";
    std::int64_t t0 = 0, rel = 0;
    std::uint64_t ticks = 0, motor_ok = 0, motor_rejected = 0, unauthorized = 0, last_seq = 0;
    std::array<bool, 9> fault_on{};
    std::array<Injected, 8> injected{};
    std::size_t injected_count = 0;
    std::array<Lagged, 64> lag{};
    std::size_t lag_head = 0, lag_count = 0;
    bool watchdog = false, contact = false, breach = false, margin_measured = false;
    unsigned contacts = 0, breaches = 0;
    double min_margin = 1000;
    int zone_override = -1;
    Feed sup, plan;
    resmgr_connect_funcs_t feed_connect{}, motor_connect{};
    resmgr_io_funcs_t feed_io{}, motor_io{};
    iofunc_attr_t motor_attr{};
    struct Snap {
        const void *ocb = nullptr;
        std::size_t len = 0;
        char text[2048];
    } snaps[8];
    char file[qnx::kInputMax];
} *g;

std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
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

template <class T> int read_struct(io_devctl_t *msg, T &out) {
    if (msg->i.nbytes < sizeof(T))
        return EMSGSIZE;
    return errno_of(
        decode(static_cast<const std::uint8_t *>(_DEVCTL_DATA(msg->i)), sizeof(T), out));
}

int allowed(resmgr_context_t *ctp, const uid_t *uids, std::size_t n) {
    int rc = qnx::require_euid(ctp->info.scoid, uids, n);
    if (rc == EPERM)
        ++g->unauthorized;
    return rc;
}

// ---- telemetry (C8, src "sim") ----
void emit(std::int64_t t, const char *kind, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
void emit(std::int64_t t, const char *kind, const char *fmt, ...) {
    char payload[1900];
    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(payload, sizeof payload, fmt, ap);
    va_end(ap);
    if (n > 0 && static_cast<std::size_t>(n) < sizeof payload)
        g->tlm.emit(t, kind, payload);
}

const char *mode_text(unsigned m) { return m == 0 ? "B0" : m == 1 ? "B1" : "B2"; }

// ---- faults: scenario timeline (World) plus C7 injections ----
const Injected *injected(unsigned fault) {
    for (std::size_t i = 0; i < g->injected_count; ++i) {
        const auto &f = g->injected[i];
        if (f.fault == fault && g->rel >= f.from && (f.until == 0 || g->rel < f.until))
            return &f;
    }
    return nullptr;
}
bool fault_active(unsigned fault) {
    return g->world->active_fault(fault, g->rel) != nullptr || injected(fault) != nullptr;
}
// Fault-specific parameter in the units of WorldInject.param (F1 lag ms, F5 mm/s, F8 Hz).
std::int64_t fault_param(unsigned fault) {
    if (const auto *f = injected(fault))
        return f->param;
    if (const auto *e = g->world->active_fault(fault, g->rel)) {
        if (fault == 1)
            return e->lag_ms;
        if (fault == 4)
            return e->supervisor_feed ? 1 : 0;
        if (fault == 5)
            return std::llround(e->v_cmd * 1000);
        if (fault == 8)
            return e->rate_hz;
    }
    return 0;
}

// ---- C6 feeds ----
void deliver(Feed &feed, Observation obs, std::uint8_t which) {
    if (feed.waiting.pending() == 0)
        return; // not queued: a reader that is not blocked misses this sample
    obs.feed = which;
    obs.seq = ++feed.seq;
    std::array<std::uint8_t, sizeof(Observation)> wire{};
    if (encode(obs, wire))
        feed.waiting.reply_all(static_cast<long>(wire.size()), wire.data(), wire.size());
}

void emit_observation(const Observation &o) {
    char obstacles[1200];
    std::size_t at = 0;
    for (unsigned i = 0; i < o.obstacle_count && at < sizeof obstacles; ++i)
        at += std::snprintf(obstacles + at, sizeof obstacles - at,
                            "%s{\"x\":%.17g,\"y\":%.17g,\"r\":%.17g}", i ? "," : "",
                            o.obstacles[i].x, o.obstacles[i].y, o.obstacles[i].r);
    obstacles[at < sizeof obstacles ? at : sizeof obstacles - 1] = '\0';
    emit(o.stamp_ns, "observation",
         "\"obs_id\":%" PRIu64 ",\"seq\":%" PRIu64
         ",\"feed\":\"planner\",\"x\":%.17g,\"y\":%.17g,\"theta\":%.17g,\"v_meas\":%.17g,"
         "\"w_meas\":%.17g,\"map_version\":%u,\"sensor_ok\":%s,\"obstacles\":[%s]",
         o.obs_id, o.obs_id, o.x, o.y, o.theta, o.v_meas, o.w_meas, o.map_version,
         o.sensor_ok ? "true" : "false", obstacles);
}

void release_lagged(std::int64_t now, bool all) {
    while (g->lag_count > 0) {
        auto &l = g->lag[g->lag_head];
        if (!all && l.release > now)
            break;
        deliver(g->plan, l.obs, 1);
        g->lag_head = (g->lag_head + 1) % g->lag.size();
        --g->lag_count;
    }
}

void sample(std::int64_t now) {
    auto obs = g->plant->sample(now, g->scene);
    emit_observation(obs);
    if (!(fault_active(4) && fault_param(4) == 1))
        deliver(g->sup, obs, 0); // F4: skipped on the supervisor feed, not queued
    if (fault_active(4) && fault_param(4) == 0)
        return;
    if (fault_active(1) && g->lag_count < g->lag.size()) {
        auto &slot = g->lag[(g->lag_head + g->lag_count++) % g->lag.size()];
        slot = {obs, now + fault_param(1) * kMs}; // F1: same obs_id and stamp, later arrival
    } else {
        release_lagged(now, true);
        deliver(g->plan, obs, 1);
    }
}

// ---- run lifecycle ----
void end_run(std::int64_t now, bool goal) {
    if (g->run != Run::running)
        return;
    char margin[48] = "";
    if (g->margin_measured)
        std::snprintf(margin, sizeof margin, ",\"min_margin_m\":%.17g", g->min_margin);
    // final_state and the C1/latch counters are completed by guard-bridge (docs/decisions/B.md).
    emit(now, "run_end",
         "\"final_state\":\"%s\",\"contacts\":%u,\"zone_breaches\":%u,\"proposals\":0,"
         "\"authorized\":0,\"rejected\":0,\"unverifiable\":0,\"latches\":0%s,"
         "\"sim_duration_ns\":%" PRId64,
         goal ? "GOAL" : "STOPPED", g->contacts, g->breaches, margin, g->rel);
    std::printf("guard-sim: run_end F%u %s contacts %u zone_breaches %u\n", g->scenario.id,
                goal ? "GOAL" : "time", g->contacts, g->breaches);
    g->run = Run::ended;
    g->lag_count = 0;
}

void start_run(std::int64_t now) {
    g->t0 = now;
    g->rel = 0;
    g->fault_on = {};
    g->injected_count = 0;
    g->lag_count = 0;
    g->watchdog = g->contact = g->breach = g->margin_measured = false;
    g->contacts = g->breaches = 0;
    g->min_margin = 1000;
    g->zone_override = -1;
    g->sup.seq = g->plan.seq = 0;
    char ablate[64] = "";
    const char *names[] = {"freshness", "stopping", "expiry", "epoch"};
    std::size_t at = 0;
    for (unsigned b = 0; b < 4; ++b)
        if (g->load.ablation_mask & (1u << b))
            at +=
                std::snprintf(ablate + at, sizeof ablate - at, "%s\"%s\"", at ? "," : "", names[b]);
    emit(now, "run_start",
         "\"scenario\":\"F%u\",\"mode\":\"%s\",\"seed\":%" PRIu64
         ",\"ablate\":[%s],\"contracts\":\"contracts-v1.2\",\"runner\":\"target\"",
         g->scenario.id, mode_text(g->load.mode), g->load.seed, ablate);
    std::printf("guard-sim: run_start F%u %s seed %" PRIu64 "\n", g->scenario.id,
                mode_text(g->load.mode), g->load.seed);
    g->run = Run::running;
}

void do_tick() {
    auto now = now_ns();
    ++g->ticks;
    if (g->run != Run::running)
        return;
    g->rel = now - g->t0;
    if (!g->world->update(g->rel)) {
        std::printf("guard-sim: world update failed\n");
        end_run(now, false);
        return;
    }
    g->scene = g->world->scene();
    if (g->zone_override >= 0 && !g->scene.zones[g->zone_override].active) {
        g->scene.zones[g->zone_override].active = true; // injected F6
        ++g->scene.map_version;
    }
    for (unsigned f = 1; f <= 8; ++f) {
        bool on = fault_active(f);
        if (on != g->fault_on[f]) {
            emit(now, "fault", "\"fault\":\"F%u\",\"phase\":\"%s\"", f, on ? "start" : "end");
            g->fault_on[f] = on;
        }
    }
    if (!g->plant->step(now, g->scene)) {
        std::printf("guard-sim: plant step failed\n");
        end_run(now, false);
        return;
    }
    if (g->plant->watchdog_active() && !g->watchdog)
        emit(now, "sim_event", "\"code\":\"SIM_ACTUATOR_WATCHDOG\"");
    g->watchdog = g->plant->watchdog_active();
    const auto truth = g->plant->ground_truth();
    bool measured = g->scene.wall_count || g->scene.obstacle_count;
    for (std::size_t i = 0; i < g->scene.zone_count; ++i)
        measured = measured || (g->scene.zones[i].active && g->scene.zones[i].restricted);
    if (measured) {
        g->min_margin = std::fmin(g->min_margin, truth.clearance);
        g->margin_measured = true;
    }
    if (truth.contact && !g->contact) {
        ++g->contacts;
        emit(now, "sim_event", "\"code\":\"SIM_CONTACT\",\"margin_m\":%.17g", truth.clearance);
    }
    if (truth.zone_breach && !g->breach) {
        ++g->breaches;
        emit(now, "sim_event", "\"code\":\"SIM_ZONE_BREACH\",\"margin_m\":%.17g", truth.clearance);
    }
    g->contact = truth.contact;
    g->breach = truth.zone_breach;
    release_lagged(now, false);
    const std::uint64_t every = 1000 / g->config.obs_rate_hz / g->config.t_tick_ms;
    if (every > 0 && g->ticks % every == 0)
        sample(now);
    const auto pose = g->plant->pose();
    if (std::hypot(pose.x - g->map.goal.x, pose.y - g->map.goal.y) <= 0.15)
        end_run(now, true); // same goal radius as guard::Planner::at_goal
    else if (g->rel >= static_cast<std::int64_t>(g->scenario.duration * 1e9))
        end_run(now, false);
}

int on_tick(message_context_t *, int, unsigned, void *) {
    do_tick();
    return 0;
}

// ---- C7 ----
int do_load(const WorldLoad &load) {
    if (g->run == Run::running)
        return EBUSY;
    const char *path = load.scenario_path.data();
    std::size_t n = 0;
    char id[40], rest[64];
    int rc = qnx::read_file(path, g->file, sizeof g->file, n);
    if (rc != EOK)
        return rc == EFBIG ? EINVAL : ENOENT;
    if (!qnx::scenario_map_id(g->file, n, id, sizeof id))
        return EINVAL;
    std::snprintf(rest, sizeof rest, "maps/%s.yaml", id);
    char map_path[160], config_path[160];
    if (!qnx::sibling_path(path, rest, map_path, sizeof map_path) ||
        !qnx::sibling_path(path, "config.yaml", config_path, sizeof config_path))
        return EINVAL;
    Config config;
    rc = qnx::read_file(config_path, g->file, sizeof g->file, n);
    if (rc == EOK && !load_config({g->file, n}, config))
        return EINVAL;
    if (rc != EOK && rc != ENOENT)
        return rc;
    if (rc == ENOENT)
        std::strcpy(config_path, "default");
    config.ablate_freshness = load.ablation_mask & 1;
    config.ablate_stopping = load.ablation_mask & 2;
    config.ablate_expiry = load.ablation_mask & 4;
    config.ablate_epoch = load.ablation_mask & 8;
    if (!valid(config) || config.obs_rate_hz == 0 || 1000 % config.obs_rate_hz)
        return EINVAL;
    if (qnx::read_file(map_path, g->file, sizeof g->file, n) != EOK)
        return ENOENT;
    if (!load_map({g->file, n}, g->map))
        return EINVAL;
    if (qnx::read_file(path, g->file, sizeof g->file, n) != EOK)
        return ENOENT;
    if (!load_scenario({g->file, n}, g->map, g->scenario))
        return EINVAL;
    g->config = config;
    g->world.emplace(g->map, g->scenario);
    g->plant.emplace(config, g->map.start, now_ns());
    g->scene = g->map.scene;
    g->load = load;
    std::strcpy(g->map_path, map_path);
    std::strcpy(g->config_path, config_path);
    g->run = Run::loaded;
    std::printf("guard-sim: loaded F%u (%s) mode %s seed %" PRIu64 "\n", g->scenario.id, id,
                mode_text(load.mode), load.seed);
    return EOK;
}

int do_inject(const WorldInject &in) {
    if (g->run != Run::running)
        return EBUSY;
    if (g->injected_count >= g->injected.size())
        return EAGAIN;
    Injected f;
    f.fault = in.fault;
    f.from = g->rel;
    // 0 = scenario default, else guardctl.md defaults.
    std::uint32_t duration = in.duration_ms;
    std::int64_t param = in.param;
    for (std::size_t i = 0; i < g->scenario.fault_count; ++i) {
        const auto &e = g->scenario.faults[i];
        if (e.fault != in.fault)
            continue;
        if (duration == 0)
            duration = e.duration_ms;
        if (param == 0)
            param = in.fault == 1   ? e.lag_ms
                    : in.fault == 5 ? std::llround(e.v_cmd * 1000)
                    : in.fault == 8 ? e.rate_hz
                                    : 0;
    }
    if (duration == 0)
        duration = in.fault == 1 ? 3000 : in.fault == 4 ? 1000 : in.fault == 8 ? 2000 : 0;
    if (param == 0)
        param = in.fault == 1 ? 600 : in.fault == 5 ? 1200 : in.fault == 8 ? 1000 : 0;
    if (in.fault == 4)
        param = 1; // C7: F4 pauses the supervisor feed
    if (in.fault == 6) {
        for (std::size_t z = 0; z < g->map.scene.zone_count && g->zone_override < 0; ++z)
            if (std::strstr(name_view(g->map.zone_names[z]).data(), "mri") != nullptr)
                g->zone_override = static_cast<int>(z);
        if (g->zone_override < 0)
            return EINVAL;
    }
    if (param < 0 || (in.fault == 1 && param > 10000) || (in.fault == 5 && param > 1200) ||
        (in.fault == 8 && param > 1000))
        return EINVAL;
    f.until = duration ? f.from + static_cast<std::int64_t>(duration) * kMs : 0;
    f.param = param;
    g->injected[g->injected_count++] = f;
    std::printf("guard-sim: injected F%u\n", in.fault);
    return EOK;
}

int do_pause(const WorldPause &p) {
    if (p.pause == 1) {
        end_run(now_ns(), false); // "scenario stop" ends the run (docs/decisions/B.md)
        return EOK;
    }
    if (g->run == Run::running)
        return EOK;
    if (g->run != Run::loaded)
        return g->run == Run::idle ? EINVAL : EBUSY;
    start_run(now_ns());
    return EOK;
}

int world_devctl(resmgr_context_t *ctp, io_devctl_t *msg, void *ocb) {
    int st = iofunc_devctl_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
    if (st != _RESMGR_DEFAULT)
        return st;
    int rc = allowed(ctp, kOp, 2);
    if (rc != EOK)
        return rc;
    switch (msg->i.dcmd) {
    case DCMD_SIM_WORLD_LOAD: {
        WorldLoad v;
        return (rc = read_struct(msg, v)) != EOK ? rc : do_load(v);
    }
    case DCMD_SIM_WORLD_INJECT: {
        WorldInject v;
        return (rc = read_struct(msg, v)) != EOK ? rc : do_inject(v);
    }
    case DCMD_SIM_WORLD_PAUSE: {
        WorldPause v;
        return (rc = read_struct(msg, v)) != EOK ? rc : do_pause(v);
    }
    default:
        return ENOSYS;
    }
}

std::size_t format_world(char *out, std::size_t cap) {
    char faults[128] = "none";
    std::size_t at = 0;
    if (g->run == Run::running)
        for (unsigned f = 1; f <= 8; ++f)
            if (fault_active(f))
                at += std::snprintf(faults + at, sizeof faults - at, "%sF%u:%" PRId64,
                                    at ? "," : "", f, fault_param(f));
    if (at == 0)
        std::strcpy(faults, "none");
    std::uint32_t zones = 0;
    for (std::size_t z = 0; z < g->scene.zone_count && z < 32; ++z)
        zones |= g->scene.zones[z].active ? (1u << z) : 0;
    Pose pose = g->plant ? g->plant->pose() : Pose{};
    double v = g->plant ? g->plant->linear_velocity() : 0;
    int n = std::snprintf(
        out, cap,
        "version: 1\nstate: %s\nscenario: F%u\nscenario_path: %s\nmap_path: %s\n"
        "config_path: %s\nmode: %s\nseed: %" PRIu64 "\nablation_mask: %u\nt_run_ns: %" PRId64
        "\nmap_version: %u\nzones_active: %u\nfaults: %s\nx: %.17g\ny: %.17g\ntheta: %.17g\n"
        "v: %.17g\nwatchdog: %d\ncontacts: %u\nzone_breaches: %u\nticks: %" PRIu64
        "\nmotor_cmds: %" PRIu64 "\nmotor_rejected: %" PRIu64 "\nunauthorized: %" PRIu64 "\n",
        run_name(g->run), g->scenario.id, g->run == Run::idle ? "-" : g->load.scenario_path.data(),
        g->run == Run::idle ? "-" : g->map_path, g->config_path, mode_text(g->load.mode),
        g->load.seed, g->load.ablation_mask, g->run == Run::running ? g->rel : 0,
        g->scene.map_version, zones, faults, pose.x, pose.y, pose.theta, v, g->watchdog ? 1 : 0,
        g->contacts, g->breaches, g->ticks, g->motor_ok, g->motor_rejected, g->unauthorized);
    return n > 0 ? (static_cast<std::size_t>(n) < cap ? n : cap - 1) : 0;
}

// C7 read(): one snapshot per open, taken at the first read (same rule as C4 in guard-gate).
int world_read(resmgr_context_t *ctp, io_read_t *msg, void *ocbv) {
    auto *ocb = static_cast<iofunc_ocb_t *>(ocbv);
    int st = iofunc_read_verify(ctp, msg, ocb, nullptr);
    if (st != EOK)
        return st;
    Sim::Snap *snap = nullptr;
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
        snap->len = format_world(snap->text, sizeof snap->text);
    }
    auto off = static_cast<std::size_t>(ocb->offset);
    std::size_t n = off >= snap->len ? 0 : snap->len - off;
    n = n < msg->i.nbytes ? n : msg->i.nbytes;
    ocb->offset += n;
    _IO_SET_READ_NBYTES(ctp, n);
    return _RESMGR_PTR(ctp, snap->text + off, n);
}

int world_close(resmgr_context_t *ctp, void *reserved, void *ocb) {
    for (auto &s : g->snaps)
        if (s.ocb == ocb)
            s.ocb = nullptr;
    return iofunc_close_ocb_default(ctp, reserved, static_cast<iofunc_ocb_t *>(ocb));
}

// ---- C5 ----
int motor_devctl(resmgr_context_t *ctp, io_devctl_t *msg, void *ocb) {
    int st = iofunc_devctl_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
    if (st != _RESMGR_DEFAULT)
        return st;
    if (msg->i.dcmd != DCMD_SIM_MOTOR)
        return ENOSYS;
    int rc = allowed(ctp, kGate, 1);
    MotorCmd cmd;
    if (rc == EOK)
        rc = read_struct(msg, cmd);
    if (rc == EOK && cmd.cmd_seq <= g->last_seq)
        rc = EINVAL;
    if (rc == EOK && g->plant && !g->plant->command(cmd, now_ns()))
        rc = EINVAL;
    if (rc != EOK) {
        ++g->motor_rejected;
        return rc;
    }
    g->last_seq = cmd.cmd_seq;
    ++g->motor_ok;
    return EOK;
}

// ---- C6 ----
Feed *feed_of(void *ocb) {
    auto *attr = static_cast<iofunc_ocb_t *>(ocb)->attr;
    return attr == &g->sup.attr ? &g->sup : attr == &g->plan.attr ? &g->plan : nullptr;
}

int feed_read(resmgr_context_t *ctp, io_read_t *msg, void *ocb) {
    int st = iofunc_read_verify(ctp, msg, static_cast<iofunc_ocb_t *>(ocb), nullptr);
    if (st != EOK)
        return st;
    Feed *feed = feed_of(ocb);
    if (feed == nullptr)
        return EBADF;
    if ((st = allowed(ctp, feed->uids, 1)) != EOK)
        return st;
    if (msg->i.nbytes < sizeof(Observation))
        return EMSGSIZE;
    return feed->waiting.save(ctp->rcvid, ocb) == EOK ? _RESMGR_NOREPLY : EAGAIN;
}

int feed_unblock(resmgr_context_t *ctp, io_pulse_t *msg, void *ocb) {
    Feed *feed = feed_of(ocb);
    if (feed != nullptr && feed->waiting.unblock(ctp->rcvid, EINTR))
        return _RESMGR_NOREPLY;
    return iofunc_unblock_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
}

int feed_close(resmgr_context_t *ctp, void *reserved, void *ocb) {
    if (Feed *feed = feed_of(ocb))
        feed->waiting.drop(ocb);
    return iofunc_close_ocb_default(ctp, reserved, static_cast<iofunc_ocb_t *>(ocb));
}

int fail(const char *what, int rc) {
    std::fprintf(stderr, "guard-sim: %s: %s\n", what, std::strerror(rc));
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
            std::fprintf(stderr, "usage: guard-sim [--keep-root]\n");
            return 2;
        }
    }
    static Sim sim;
    g = &sim;
    g->sup.uids = kSup;
    g->plan.uids = kPlan;
    g->rm.io().devctl = world_devctl;
    g->rm.io().read = world_read;
    g->rm.io().close_ocb = world_close;
    int rc, coid = -1, tick_code = 0;
    if ((rc = g->rm.attach("/dev/sim/world", 0666)) != EOK)
        return fail("attach /dev/sim/world", rc);
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &g->motor_connect, _RESMGR_IO_NFUNCS, &g->motor_io);
    g->motor_io.devctl = motor_devctl;
    iofunc_attr_init(&g->motor_attr, S_IFNAM | 0666, nullptr, nullptr);
    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &g->feed_connect, _RESMGR_IO_NFUNCS, &g->feed_io);
    g->feed_io.read = feed_read;
    g->feed_io.unblock = feed_unblock;
    g->feed_io.close_ocb = feed_close;
    iofunc_attr_init(&g->sup.attr, S_IFNAM | 0444, nullptr, nullptr);
    iofunc_attr_init(&g->plan.attr, S_IFNAM | 0444, nullptr, nullptr);
    if ((rc = g->rm.add_path("/dev/sim/motor", &g->motor_connect, &g->motor_io, &g->motor_attr)) !=
        EOK)
        return fail("attach /dev/sim/motor", rc);
    if ((rc = g->rm.add_path("/dev/sim/obs/supervisor", &g->feed_connect, &g->feed_io,
                             &g->sup.attr)) != EOK ||
        (rc = g->rm.add_path("/dev/sim/obs/planner", &g->feed_connect, &g->feed_io,
                             &g->plan.attr)) != EOK)
        return fail("attach /dev/sim/obs", rc);
    if ((rc = g->tlm.open("sim")) != EOK)
        return fail("telemetry ring", rc);
    if ((rc = g->rm.self_coid(coid)) != EOK ||
        (rc = g->rm.alloc_pulse(tick_code, on_tick, nullptr)) != EOK ||
        (rc = g->tick.create(coid, tick_code, kPrio)) != EOK)
        return fail("timer", rc);
    if ((rc = qnx::set_fifo(kPrio)) != EOK)
        return fail("SCHED_FIFO", rc);
    if (!keep_root && (rc = qnx::drop_and_lock(1101, 1101, nullptr, 0)) != EOK)
        return fail("drop to guard_sim (1101)", rc);
    if ((rc = g->tick.arm_periodic(kTickNs)) != EOK)
        return fail("tick", rc);
    std::printf("guard-sim: ready, uid %d\n", static_cast<int>(geteuid()));
    for (;;)
        if ((rc = g->rm.run_once()) != EOK)
            return fail("dispatch", rc);
}
