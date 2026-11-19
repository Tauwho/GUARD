#include "guard/scenario.hpp"
#include "guard/supervisor.hpp"
#include <doctest/doctest.h>
#include <string>

namespace {
guard::Observation observation(std::uint64_t id = 1, std::int64_t stamp = 0) {
    guard::Observation value;
    value.obs_id = value.seq = id;
    value.stamp_ns = stamp;
    value.map_version = value.sensor_ok = 1;
    value.v_meas = 0.4;
    return value;
}
guard::Proposal proposal(std::uint64_t sequence = 1) {
    guard::Proposal value;
    value.proposal_id = value.seq = sequence;
    value.planner_id = 1;
    value.based_on = {1, 1, 0, 1, 0};
    value.created_ns = 1;
    value.v_cmd = 0.4;
    return value;
}
} // namespace

TEST_CASE("valid recent source allows normal motion") {
    guard::Supervisor supervisor;
    REQUIRE(supervisor.observe(observation(), 0));
    auto result = supervisor.evaluate(proposal(), {}, 10'000'000);
    CHECK(result.authorized);
    CHECK(result.primary_reason == guard::Reason::OK_AUTHORIZED);
    CHECK(result.reasons_mask == 0);
}

TEST_CASE("representable acceleration ceiling is accepted without subtraction rounding") {
    guard::Supervisor supervisor;
    auto source = observation();
    source.v_meas = 0.045;
    REQUIRE(supervisor.observe(source, 0));
    auto request = proposal();
    request.v_cmd = source.v_meas + 0.5 / 20;
    CHECK(supervisor.evaluate(request, {}, 1).authorized);
    ++request.seq;
    request.v_cmd += 1e-6;
    CHECK(supervisor.evaluate(request, {}, 2).primary_reason == guard::Reason::REJ_ACCEL_ENVELOPE);
}

TEST_CASE("stale source is rejected while current observations remain fresh") {
    guard::Supervisor supervisor;
    REQUIRE(supervisor.observe(observation(1, 0), 0));
    REQUIRE(supervisor.observe(observation(2, 600'000'000), 600'000'000));
    auto result = supervisor.evaluate(proposal(), {}, 610'000'000);
    CHECK_FALSE(result.authorized);
    CHECK(result.primary_reason == guard::Reason::REJ_STALE_OBSERVATION);
    CHECK(result.reasons[7] == 0);
}

