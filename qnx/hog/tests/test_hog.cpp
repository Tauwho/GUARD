// On-target test for guard-hog. Run as root on QSTI.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "guard/hog.hpp"
#include "guard/qnx/sched.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <doctest/doctest.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <unistd.h>

using guard::hog::Options;

namespace {
int parse_args(std::initializer_list<const char *> args, Options &opt) {
    char *argv[8] = {};
    int argc = 0;
    for (const char *a : args) {
        argv[argc++] = const_cast<char *>(a);
    }
    return guard::hog::parse(argc, argv, opt);
}

std::atomic<std::uint64_t> g_count{0};
std::atomic<bool> g_stop{false};
std::atomic<int> g_hog_state{0}; // 0 before, 1 running, 2 done
std::uint64_t g_at_start = 0, g_at_end = 0;

void pin_cpu0() { ThreadCtl(_NTO_TCTL_RUNMASK, reinterpret_cast<void *>(1u)); }

void *low_counter(void *) {
    pin_cpu0();
    guard::qnx::set_fifo(10);
    while (!g_stop) {
        g_count++;
    }
    return nullptr;
}

void *hog_thread(void *) {
    Options opt;
    opt.prio = 32;
    opt.duration_ms = 300;
    opt.cpu = 0;
    pin_cpu0();
    guard::qnx::set_fifo(32);
    g_at_start = g_count.load();
    g_hog_state = 1;
    guard::hog::run(opt);
    g_at_end = g_count.load();
    g_hog_state = 2;
    return nullptr;
}
} // namespace

TEST_CASE("parse accepts prio, ms and an optional cpu") {
    Options opt;
    REQUIRE(parse_args({"guard-hog", "32", "400"}, opt) == EOK);
    CHECK(opt.prio == 32);
    CHECK(opt.duration_ms == 400);
    CHECK(opt.cpu == -1);
    REQUIRE(parse_args({"guard-hog", "32", "400", "-c", "1"}, opt) == EOK);
    CHECK(opt.cpu == 1);
}

TEST_CASE("parse rejects bad arguments") {
    Options opt;
    CHECK(parse_args({"guard-hog"}, opt) == EINVAL);
    CHECK(parse_args({"guard-hog", "64", "400"}, opt) == EINVAL);
    CHECK(parse_args({"guard-hog", "0", "400"}, opt) == EINVAL);
    CHECK(parse_args({"guard-hog", "32", "0"}, opt) == EINVAL);
    CHECK(parse_args({"guard-hog", "32", "x"}, opt) == EINVAL);
    CHECK(parse_args({"guard-hog", "32", "400", "-x", "1"}, opt) == EINVAL);
}

TEST_CASE("run rejects a zero duration") {
    Options opt;
    CHECK(guard::hog::run(opt) == EINVAL);
}

TEST_CASE("hog at 32 starves a priority-10 thread on the same CPU, then lets it run") {
    // Keep this thread off CPU 0 and above the counter, or it starves too.
    REQUIRE(ThreadCtl(_NTO_TCTL_RUNMASK, reinterpret_cast<void *>(2u)) != -1);
    REQUIRE(guard::qnx::set_fifo(20) == EOK);
    g_count = 0;
    g_stop = false;
    pthread_t low, hog;
    REQUIRE(pthread_create(&low, nullptr, low_counter, nullptr) == EOK);
    while (g_count.load() < 1000) {
        usleep(1000); // the counter is running
    }
    REQUIRE(pthread_create(&hog, nullptr, hog_thread, nullptr) == EOK);
    pthread_join(hog, nullptr);
    CHECK(g_hog_state.load() == 2);
    CHECK(g_at_end == g_at_start); // no progress at priority 10 while the hog ran
    usleep(50 * 1000);
    CHECK(g_count.load() > g_at_end); // progress resumes after the hog
    g_stop = true;
    pthread_join(low, nullptr);
}
