#include "guard/route.hpp"
#include "guard/supervisor.hpp"
#include <doctest/doctest.h>

namespace {
guard::Observation observation(std::int64_t stamp = 0) {
    guard::Observation value;
    value.obs_id = value.seq = value.sensor_ok = value.map_version = 1;
    value.stamp_ns = stamp;
    return value;
}
guard::RouteProposal route() {
    guard::RouteProposal value;
    value.request_id = value.route_id = value.observation_id = value.map_version = 1;
    value.waypoint_count = 1;
    value.waypoints[0] = {10, 0};
    return value;
}
} // namespace
TEST_CASE("route checker rejects an old map while an advisor is thinking") {
    auto source = observation();
    auto current = observation(1'000'000'000);
    current.map_version = 2;
    guard::Scene scene;
    scene.map_version = 2;
    CHECK(guard::check_route(route(), source, current, scene, {}, 1'000'000'000) == 207);
}
TEST_CASE("route age uses retained own provenance rather than the response creation clock") {
    auto request = route();
    request.created_ns = 9'000'000'000;
    auto source = observation(), current = observation(5'000'000'000);
    CHECK(guard::check_route(request, source, current, {}, {}, 5'000'000'000) == 0);
    CHECK(guard::check_route(request, source, current, {}, {}, 5'000'000'001) == 0);
    current.stamp_ns += 1;
    CHECK(guard::check_route(request, source, current, {}, {}, current.stamp_ns) == 205);
    current.stamp_ns -= 1;
    request.observation_stamp_ns = 1;
    CHECK(guard::check_route(request, source, current, {}, {}, 5'000'000'000) == 204);
}
TEST_CASE("route footprint checks whole segments including thin active restricted zones") {
    auto source = observation();
    guard::Scene scene;
    scene.zone_count = 1;
    scene.zones[0] = {{{4, -0.1}, {4.01, 0.1}}, true, true};
    CHECK(guard::check_route(route(), source, source, scene, {}, 0) == 210);
    scene.zones[0].active = false;
    CHECK(guard::check_route(route(), source, source, scene, {}, 0) == 0);
}
TEST_CASE("C9 route buffer loader is bounded and transactional") {
    guard::RouteProposal output;
    REQUIRE(guard::load_route(
        R"({"v":1,"type":"route","req_id":1,"route_id":2,"based_on":{"obs_id":1,"stamp_ns":0,"map_version":1},"waypoints":[[5,0]],"created_ns":0})",
        output));
    CHECK(output.route_id == 2);
    CHECK_FALSE(guard::load_route(R"({"v":1,"type":"route","req_id":1})", output));
    CHECK(output.route_id == 2);
    auto invalid = route();
    invalid.waypoint_count = 17;
    CHECK(guard::check_route(invalid, observation(), observation(), {}, {}, 0) == 200);
}

TEST_CASE("route provenance outlives C1 history and wraps at its independent capacity") {
    guard::Supervisor supervisor;
    for (unsigned i = 1; i <= 100; ++i) {
        auto sample = observation((i - 1) * 50'000'000LL);
        sample.obs_id = sample.seq = i;
        REQUIRE(supervisor.observe(sample, sample.stamp_ns));
    }
    CHECK(supervisor.find(1) == nullptr);
    CHECK(supervisor.admit_route(route(), {}, 4'950'000'000) == 0);
    auto sample = observation(5'000'000'000);
    sample.obs_id = sample.seq = 101;
    REQUIRE(supervisor.observe(sample, sample.stamp_ns));
    CHECK(supervisor.admit_route(route(), {}, sample.stamp_ns) == 203);
    auto request = route();
    request.observation_id = 2;
    request.observation_stamp_ns = 50'000'000;
    CHECK(supervisor.admit_route(request, {}, sample.stamp_ns) == 0);
    ++request.observation_stamp_ns;
    CHECK(supervisor.admit_route(request, {}, sample.stamp_ns) == 204);
}
TEST_CASE("route capacity rounds up and admission keeps map health and age checks") {
    guard::Config config;
    config.obs_rate_hz = 10;
    config.route_max_age_ms = 450; // ceil(4.5) = 5 sources, independent of C1's 64.
    guard::Supervisor supervisor(config);
    for (unsigned i = 1; i <= 5; ++i) {
        auto sample = observation((i - 1) * 100'000'000LL);
        sample.obs_id = sample.seq = i;
        REQUIRE(supervisor.observe(sample, sample.stamp_ns));
    }
    CHECK(supervisor.admit_route(route(), {}, 400'000'000) == 0);
    auto sample = observation(450'000'001);
    sample.obs_id = sample.seq = 6;
    REQUIRE(supervisor.observe(sample, sample.stamp_ns));
    CHECK(supervisor.find(1) != nullptr);
    CHECK(supervisor.admit_route(route(), {}, sample.stamp_ns) == 203);
    auto request = route();
    request.observation_id = 2;
    request.observation_stamp_ns = 100'000'000;
    guard::Scene scene;
    scene.map_version = 2;
    CHECK(supervisor.admit_route(request, scene, sample.stamp_ns) == 207);
    CHECK(supervisor.admit_route(request, {}, sample.stamp_ns + 150'000'001) == 300);
    auto invalid = sample;
    invalid.obs_id = invalid.seq = 7;
    invalid.sensor_ok = 0;
    invalid.stamp_ns += 100'000'000;
    REQUIRE(supervisor.observe(invalid, invalid.stamp_ns));
    request.observation_id = 7;
    request.observation_stamp_ns = invalid.stamp_ns;
    CHECK(supervisor.admit_route(request, {}, invalid.stamp_ns) == 300);
}

TEST_CASE("route ring retains age and health independently of planner supplied metadata") {
    guard::Supervisor supervisor;
    auto source = observation();
    source.sensor_ok = 0;
    REQUIRE(supervisor.observe(source, 0));
    auto current = observation(1'000'000'000);
    current.obs_id = current.seq = 2;
    REQUIRE(supervisor.observe(current, current.stamp_ns));
    CHECK(supervisor.admit_route(route(), {}, current.stamp_ns) == 204);
    auto rejected = current;
    rejected.obs_id = 1;
    CHECK_FALSE(supervisor.observe(rejected, current.stamp_ns));
    CHECK(supervisor.admit_route(route(), {}, current.stamp_ns) == 204);
    auto request = route();
    request.observation_id = 2;
    request.observation_stamp_ns = current.stamp_ns;
    current.obs_id = current.seq = 3;
    current.stamp_ns = 6'000'000'001;
    REQUIRE(supervisor.observe(current, current.stamp_ns));
    CHECK(supervisor.admit_route(request, {}, current.stamp_ns) == 205);
}