TEST_CASE("all checker reasons are recorded and unverifiable dominates") {
    guard::Supervisor supervisor;
    REQUIRE(supervisor.observe(observation(), 0));
    auto request = proposal();
    request.v_cmd = 1.4;
    request.based_on.obs_id = 999;
    auto result = supervisor.evaluate(request, {}, 1'000'000'000);
    CHECK(result.primary_reason == guard::Reason::UNV_SENSOR_STALE);
    CHECK(result.reasons[3] == static_cast<unsigned>(guard::Reason::REJ_UNKNOWN_OBSERVATION));
    CHECK(result.reasons[8] == static_cast<unsigned>(guard::Reason::REJ_SPEED_ENVELOPE));
}

TEST_CASE("source provenance uses recorded stamp and ignores per-feed sequence") {
    guard::Supervisor supervisor;
    REQUIRE(supervisor.observe(observation(), 0));
    auto request = proposal();
    request.based_on.obs_seq = 999;
    CHECK(supervisor.evaluate(request, {}, 10'000'000).authorized);
    request.seq = 2;
    request.based_on.obs_stamp_ns = 1;
    auto result = supervisor.evaluate(request, {}, 10'000'000);
    CHECK(result.primary_reason == guard::Reason::REJ_PROVENANCE_MISMATCH);
}

TEST_CASE("unknown evicted and superseded sources cannot authorize") {
    guard::Supervisor supervisor;
    for (unsigned i = 1; i <= 70; ++i)
        REQUIRE(supervisor.observe(observation(i, i * 1'000'000), i * 1'000'000));
    CHECK(supervisor.find(1) == nullptr);
    auto request = proposal();
    CHECK(supervisor.evaluate(request, {}, 71'000'000).primary_reason ==
          guard::Reason::REJ_UNKNOWN_OBSERVATION);
    request.seq = 2;
    request.based_on = {60, 60, 60'000'000, 1, 0};
    request.created_ns = 70'000'000;
    CHECK(supervisor.evaluate(request, {}, 71'000'000).primary_reason ==
          guard::Reason::REJ_SUPERSEDED_OBSERVATION);
}

TEST_CASE("replay and rolling rate limit are enforced independently") {
    guard::Config config;
    config.rate_limit_hz = 2;
    guard::Supervisor supervisor(config);
    REQUIRE(supervisor.observe(observation(), 0));
    CHECK(supervisor.evaluate(proposal(), {}, 10'000'000).authorized);
    CHECK(supervisor.evaluate(proposal(), {}, 11'000'000).primary_reason ==
          guard::Reason::REJ_SEQ_REPLAY);
    CHECK(supervisor.evaluate(proposal(2), {}, 12'000'000).primary_reason ==
          guard::Reason::REJ_RATE_LIMIT);
}

TEST_CASE("stopping model retains measured speed when proposal slows down") {
    auto current = observation();
    current.v_meas = 1.2;
    auto request = proposal();
    request.v_cmd = 0.2;
    const auto result = guard::stopping_check(current, request, {}, {});
    CHECK(result.required == doctest::Approx(1.056));
    CHECK(result.available >= result.required);
}

TEST_CASE("current geometry rejects insufficient clearance and occupied zones") {
    guard::Supervisor supervisor;
    auto current = observation();
    current.obstacle_count = 1;
    current.obstacles[0] = {0.9, 0, 0.3};
    REQUIRE(supervisor.observe(current, 0));
    CHECK(supervisor.evaluate(proposal(), {}, 10'000'000).primary_reason ==
          guard::Reason::REJ_STOPPING_DISTANCE);
    guard::Scene scene;
    scene.zone_count = 1;
    scene.zones[0] = {{{-1, -1}, {1, 1}}, true, true};
    auto result = supervisor.evaluate(proposal(2), scene, 10'000'000);
    CHECK(result.reasons[9] == static_cast<unsigned>(guard::Reason::REJ_RESTRICTED_ZONE));
}

TEST_CASE("schema and acceleration envelope reject invalid proposals") {
    guard::Supervisor supervisor;
    REQUIRE(supervisor.observe(observation(), 0));
    auto request = proposal();
    request.v_cmd = 0.6;
    CHECK(supervisor.evaluate(request, {}, 10'000'000).primary_reason ==
          guard::Reason::REJ_ACCEL_ENVELOPE);
    request.seq = 2;
    request.header.version = 99;
    CHECK(supervisor.evaluate(request, {}, 10'000'000).primary_reason == guard::Reason::REJ_SCHEMA);
}

TEST_CASE("worked example: 1.2 m/s needs 1.06 m of free arc, 1.61 m centre to obstacle") {
    const guard::Config config;
    auto current = observation();
    current.v_meas = 1.2;
    auto request = proposal();
    request.v_cmd = 1.2;
    const auto open = guard::stopping_check(current, request, {}, config);
    CHECK(open.required == doctest::Approx(1.056));
    CHECK(open.required + config.r_robot_m + config.margin_m == doctest::Approx(1.606));
    auto verdict = [&](double edge_distance) {
        const double radius = 0.3;
        auto source = current;
        source.obstacle_count = 1;
        source.obstacles[0] = {edge_distance + radius, 0, radius};
        guard::Supervisor supervisor;
        REQUIRE(supervisor.observe(source, 0));
        return supervisor.evaluate(request, {}, 10'000'000);
    };
    const auto clear = verdict(1.70);
    CHECK(clear.authorized);
    CHECK(clear.free_distance >= clear.required_distance);
    const auto blocked = verdict(1.60);
    CHECK(blocked.primary_reason == guard::Reason::REJ_STOPPING_DISTANCE);
    CHECK(blocked.free_distance < blocked.required_distance);
}

TEST_CASE("map version must match the current observation and scene") {
    guard::Supervisor supervisor;
    REQUIRE(supervisor.observe(observation(), 0));
    auto request = proposal();
    request.based_on.map_version = 2;
    auto result = supervisor.evaluate(request, {}, 10'000'000);
    CHECK(result.primary_reason == guard::Reason::REJ_MAP_VERSION);
    CHECK(result.reasons_mask == (std::uint64_t{1} << 6));
    request = proposal(2);
    guard::Scene scene;
    scene.map_version = 2;
    CHECK(supervisor.evaluate(request, scene, 10'000'000).primary_reason ==
          guard::Reason::REJ_MAP_VERSION);
    request = proposal(3);
    request.based_on.map_version = 2;
    request.v_cmd = 1.4;
    result = supervisor.evaluate(request, {}, 10'000'000);
    CHECK(result.primary_reason == guard::Reason::REJ_MAP_VERSION);
    CHECK(result.reasons_mask == ((std::uint64_t{1} << 6) | (std::uint64_t{1} << 8)));
    CHECK(result.reasons[8] == static_cast<unsigned>(guard::Reason::REJ_SPEED_ENVELOPE));
    CHECK(supervisor.evaluate(proposal(4), {}, 10'000'000).authorized);
}

TEST_CASE("over-bound zones and obstacles are refused at every input boundary") {
    auto source = observation();
    source.obstacle_count = 17;
    CHECK_FALSE(guard::valid(source));
    guard::Supervisor supervisor;
    CHECK_FALSE(supervisor.observe(source, 0));
    CHECK_FALSE(supervisor.evaluate(proposal(), {}, 10'000'000).authorized);
    REQUIRE(supervisor.observe(observation(), 0));
    guard::Scene scene;
    scene.zone_count = scene.zones.size() + 1;
    auto result = supervisor.evaluate(proposal(2), scene, 10'000'000);
    CHECK_FALSE(result.authorized);
    CHECK(result.primary_reason == guard::Reason::UNV_INTERNAL);
    scene = {};
    scene.obstacle_count = scene.obstacles.size() + 1;
    CHECK_FALSE(supervisor.evaluate(proposal(3), scene, 10'000'000).authorized);

    auto map_with = [](std::size_t zones) {
        std::string json =
            R"({"kind":"map","version":1,"id":"ward","size":[40,6],"walls":[],"zones":[)";
        for (std::size_t i = 0; i < zones; ++i)
            json += (i ? "," : "") + std::string(R"({"id":"z)") + std::to_string(i) +
                    R"(","kind":"restricted","rect":{"min":[1,1],"max":[2,2]},"active":true})";
        return json + R"(],"start":{"x":2,"y":2,"theta":0},"goal":{"x":38,"y":2,"theta":0}})";
    };
    guard::MapSpec map;
    CHECK(guard::load_map(map_with(32), map));
    CHECK(map.scene.zone_count == 32);
    CHECK_FALSE(guard::load_map(map_with(33), map));
    CHECK(map.scene.zone_count == 32);

    auto world_with = [](std::size_t obstacles) {
        guard::ScenarioSpec scenario;
        scenario.obstacle_count = obstacles;
        for (std::size_t i = 0; i < obstacles; ++i) {
            scenario.obstacles[i].id = {'o', static_cast<char>('a' + i)};
            scenario.obstacles[i].position = {static_cast<double>(i), 3};
        }
        return guard::World(guard::MapSpec{}, scenario);
    };
    auto full = world_with(16);
    REQUIRE(full.update(0));
    CHECK(full.scene().obstacle_count == 16);
    CHECK_FALSE(world_with(17).update(0));
}
