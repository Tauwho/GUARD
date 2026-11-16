#include "guard/route.hpp"
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
    CHECK(guard::check_route(request, source, current, {}, {}, 5'000'000'001) == 205);
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
