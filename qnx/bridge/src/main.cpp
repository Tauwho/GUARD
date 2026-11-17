// guard-bridge (B5, #42): C8 telemetry over TCP 7700. Reads the per-process shared-memory
// rings (sim, gate, supervisor, planner; qnx/common tlm.hpp) read-only, merges them in time
// order and sends JSONL to connected clients. Send-only: bytes from clients are ignored.
//   guard-bridge [--port N] [--keep-root]
// Priority 8, uid guard_bridge (1105). It never connects to another GUARD process.
#include "guard/qnx/priv.hpp"
#include "guard/qnx/sched.hpp"
#include "guard/qnx/tlm.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace guard;
constexpr int kPrio = 8, kMaxClients = 4;
constexpr std::int64_t kPollNs = 20'000'000, kHoldNs = 200'000'000;
// Ring names; "guardctl" carries the process-fault events (src "harness") of guardctl inject.
const char *kSources[] = {"sim", "gate", "supervisor", "planner", "guardctl"};
constexpr int kRings = 5;

std::int64_t now_ns() {
    struct timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

struct Line {
    std::int64_t t = 0;
    std::uint64_t order = 0;
    std::string text;
};

// Numbers and strings from a C8 line; only for the few fields the bridge needs.
bool find_num(const std::string &s, const char *key, double &out) {
    std::string k = std::string("\"") + key + "\":";
    auto at = s.find(k);
    if (at == std::string::npos)
        return false;
    out = std::strtod(s.c_str() + at + k.size(), nullptr);
    return true;
}
std::string find_str(const std::string &s, const char *key) {
    std::string k = std::string("\"") + key + "\":\"";
    auto at = s.find(k);
    if (at == std::string::npos)
        return {};
    auto end = s.find('"', at + k.size());
    return end == std::string::npos ? std::string() : s.substr(at + k.size(), end - at - k.size());
}
bool is_kind(const std::string &s, const char *kind) {
    return s.find(std::string("\"kind\":\"") + kind + "\"") != std::string::npos;
}

struct Bridge {
    qnx::TlmReader rings[kRings];
    std::vector<Line> pending;
    std::uint64_t order = 0, seq = 0, sent = 0, client_drops = 0;
    std::int64_t last_t = 0;
    int listen_fd = -1;
    int clients[kMaxClients] = {-1, -1, -1, -1};
    // Per-run tallies for run_end (docs/decisions/B.md: the bridge completes run_end).
    std::uint64_t proposals = 0, authorized = 0, rejected = 0, unverifiable = 0, latches = 0;
    std::string gate_mode = "INIT";
} b;

void send_all(const std::string &line) {
    for (int &c : b.clients) {
        if (c < 0)
            continue;
        ssize_t n = send(c, line.data(), line.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n == static_cast<ssize_t>(line.size()))
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            ++b.client_drops; // slow reader: skip the line, never block
            continue;
        }
        close(c); // gone, or a partial write that would break the JSONL framing
        c = -1;
    }
}

// Emits one line with t_ns raised to keep the stream non-decreasing.
void out(std::string text, std::int64_t t) {
    if (t < b.last_t) {
        // Rewrite the t_ns field ({"v":1,"t_ns":<n>,...}): a late line keeps its place.
        auto at = text.find("\"t_ns\":");
        auto end = text.find(',', at);
        if (at != std::string::npos && end != std::string::npos)
            text.replace(at + 7, end - at - 7, std::to_string(b.last_t));
        t = b.last_t;
    }
    b.last_t = t;
    text.push_back('\n');
    send_all(text);
    ++b.sent;
}

std::string bridge_line(std::int64_t t, const char *kind, const std::string &payload) {
    char head[160];
    std::snprintf(head, sizeof head,
                  "{\"v\":1,\"t_ns\":%" PRId64 ",\"src\":\"bridge\",\"seq\":%" PRIu64
                  ",\"kind\":\"%s\",\"payload\":{",
                  t, b.seq++, kind);
    return head + payload + "}}";
}

void tally(const std::string &s) {
    if (is_kind(s, "run_start")) {
        b.proposals = b.authorized = b.rejected = b.unverifiable = b.latches = 0;
    } else if (is_kind(s, "decision")) {
        ++b.proposals;
        auto v = find_str(s, "verdict");
        b.authorized += v == "AUTHORIZED";
        b.rejected += v == "REJECTED";
        b.unverifiable += v == "UNVERIFIABLE";
    } else if (is_kind(s, "gate_transition")) {
        b.gate_mode = find_str(s, "to");
        b.latches += b.gate_mode == "LATCHED";
    }
}

// The sim's run_end carries its own fields; the C1 counters, latches and a non-GOAL final
// state come from what this bridge forwarded during the run.
std::string complete_run_end(const std::string &s, std::int64_t t) {
    double contacts = 0, breaches = 0, margin = 0, duration = 0;
    find_num(s, "contacts", contacts);
    find_num(s, "zone_breaches", breaches);
    find_num(s, "sim_duration_ns", duration);
    bool has_margin = find_num(s, "min_margin_m", margin);
    std::string final_state = find_str(s, "final_state") == "GOAL" ? "GOAL" : b.gate_mode;
    char p[512];
    int n = std::snprintf(
        p, sizeof p,
        "\"final_state\":\"%s\",\"contacts\":%.0f,\"zone_breaches\":%.0f,\"proposals\":%" PRIu64
        ",\"authorized\":%" PRIu64 ",\"rejected\":%" PRIu64 ",\"unverifiable\":%" PRIu64
        ",\"latches\":%" PRIu64,
        final_state.c_str(), contacts, breaches, b.proposals, b.authorized, b.rejected,
        b.unverifiable, b.latches);
    if (has_margin)
        n += std::snprintf(p + n, sizeof p - n, ",\"min_margin_m\":%.17g", margin);
    if (s.find("\"sim_duration_ns\":") != std::string::npos)
        std::snprintf(p + n, sizeof p - n, ",\"sim_duration_ns\":%.0f", duration);
    // Keep the sim's envelope (src, seq, run_id); only the payload is completed.
    auto at = s.find("\"payload\":{");
    (void)t;
    return at == std::string::npos ? bridge_line(t, "run_end", p)
                                   : s.substr(0, at) + "\"payload\":{" + p + "}}";
}

