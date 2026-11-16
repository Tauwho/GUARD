#include "guard/qnx/resmgr.hpp"
#include "guard/qnx/timer.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <devctl.h>
#include <doctest/doctest.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace guard::qnx;

namespace {
constexpr std::uint64_t kMs = 1000000u;
constexpr const char *kPath = "/dev/guard_test_rm";
#define DCMD_TEST_ECHO __DIOTF(_DCMD_MISC, 0x61, int)
#define DCMD_TEST_HOLD __DION(_DCMD_MISC, 0x62)
constexpr int kStopCode = _PULSE_CODE_MINAVAIL + 3;

// Test server state. Everything runs on the single dispatch thread.
struct Server {
    ResMgr rm;
    DelayedReplies<4> delayed;
    PulseTimer late;
    int late_code = 0;
    const void *late_ocb = nullptr;
    std::atomic<bool> stop{false};
    std::atomic<int> closes{0};
    std::atomic<int> unblocks{0};
};
Server *g; // handlers get no user pointer for io funcs

int io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, void *ocb) {
    int st = iofunc_devctl_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
    if (st != _RESMGR_DEFAULT) {
        return st;
    }
    if (msg->i.dcmd == DCMD_TEST_ECHO) {
        int *v = static_cast<int *>(_DEVCTL_DATA(msg->i));
        *v += 1;
        memset(&msg->o, 0, sizeof msg->o);
        msg->o.nbytes = sizeof *v;
        return _RESMGR_PTR(ctp, &msg->o, sizeof msg->o + sizeof *v);
    }
    if (msg->i.dcmd == DCMD_TEST_HOLD) {
        return g->delayed.save(ctp->rcvid, ocb) == EOK ? _RESMGR_NOREPLY : EAGAIN;
    }
    return ENOSYS;
}

int io_read(resmgr_context_t *ctp, io_read_t *msg, void *ocb) {
    int st = iofunc_read_verify(ctp, msg, static_cast<iofunc_ocb_t *>(ocb), nullptr);
    if (st != EOK) {
        return st;
    }
    if (g->delayed.save(ctp->rcvid, ocb) != EOK) {
        return EAGAIN;
    }
    g->late_ocb = ocb;
    g->late.arm_oneshot(20 * kMs);
    return _RESMGR_NOREPLY;
}

int on_late(message_context_t *, int, unsigned, void *) {
    static const char text[] = "late";
    g->delayed.reply(g->late_ocb, sizeof text - 1, text, sizeof text - 1);
    return 0;
}

int on_stop(message_context_t *, int, unsigned, void *) {
    g->stop = true;
    return 0;
}

int io_unblock(resmgr_context_t *ctp, io_pulse_t *msg, void *ocb) {
    if (g->delayed.unblock(ctp->rcvid, ETIMEDOUT)) {
        g->unblocks++;
        return _RESMGR_NOREPLY;
    }
    return iofunc_unblock_default(ctp, msg, static_cast<iofunc_ocb_t *>(ocb));
}

int io_close_ocb(resmgr_context_t *ctp, void *reserved, void *ocb) {
    g->delayed.drop(ocb);
    g->closes++;
    return iofunc_close_ocb_default(ctp, reserved, static_cast<iofunc_ocb_t *>(ocb));
}

void *dispatch_loop(void *) {
    while (!g->stop) {
        g->rm.run_once();
    }
    return nullptr;
}

// Starts the server thread on construction, stops it on destruction.
struct Running {
    pthread_t thread{};
    int stop_coid = -1;
    Running() {
        static Server server;
        g = &server;
        g->stop = false;
        static bool attached = false;
        if (!attached) {
            g->rm.io().devctl = io_devctl;
            g->rm.io().read = io_read;
            g->rm.io().unblock = io_unblock;
            g->rm.io().close_ocb = io_close_ocb;
            REQUIRE(g->rm.attach(kPath, 0666, _RESMGR_FLAG_SELF) == EOK);
            int coid = -1;
            REQUIRE(g->rm.self_coid(coid) == EOK);
            REQUIRE(g->rm.alloc_pulse(g->late_code, on_late, nullptr) == EOK);
            REQUIRE(g->rm.on_pulse(kStopCode, on_stop, nullptr) == EOK);
            REQUIRE(g->late.create(coid, g->late_code, 20) == EOK);
            attached = true;
        }
        REQUIRE(g->rm.self_coid(stop_coid) == EOK);
        REQUIRE(pthread_create(&thread, nullptr, dispatch_loop, nullptr) == EOK);
    }
    ~Running() {
        MsgSendPulse(stop_coid, 10, kStopCode, 0);
        pthread_join(thread, nullptr);
    }
};
} // namespace

TEST_CASE("DelayedReplies is fixed-capacity and tracks clients by OCB") {
    DelayedReplies<2> d;
    int a = 0, b = 0, c = 0;
    CHECK(d.save(101, &a) == EOK);
    CHECK(d.save(102, &b) == EOK);
    CHECK(d.save(103, &c) == EAGAIN);
    CHECK(d.pending() == 2);
    CHECK(d.reply(&c, 0, nullptr, 0) == ENOENT);
    d.drop(&a);
    CHECK(d.pending() == 1);
}

TEST_CASE("resmgr: __DIOTF devctl round trip") {
    Running running;
    int fd = open(kPath, O_RDWR);
    REQUIRE(fd != -1);
    int v = 41;
    CHECK(devctl(fd, DCMD_TEST_ECHO, &v, sizeof v, nullptr) == EOK);
    CHECK(v == 42);
    close(fd);
}

TEST_CASE("resmgr: blocked read replied later from a timer pulse") {
    Running running;
    int fd = open(kPath, O_RDONLY);
    REQUIRE(fd != -1);
    char buf[8] = {};
    CHECK(read(fd, buf, sizeof buf - 1) == 4);
    CHECK(std::strcmp(buf, "late") == 0);
    close(fd);
    CHECK(g->delayed.pending() == 0);
}

TEST_CASE("resmgr: client TimerTimeout on a held reply gets the server's ETIMEDOUT") {
    Running running;
    int fd = open(kPath, O_RDWR);
    REQUIRE(fd != -1);
    int before = g->unblocks;
    struct sigevent ev;
    SIGEV_UNBLOCK_INIT(&ev);
    std::uint64_t ns = 5 * kMs;
    TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_SEND | _NTO_TIMEOUT_REPLY, &ev, &ns, nullptr);
    CHECK(devctl(fd, DCMD_TEST_HOLD, nullptr, 0, nullptr) == ETIMEDOUT);
    CHECK(g->unblocks == before + 1);
    CHECK(g->delayed.pending() == 0);
    close(fd);
}

TEST_CASE("resmgr: kill -9 of a held client is answered and its OCB closes") {
    Running running;
    int closes_before = g->closes;
    pid_t child = fork();
    if (child == 0) {
        int fd = open(kPath, O_RDWR);
        devctl(fd, DCMD_TEST_HOLD, nullptr, 0, nullptr); // held until killed
        _exit(0);
    }
    REQUIRE(child > 0);
    for (int i = 0; i < 100 && g->delayed.pending() == 0; ++i) {
        usleep(10 * 1000);
    }
    REQUIRE(g->delayed.pending() == 1);
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0); // returns only once the server answered the unblock pulse
    for (int i = 0; i < 100 && g->closes == closes_before; ++i) {
        usleep(10 * 1000);
    }
    CHECK(g->closes > closes_before);
    CHECK(g->delayed.pending() == 0);
}
