// guardctl (B5, #42): the operator CLI on the target, contracts/guardctl.md (C14).
// Talks to the gate (C3 recover, C4 status) and, on the test image, to the sim (C7). Process
// faults F2, F3 and F7 act on real QNX processes. It never bypasses gate rules.
#include "guard/codec.hpp"
#include "guard/qnx/dcmd.hpp"
#include "guard/qnx/kv.hpp"
#include "guard/qnx/tlm.hpp"
#include "guard/reason_codes.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <devctl.h>
#include <fcntl.h>
#include <string>
#include <sys/neutrino.h>
#include <sys/syspage.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace guard;
constexpr std::uint64_t kC3Ms = 100, kC7Ms = 100;
bool g_json = false;
volatile std::sig_atomic_t g_stop = 0;

const char kUsage[] =
    "usage:\n"
    "  guardctl status [--json]\n"
    "  guardctl watch [--interval-ms N]\n"
    "  guardctl recover\n"
    "  guardctl inject F1|F2|F3|F4|F5|F6|F7|F8 [--duration-ms N] [--param N]\n"
    "  guardctl scenario load <path> [--seed N] [--mode B0|B1|B2]"
    " [--ablate freshness,stopping,expiry,epoch]\n"
    "  guardctl scenario start\n"
    "  guardctl scenario stop\n"
    "  guardctl --help | --version\n"
    "--json is accepted on every command.\n"
    "exit: 0 ok, 1 refused, 2 usage, 3 EPERM, 4 timeout, 5 device missing, 6 other\n";

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
std::string json_escape(const std::string &s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\')
            o += '\\';
        if (static_cast<unsigned char>(c) >= 0x20)
            o += c;
    }
    return o;
}
int result(const std::string &line) {
    if (g_json)
        std::printf("{\"ok\":true,\"result\":\"%s\"}\n", json_escape(line).c_str());
    else
        std::printf("%s\n", line.c_str());
    return 0;
}
// A refusal (exit 1) still prints its result line on stdout (guardctl.md).
int refused(const std::string &line) {
    if (g_json)
        std::printf("{\"ok\":false,\"error\":\"%s\",\"exit\":1}\n", json_escape(line).c_str());
    else
        std::printf("%s\n", line.c_str());
    return 1;
}
int error(const char *command, const std::string &reason, int code) {
    std::fprintf(stderr, "guardctl: %s: %s\n", command, reason.c_str());
    if (g_json)
        std::printf("{\"ok\":false,\"error\":\"%s\",\"exit\":%d}\n", json_escape(reason).c_str(),
                    code);
    return code;
}
int exit_of(int err) {
    switch (err) {
    case EOK:
        return 0;
    case EPERM:
    case EACCES:
        return 3;
    case ETIMEDOUT:
        return 4;
    case ENOENT:
        return 5;
    default:
        return 6;
    }
}
int errno_error(const char *command, int err) {
    return error(command, std::strerror(err), exit_of(err));
}

// open() is a message to the resource manager too; a stopped server would block it for
// ever. An interval timer interrupts it (EINTR -> ETIMEDOUT). Returns the fd or -1 (errno).
int open_bounded(const char *path, int flags, std::uint64_t ms) {
    struct sigaction sa {};
    sa.sa_handler = [](int) {};
    sigaction(SIGALRM, &sa, nullptr); // no SA_RESTART
    struct itimerval it {};
    it.it_value.tv_sec = static_cast<time_t>(ms / 1000);
    it.it_value.tv_usec = static_cast<suseconds_t>(ms % 1000 * 1000);
    setitimer(ITIMER_REAL, &it, nullptr);
    int fd = open(path, flags);
    int e = errno;
    struct itimerval off {};
    setitimer(ITIMER_REAL, &off, nullptr);
    errno = fd < 0 && e == EINTR ? ETIMEDOUT : e;
    return fd;
}

int bounded_devctl(const char *path, unsigned cmd, void *data, std::size_t len, std::uint64_t ms,
                   int &open_err) {
    int fd = open_bounded(path, O_RDWR, ms);
    open_err = fd < 0 ? errno : EOK;
    if (fd < 0)
        return open_err;
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = ms * 1'000'000;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    int rc = devctl(fd, cmd, data, len, nullptr);
    close(fd);
    return rc;
}
template <class T> int world(unsigned cmd, const T &v, int &open_err) {
    std::array<std::uint8_t, sizeof(T)> w{};
    if (!encode(v, w))
        return EINVAL;
    return bounded_devctl("/dev/sim/world", cmd, w.data(), w.size(), kC7Ms, open_err);
}