void poll_rings(std::int64_t now) {
    char buf[qnx::kTlmLine];
    for (int i = 0; i < kRings; ++i) {
        auto &r = b.rings[i];
        if (!r.is_open() && r.open(kSources[i]) != EOK)
            continue;
        std::size_t n = 0;
        while (r.next(buf, sizeof buf, n)) {
            Line l;
            l.text.assign(buf, n);
            double t = 0;
            find_num(l.text, "t_ns", t);
            l.t = static_cast<std::int64_t>(t);
            l.order = b.order++;
            b.pending.push_back(std::move(l));
        }
        if (auto d = r.take_dropped(); d && i < 4) { // drop.ring names the four processes
            Line l;
            l.t = now - kHoldNs;
            l.order = b.order++;
            l.text = bridge_line(l.t, "drop",
                                 std::string("\"ring\":\"") + kSources[i] +
                                     "\",\"dropped\":" + std::to_string(d));
            b.pending.push_back(std::move(l));
        }
    }
    // Hold each line for 200 ms so slower writers catch up, then send in time order.
    std::stable_sort(b.pending.begin(), b.pending.end(), [](const Line &a, const Line &c) {
        return a.t != c.t ? a.t < c.t : a.order < c.order;
    });
    std::size_t done = 0;
    for (; done < b.pending.size() && b.pending[done].t <= now - kHoldNs; ++done) {
        const auto &l = b.pending[done];
        tally(l.text);
        if (is_kind(l.text, "run_end") && l.text.find("\"src\":\"sim\"") != std::string::npos)
            out(complete_run_end(l.text, std::max(l.t, b.last_t)), l.t);
        else
            out(l.text, l.t);
    }
    b.pending.erase(b.pending.begin(), b.pending.begin() + static_cast<long>(done));
    if (b.pending.size() > 20000) { // keep memory bounded; the dropped lines still count
        for (std::size_t i = 0; i < 10000; ++i)
            tally(b.pending[i].text);
        b.pending.erase(b.pending.begin(), b.pending.begin() + 10000);
    }
}

void accept_clients() {
    for (;;) {
        int c = accept(b.listen_fd, nullptr, nullptr);
        if (c < 0)
            return;
        fcntl(c, F_SETFL, fcntl(c, F_GETFL) | O_NONBLOCK);
        bool placed = false;
        for (int &slot : b.clients)
            if (slot < 0 && !placed) {
                slot = c;
                placed = true;
            }
        if (!placed)
            close(c);
        else
            std::printf("guard-bridge: client connected\n");
    }
}

void drain_input() { // send-only: read and discard anything a client writes
    char junk[256];
    for (int &c : b.clients)
        if (c >= 0) {
            ssize_t n = recv(c, junk, sizeof junk, MSG_DONTWAIT);
            if (n == 0) {
                close(c);
                c = -1;
            }
        }
}

int fail(const char *what, int rc) {
    std::fprintf(stderr, "guard-bridge: %s: %s\n", what, std::strerror(rc));
    return 1;
}
} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    int port = 7700;
    bool keep_root = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--port") && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "--keep-root")) {
            keep_root = true;
        } else {
            std::fprintf(stderr, "usage: guard-bridge [--port N] [--keep-root]\n");
            return 2;
        }
    }
    if (port < 1 || port > 65535) {
        std::fprintf(stderr, "usage: guard-bridge [--port N] [--keep-root]\n");
        return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    b.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(b.listen_fd, F_SETFL, fcntl(b.listen_fd, F_GETFL) | O_NONBLOCK);
    int one = 1;
    setsockopt(b.listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(static_cast<std::uint16_t>(port));
    a.sin_addr.s_addr = htonl(INADDR_ANY); // QEMU user net forwards host 127.0.0.1:7700 here
    if (b.listen_fd < 0 || bind(b.listen_fd, reinterpret_cast<sockaddr *>(&a), sizeof a) != 0 ||
        listen(b.listen_fd, kMaxClients) != 0)
        return fail("listen", errno);
    int rc;
    if ((rc = qnx::set_fifo(kPrio)) != EOK)
        return fail("SCHED_FIFO", rc);
    if (!keep_root && (rc = qnx::drop_and_lock(1105, 1105, nullptr, 0)) != EOK)
        return fail("drop to guard_bridge (1105)", rc);
    std::printf("guard-bridge: ready on TCP %d, uid %d\n", port, static_cast<int>(geteuid()));
    for (;;) {
        accept_clients();
        drain_input();
        poll_rings(now_ns());
        struct timespec ts {
            0, kPollNs
        };
        nanosleep(&ts, nullptr);
    }
}
