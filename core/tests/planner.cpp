#include "guard/planner.hpp"
#include <doctest/doctest.h>

namespace {
guard::Observation observation(std::uint64_t id = 1, std::int64_t stamp = 0) {
    guard::Observation value;
    value.obs_id = value.seq = id;
    value.stamp_ns = stamp;
    value.map_version = value.sensor_ok = value.feed = 1;
    return value;
}
guard::MapSpec map() {
    guard::MapSpec value;
    value.goal = {10, 0, 0};
    return value;
}
} // namespace
TEST_CASE("planner cites its observation and respects normal proposal rate") {
    guard::Planner planner(map());
    guard::Proposal result;
    CHECK_FALSE(planner.propose(0, {}, result));
    REQUIRE(planner.observe(observation()));
    REQUIRE(planner.propose(0, {}, result));
    CHECK(result.based_on.obs_id == 1);
    CHECK(result.v_cmd == doctest::Approx(0.025));
    CHECK_FALSE(planner.propose(99'000'000, {}, result));
    CHECK(planner.propose(100'000'000, {}, result));
    CHECK(result.seq == 2);
}
TEST_CASE("lag selects a real historical source and cannot invent history") {
    guard::Planner planner(map());
    guard::PlannerOptions options;
    options.mode = guard::PlannerMode::lag;
    guard::Proposal result;
    REQUIRE(planner.observe(observation()));
    CHECK_FALSE(planner.propose(500'000'000, options, result));
    REQUIRE(planner.observe(observation(2, 600'000'000)));
    REQUIRE(planner.propose(600'000'000, options, result));
    CHECK(result.based_on.obs_id == 1);
    CHECK(result.based_on.obs_stamp_ns == 0);
}
TEST_CASE("flood uses requested cadence and replay repeats the old sequence") {
    guard::Planner planner(map());
    REQUIRE(planner.observe(observation()));
    guard::PlannerOptions options;
    options.mode = guard::PlannerMode::flood;
    options.rate_hz = 1000;
    guard::Proposal first, second;
    REQUIRE(planner.propose(0, options, first));
    REQUIRE(planner.propose(1'000'000, options, second));
    CHECK(second.seq == first.seq + 1);
    options.mode = guard::PlannerMode::replay;
    REQUIRE(planner.propose(2'000'000, options, first));
    CHECK(first.seq == second.seq);
    CHECK(first.created_ns == second.created_ns);
}
TEST_CASE("shortcut ignores waypoints while aggressive retains normal acceleration") {
    auto route = map();
    route.waypoint_count = 1;
    route.waypoints[0] = {0, 5};
    guard::Planner normal(route), shortcut(route), aggressive(map());
    REQUIRE(normal.observe(observation()));
    REQUIRE(shortcut.observe(observation()));
    REQUIRE(aggressive.observe(observation()));
    guard::Proposal result;
    REQUIRE(normal.propose(0, {}, result));
    CHECK(result.v_cmd == 0);
    CHECK(result.w_cmd > 0);
    guard::PlannerOptions options;
    options.mode = guard::PlannerMode::shortcut;
    REQUIRE(shortcut.propose(0, options, result));
    CHECK(result.v_cmd > 0);
    CHECK(result.w_cmd == 0);
    options.mode = guard::PlannerMode::aggressive;
    options.cruise_v = 1.2;
    REQUIRE(aggressive.propose(0, options, result));
    CHECK(result.v_cmd == doctest::Approx(0.025));
}
TEST_CASE("planner yields to nearby obstacles and stops at goal") {
    guard::Planner planner(map());
    auto value = observation();
    value.obstacle_count = 1;
    value.obstacles[0] = {0.5, 0, 0.3};
    REQUIRE(planner.observe(value));
    guard::Proposal result;
    REQUIRE(planner.propose(0, {}, result));
    CHECK(result.v_cmd == 0);
    value = observation(2, 100'000'000);
    value.x = 10;
    REQUIRE(planner.observe(value));
    CHECK(planner.at_goal());
    REQUIRE(planner.propose(100'000'000, {}, result));
    CHECK(result.v_cmd == 0);
    CHECK(result.w_cmd == 0);
}

TEST_CASE("shortcut retains planned map version while citing a fresh pose") {
    guard::Planner planner(map());
    REQUIRE(planner.observe(observation()));
    auto changed = observation(2, 100'000'000);
    changed.map_version = 2;
    REQUIRE(planner.observe(changed));
    guard::PlannerOptions options;
    options.mode = guard::PlannerMode::shortcut;
    guard::Proposal result;
    REQUIRE(planner.propose(100'000'000, options, result));
    CHECK(result.based_on.obs_id == 2);
    CHECK(result.based_on.obs_stamp_ns == 100'000'000);
    CHECK(result.based_on.map_version == 1);
}

TEST_CASE("aggressive yields only after a matching stopping-distance rejection") {
    guard::Planner planner(map());
    auto source = observation();
    source.v_meas = 0.8;
    source.obstacle_count = 1;
    source.obstacles[0] = {0.8, 0, 0.3};
    REQUIRE(planner.observe(source));
    guard::PlannerOptions options;
    options.mode = guard::PlannerMode::aggressive;
    options.cruise_v = 1.2;
    guard::Proposal first, retry;
    REQUIRE(planner.propose(0, options, first));
    CHECK(first.v_cmd == doctest::Approx(0.825));
    guard::Decision reply;
    reply.proposal_id = first.proposal_id + 1;
    reply.verdict = 2;
    reply.primary_reason = 212;
    planner.decision(reply);
    REQUIRE(planner.propose(100'000'000, options, retry));
    CHECK(retry.v_cmd == first.v_cmd);
    reply.proposal_id = retry.proposal_id;
    planner.decision(reply);
    REQUIRE(planner.propose(200'000'000, options, retry));
    CHECK(retry.v_cmd == 0);
    REQUIRE(planner.propose(300'000'000, options, retry));
    CHECK(retry.v_cmd == first.v_cmd);
}