int read_text(const char *path, std::string &out) {
    char buf[4096];
    int fd = open_bounded(path, O_RDONLY, kC3Ms);
    if (fd < 0)
        return errno;
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = kC3Ms * 1'000'000;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    ssize_t n = read(fd, buf, sizeof buf);
    int e = errno;
    close(fd);
    if (n < 0)
        return e;
    out.assign(buf, static_cast<std::size_t>(n));
    return EOK;
}
// C4 lines are key=value.
std::string c4(const std::string &text, const char *key) {
    std::string k = std::string(key) + "=";
    std::size_t at = 0;
    while (at < text.size()) {
        auto end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        if (text.compare(at, k.size(), k) == 0)
            return text.substr(at + k.size(), end - at - k.size());
        at = end + 1;
    }
    return {};
}
std::string unmet_names(std::uint32_t mask) {
    const char *names[] = {"standstill", "outside_zones", "supervisor_registered",
                           "supervisor_fresh", "latch_cleared"};
    std::string s;
    for (unsigned b = 0; b < 5; ++b)
        if (mask & (1u << b))
            s += (s.empty() ? "" : ",") + std::string(names[b]);
    return s;
}

// ---- status / watch (C4) ----
std::string status_json(const std::string &t) {
    std::string o = "{";
    std::string counts;
    std::size_t at = 0;
    bool first = true;
    while (at < t.size()) {
        auto end = t.find('\n', at);
        if (end == std::string::npos)
            end = t.size();
        auto line = t.substr(at, end - at);
        at = end + 1;
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        auto key = line.substr(0, eq), val = line.substr(eq + 1);
        if (key.compare(0, 6, "count.") == 0) {
            counts += (counts.empty() ? "\"" : ",\"") + key.substr(6) + "\":" + val;
            continue;
        }
        bool text = key == "mode" || key == "latch_cause";
        if (key == "recovery_unmet_mask")
            val = std::to_string(std::strtoul(val.c_str(), nullptr, 16));
        o += (first ? "\"" : ",\"") + key + "\":" + (text ? "\"" + val + "\"" : val);
        first = false;
    }
    return o + (first ? "" : ",") + "\"counts\":{" + counts + "}}";
}

int cmd_status(bool json) {
    std::string t;
    int rc = read_text("/dev/guard/status", t);
    if (rc != EOK)
        return errno_error("status", rc);
    if (json)
        std::printf("%s\n", status_json(t).c_str());
    else
        std::fputs(t.c_str(), stdout);
    return 0;
}

int cmd_watch(long interval) {
    std::signal(SIGINT, [](int) { g_stop = 1; });
    while (!g_stop) {
        std::string t;
        int rc = read_text("/dev/guard/status", t);
        if (rc != EOK)
            return errno_error("watch", rc);
        auto mask = static_cast<std::uint32_t>(
            std::strtoul(c4(t, "recovery_unmet_mask").c_str(), nullptr, 16));
        std::printf("\033[H\033[2Jmode %s  epoch %s  lease_remaining_ns %s\nlatch %s  unmet %s\n",
                    c4(t, "mode").c_str(), c4(t, "epoch").c_str(),
                    c4(t, "lease_remaining_ns").c_str(), c4(t, "latch_cause").c_str(),
                    unmet_names(mask).empty() ? "-" : unmet_names(mask).c_str());
        std::vector<std::pair<unsigned long long, std::string>> counts;
        std::size_t at = 0;
        while ((at = t.find("count.", at)) != std::string::npos) {
            auto end = t.find('\n', at);
            auto line = t.substr(at + 6, end - at - 6);
            auto eq = line.find('=');
            counts.emplace_back(std::strtoull(line.c_str() + eq + 1, nullptr, 10),
                                line.substr(0, eq));
            at = end;
        }
        std::sort(counts.rbegin(), counts.rend());
        for (std::size_t i = 0; i < counts.size() && i < 10; ++i)
            std::printf("  %-28s %llu\n", counts[i].second.c_str(), counts[i].first);
        std::fflush(stdout);
        sleep_ms(interval);
    }
    return 0;
}

