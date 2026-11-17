#include "guard/qnx/kv.hpp"
#include "guard/qnx/tlm.hpp"

#include <cstring>
#include <doctest/doctest.h>
#include <errno.h>
#include <string>
#include <sys/mman.h>

using namespace guard::qnx;

namespace {
std::string next_line(TlmReader &r) {
    char buf[kTlmLine];
    std::size_t n = 0;
    return r.next(buf, sizeof buf, n) ? std::string(buf, n) : std::string();
}
} // namespace

TEST_CASE("telemetry ring: lines arrive in order with per-src seq") {
    shm_unlink("/guard_tlm_test");
    TlmWriter w;
    REQUIRE(w.open("test") == EOK);
    CHECK(w.open("test") == EBUSY);
    TlmReader r;
    REQUIRE(r.open("test") == EOK);
    CHECK(next_line(r).empty());
    REQUIRE(w.emit(5, "fault", "\"fault\":\"F1\",\"phase\":\"start\""));
    REQUIRE(w.emit(7, "fault", "\"fault\":\"F1\",\"phase\":\"end\""));
    CHECK(next_line(r) == "{\"v\":1,\"t_ns\":5,\"src\":\"test\",\"seq\":0,\"kind\":\"fault\","
                          "\"payload\":{\"fault\":\"F1\",\"phase\":\"start\"}}");
    CHECK(next_line(r).find("\"seq\":1,") != std::string::npos);
    CHECK(next_line(r).empty());
    CHECK(r.dropped() == 0);
    shm_unlink("/guard_tlm_test");
}

TEST_CASE("telemetry ring: a full ring overwrites the oldest lines and the reader counts them") {
    shm_unlink("/guard_tlm_test");
    TlmWriter w;
    REQUIRE(w.open("test") == EOK);
    TlmReader r;
    REQUIRE(r.open("test") == EOK);
    for (std::size_t i = 0; i < kTlmSlots + 10; ++i)
        REQUIRE(w.emit(static_cast<std::int64_t>(i), "status", ""));
    std::string first = next_line(r);
    CHECK(first.find("\"seq\":10,") != std::string::npos);
    CHECK(r.take_dropped() == 10);
    CHECK(r.take_dropped() == 0);
    std::size_t rest = 0;
    while (!next_line(r).empty())
        ++rest;
    CHECK(rest == kTlmSlots - 1);
    shm_unlink("/guard_tlm_test");
}

TEST_CASE("telemetry ring: too-long line is refused; a restarted writer starts over") {
    shm_unlink("/guard_tlm_test");
    TlmReader r;
    CHECK(r.open("test") == ENOENT);
    std::string big(kTlmLine, 'x');
    {
        TlmWriter w;
        REQUIRE(w.open("test") == EOK);
        CHECK_FALSE(w.emit(1, "status", big.c_str()));
        CHECK(w.too_long() == 1);
        CHECK_FALSE(w.emit(-1, "status", ""));
        REQUIRE(w.emit(1, "status", ""));
        REQUIRE(w.emit(2, "status", ""));
        REQUIRE(r.open("test") == EOK);
        CHECK(!next_line(r).empty());
    }
    TlmWriter again;
    REQUIRE(again.open("test") == EOK);
    REQUIRE(again.emit(9, "status", ""));
    CHECK(next_line(r).find("\"t_ns\":9,") != std::string::npos);
    CHECK(next_line(r).empty());
    shm_unlink("/guard_tlm_test");
}

TEST_CASE("kv_get reads C4-style lines") {
    const char *t = "version: 1\nmode: READY\nscenario_path: /a/F0.yaml\nmodex: 2\n";
    char v[32];
    REQUIRE(kv_get(t, std::strlen(t), "mode", v, sizeof v));
    CHECK(std::string(v) == "READY");
    REQUIRE(kv_get(t, std::strlen(t), "scenario_path", v, sizeof v));
    CHECK(std::string(v) == "/a/F0.yaml");
    CHECK_FALSE(kv_get(t, std::strlen(t), "epoch", v, sizeof v));
    CHECK_FALSE(kv_get(t, std::strlen(t), "scenario_path", v, 5));
}
