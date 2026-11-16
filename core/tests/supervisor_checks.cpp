#include "guard/supervisor.hpp"
#include <doctest/doctest.h>

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