// ---- recover (C3) ----
int cmd_recover() {
    RecoverRequest r;
    r.request_id = static_cast<std::uint64_t>(now_ns());
    std::array<std::uint8_t, sizeof(RecoverReply)> buf{};
    std::array<std::uint8_t, sizeof(RecoverRequest)> w{};
    encode(r, w);
    std::memcpy(buf.data(), w.data(), w.size());
    int open_err = EOK;
    int rc = bounded_devctl("/dev/guard/gate", DCMD_GUARD_RECOVER, buf.data(), buf.size(), kC3Ms,
                            open_err);
    // D22: a denied secpol connect and the gate's uid check both give EPERM; say which.
    if (rc == EPERM && open_err == EPERM)
        return error("recover", "open /dev/guard/gate: Operation not permitted (connect denied)",
                     3);
    if (rc == EPERM)
        return error("recover", "Operation not permitted (gate uid check)", 3);
    if (rc != EOK)
        return errno_error("recover", rc);
    RecoverReply reply;
    if (decode(buf.data(), buf.size(), reply) != DecodeError::none)
        return error("recover", "bad reply", 6);
    const char *modes[] = {"INIT", "LATCHED", "READY", "MOVING", "STOPPING", "STOPPED"};
    const char *mode = reply.gate_mode < 6 ? modes[reply.gate_mode] : "INIT";
    std::string t;
    read_text("/dev/guard/status", t);
    if (reply.reason == static_cast<std::uint16_t>(Reason::GATE_RECOVERED))
        return result(std::string("recovered: mode=") + mode + " epoch=" + c4(t, "epoch"));
    if (reply.reason == static_cast<std::uint16_t>(Reason::GATE_RECOVERY_UNMET))
        return refused("unmet: " + unmet_names(reply.unmet_mask));
    return refused(std::string("not latched: mode=") + mode);
}

// On a secpol image a process guardctl starts gets its GUARD type (image/secpol, #46).
std::vector<std::string> typed(const char *type, std::vector<std::string> argv) {
    if (access("/system/etc/guard-secpol", R_OK) != 0)
        return argv;
    argv.insert(argv.begin(), {"on", "-T", type});
    return argv;
}

// ---- inject ----
// slay exits with the number of processes it signalled: 0 means none matched.
int slay(const char *sig, const char *name) {
    pid_t p = fork();
    if (p < 0)
        return errno;
    if (p == 0) {
        execlp("slay", "slay", "-f", "-Q", "-s", sig, name, static_cast<char *>(nullptr));
        _exit(127);
    }
    int st = 0;
    waitpid(p, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) == 127)
        return ENOENT;
    return WEXITSTATUS(st) >= 1 ? EOK : ESRCH;
}

int run(const std::vector<std::string> &argv, bool detach) {
    pid_t p = fork();
    if (p < 0)
        return errno;
    if (p == 0) {
        if (detach) {
            setsid();
            int null = open("/dev/null", O_RDWR);
            dup2(null, 0);
            dup2(null, 1);
            dup2(null, 2);
        }
        std::vector<char *> a;
        for (const auto &s : argv)
            a.push_back(const_cast<char *>(s.c_str()));
        a.push_back(nullptr);
        execvp(a[0], a.data());
        _exit(127);
    }
    if (detach)
        return EOK;
    int st = 0;
    waitpid(p, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0     ? EOK
           : WIFEXITED(st) && WEXITSTATUS(st) == 127 ? ENOENT
                                                     : EIO;
}

int cmd_inject(unsigned fault, long duration, long long param) {
    char ok[64];
    const auto t_inject = now_ns(); // before acting, so the event precedes its effects
    std::snprintf(ok, sizeof ok, "injected F%u at t_ns=%lld", fault,
                  static_cast<long long>(t_inject));
    int rc = EOK;
    // Process faults act on real processes: test image only (guard-hog marks it).
    if ((fault == 2 || fault == 3 || fault == 7) && access("/system/bin/guard-hog", X_OK) != 0)
        return error("inject", "process faults need the test image (run-demo.sh --test)", 5);
    switch (fault) {
    case 2: // F2: delayed authorization, guard-hog at 32 (REF-PRIORITIES)
        // One hog per CPU, each pinned, so the priority-32 load covers the whole machine.
        for (unsigned cpu = 0; cpu < _syspage_ptr->num_cpu && cpu < 32 && rc == EOK; ++cpu)
            rc = run(typed("hog_t", {"/system/bin/guard-hog", "32",
                                     std::to_string(duration > 0 ? duration : 400), "-c",
                                     std::to_string(cpu)}),
                     true);
        break;
    case 3: // F3: supervisor killed
        rc = slay("SIGKILL", "guard-supervisor");
        break;
    case 7: { // F7: stop supervisor A, start B (new epoch), continue A later
        if ((rc = slay("SIGSTOP", "guard-supervisor")) != EOK)
            break;
        if ((rc = run(typed("supervisor_t", {"/system/bin/guard-supervisor"}), true)) != EOK)
            break;
        long ms = duration > 0 ? duration : 2000;
        rc = run({"/bin/sh", "-c",
                  "sleep " + std::to_string(ms / 1000) + "." + std::to_string(ms % 1000 / 100) +
                      std::to_string(ms % 100 / 10) + std::to_string(ms % 10) +
                      "; slay -f -Q -s SIGCONT guard-supervisor"},
                 true);
        break;
    }
    default: {
        WorldInject in;
        in.fault = static_cast<std::uint8_t>(fault);
        in.duration_ms = static_cast<std::uint32_t>(duration > 0 ? duration : 0);
        in.param = param;
        int open_err = EOK;
        rc = world(DCMD_SIM_WORLD_INJECT, in, open_err);
        if (open_err != EOK)
            return errno_error("inject", open_err);
    }
    }
    if (rc != EOK)
        return errno_error("inject", rc);
    if (fault == 2 || fault == 3 || fault == 7) {
        // C8 fault event at the moment guardctl acted (the sim skips process faults).
        qnx::TlmWriter tlm;
        char payload[96];
        std::snprintf(payload, sizeof payload,
                      "\"fault\":\"F%u\",\"phase\":\"start\",\"detail\":\"guardctl inject\"",
                      fault);
        if (tlm.open("guardctl", "harness", true) != EOK || !tlm.emit(t_inject, "fault", payload))
            std::fprintf(stderr, "guardctl: inject: fault event not written to telemetry\n");
    }
    return result(ok);
}

// ---- scenario (C7) ----
int cmd_load(const std::string &path, std::uint64_t seed, unsigned mode, unsigned mask) {
    WorldLoad l;
    l.seed = seed;
    l.mode = static_cast<std::uint8_t>(mode);
    l.ablation_mask = static_cast<std::uint8_t>(mask);
    if (path.size() >= l.scenario_path.size() || path.empty() || path[0] != '/')
        return error("scenario load", "path must be absolute and shorter than 96 bytes", 2);
    std::memcpy(l.scenario_path.data(), path.c_str(), path.size());
    int open_err = EOK;
    int rc = world(DCMD_SIM_WORLD_LOAD, l, open_err);
    if (open_err != EOK)
        return errno_error("scenario load", open_err);
    if (rc != EOK) // the scenario file is missing or invalid (guardctl.md: exit 6)
        return error("scenario load", std::strerror(rc), rc == EPERM ? 3 : rc == ETIMEDOUT ? 4 : 6);
    std::string t;
    read_text("/dev/sim/world", t);
    char id[16] = "?";
    qnx::kv_get(t.data(), t.size(), "scenario", id, sizeof id);
    const char *modes[] = {"B0", "B1", "B2"};
    return result(std::string("loaded ") + id + " mode=" + modes[mode] +
                  " seed=" + std::to_string(seed));
}

int cmd_pause(bool pause) {
    WorldPause p;
    p.pause = pause ? 1 : 0;
    int open_err = EOK;
    int rc = world(DCMD_SIM_WORLD_PAUSE, p, open_err);
    const char *name = pause ? "scenario stop" : "scenario start";
    if (rc != EOK)
        return errno_error(name, open_err != EOK ? open_err : rc);
    if (!pause) {
        // A latched gate (BOOT, or the end of an earlier run) is recovered once at start, as
        // the host harness does at boot; the gate still checks every condition.
        std::string t;
        if (read_text("/dev/guard/status", t) == EOK && c4(t, "mode") == "LATCHED") {
            sleep_ms(100); // standstill ticks
            RecoverRequest r;
            r.request_id = static_cast<std::uint64_t>(now_ns());
            std::array<std::uint8_t, sizeof(RecoverReply)> buf{};
            std::array<std::uint8_t, sizeof(RecoverRequest)> w{};
            encode(r, w);
            std::memcpy(buf.data(), w.data(), w.size());
            int oe = EOK;
            RecoverReply reply;
            if (bounded_devctl("/dev/guard/gate", DCMD_GUARD_RECOVER, buf.data(), buf.size(), kC3Ms,
                               oe) != EOK ||
                decode(buf.data(), buf.size(), reply) != DecodeError::none ||
                reply.reason != static_cast<std::uint16_t>(Reason::GATE_RECOVERED))
                std::fprintf(stderr, "guardctl: scenario start: gate still LATCHED\n");
        }
    }
    return result(pause ? "stopped" : "started");
}

bool parse_u64(const char *s, unsigned long long max, unsigned long long &out) {
    if (s == nullptr || *s == '\0' || *s == '-')
        return false;
    char *end = nullptr;
    errno = 0;
    out = std::strtoull(s, &end, 10);
    return errno == 0 && *end == '\0' && out <= max;
}
int usage() {
    std::fputs(kUsage, stderr);
    if (g_json)
        std::printf("{\"ok\":false,\"error\":\"usage\",\"exit\":2}\n");
    return 2;
}
} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--json"))
            g_json = true;
        else
            args.emplace_back(argv[i]);
    }
    if (args.empty())
        return usage();
    const std::string &cmd = args[0];
    if (cmd == "--help" || cmd == "-h") {
        std::fputs(kUsage, stdout);
        return 0;
    }
    if (cmd == "--version")
        return result("guardctl 1.0 (contracts-v1.2)");
    if (cmd == "status")
        return args.size() == 1 ? cmd_status(g_json) : usage();
    if (cmd == "watch") {
        unsigned long long ms = 200;
        if (args.size() == 3 && args[1] == "--interval-ms") {
            if (!parse_u64(args[2].c_str(), 5000, ms) || ms < 50)
                return usage();
        } else if (args.size() != 1) {
            return usage();
        }
        return cmd_watch(static_cast<long>(ms));
    }
    if (cmd == "recover")
        return args.size() == 1 ? cmd_recover() : usage();
    if (cmd == "inject") {
        if (args.size() < 2 || args[1].size() != 2 || args[1][0] != 'F' || args[1][1] < '1' ||
            args[1][1] > '8')
            return usage();
        unsigned long long duration = 0, param = 0;
        for (std::size_t i = 2; i < args.size(); i += 2) {
            if (i + 1 >= args.size())
                return usage();
            if (args[i] == "--duration-ms" && parse_u64(args[i + 1].c_str(), 60000, duration))
                continue;
            if (args[i] == "--param" && parse_u64(args[i + 1].c_str(), 1000000, param))
                continue;
            return usage();
        }
        return cmd_inject(static_cast<unsigned>(args[1][1] - '0'), static_cast<long>(duration),
                          static_cast<long long>(param));
    }
    if (cmd == "scenario" && args.size() >= 2) {
        if (args[1] == "start" && args.size() == 2)
            return cmd_pause(false);
        if (args[1] == "stop" && args.size() == 2)
            return cmd_pause(true);
        if (args[1] == "load" && args.size() >= 3) {
            unsigned long long seed = 0;
            unsigned mode = 2, mask = 0;
            for (std::size_t i = 3; i < args.size(); i += 2) {
                if (i + 1 >= args.size())
                    return usage();
                const auto &k = args[i], &v = args[i + 1];
                if (k == "--seed" && parse_u64(v.c_str(), ~0ULL, seed))
                    continue;
                if (k == "--mode" && (v == "B0" || v == "B1" || v == "B2")) {
                    mode = static_cast<unsigned>(v[1] - '0');
                    continue;
                }
                if (k == "--ablate") {
                    const char *names[] = {"freshness", "stopping", "expiry", "epoch"};
                    std::size_t at = 0;
                    while (at <= v.size()) {
                        auto end = v.find(',', at);
                        auto item =
                            v.substr(at, end == std::string::npos ? std::string::npos : end - at);
                        unsigned bit = 4;
                        for (unsigned b = 0; b < 4; ++b)
                            if (item == names[b])
                                bit = b;
                        if (bit == 4)
                            return usage();
                        mask |= 1u << bit;
                        if (end == std::string::npos)
                            break;
                        at = end + 1;
                    }
                    continue;
                }
                return usage();
            }
            return cmd_load(args[2], seed, mode, mask);
        }
    }
    return usage();
}
