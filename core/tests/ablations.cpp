#include "guard/gate_fsm.hpp"
#include "guard/supervisor.hpp"
#include <doctest/doctest.h>
#include <memory>

namespace {
void ready(guard::Gate &gate) {
    guard::RegisterReply registration;
    REQUIRE(gate.register_supervisor(registration));
    for (unsigned i = 0; i < 5; ++i)
        gate.tick(i * 10'000'000, 0);
    guard::RecoverRequest request;
    request.request_id = 1;
    REQUIRE(gate.recover(request, {true, true, true}).reason == 512);
}
guard::Observation observation() {
    guard::Observation value;
    value.obs_id = value.seq = value.sensor_ok = value.map_version = 1;
    value.v_meas = 0.4;
    return value;
}
guard::Proposal proposal() {
    guard::Proposal value;
    value.proposal_id = value.seq = value.planner_id = 1;
    value.based_on = {1, 1, 0, 1, 0};
    value.v_cmd = 0.4;
    return value;
}
} // namespace
TEST_CASE("freshness ablation removes provenance checks but retains sensor health") {
    guard::Config config;
    config.ablate_freshness = true;
    // Heap: two Supervisors (~300 KB each) overflow the QNX main-thread stack.
    auto owned = std::make_unique<guard::Supervisor>(config);
    auto owned_control = std::make_unique<guard::Supervisor>();
    guard::Supervisor &supervisor = *owned, &control = *owned_control;
    REQUIRE(supervisor.observe(observation(), 0));
    REQUIRE(control.observe(observation(), 0));
    auto request = proposal();
    request.based_on.obs_id = 999;
    request.based_on.map_version = 2;
    CHECK_FALSE(control.evaluate(request, {}, 1).authorized);
    CHECK(supervisor.evaluate(request, {}, 1).authorized);
    ++request.seq;
    CHECK(supervisor.evaluate(request, {}, 200'000'000).primary_reason ==
          guard::Reason::UNV_SENSOR_STALE);
}
TEST_CASE("stopping ablation removes check 11 without removing speed checks") {
    guard::Config config;
    config.ablate_stopping = true;
    auto owned = std::make_unique<guard::Supervisor>(config);
    auto owned_control = std::make_unique<guard::Supervisor>();
    guard::Supervisor &supervisor = *owned, &control = *owned_control;
    auto source = observation();
    source.obstacle_count = 1;
    source.obstacles[0] = {0.6, 0, 0.3};
    REQUIRE(supervisor.observe(source, 0));
    REQUIRE(control.observe(source, 0));
    auto request = proposal();
    CHECK(control.evaluate(request, {}, 1).primary_reason == guard::Reason::REJ_STOPPING_DISTANCE);
    CHECK(supervisor.evaluate(request, {}, 1).authorized);
    ++request.seq;
    request.v_cmd = 1.4;
    CHECK(supervisor.evaluate(request, {}, 2).primary_reason == guard::Reason::REJ_SPEED_ENVELOPE);
}
TEST_CASE("expiry and epoch ablations independently remove their gate checks") {
    for (bool expiry : {false, true}) {
        guard::Config config;
        config.ablate_expiry = expiry;
        config.ablate_epoch = !expiry;
        guard::Gate gate(config), control;
        ready(gate);
        ready(control);
        guard::Lease lease;
        lease.lease_id = lease.lease_seq = lease.proposal_id = 1;
        lease.epoch = expiry ? 1 : 2;
        lease.issued_ns = 50'000'000;
        lease.expires_ns = 300'000'000;
        lease.v_cmd = lease.v_cap = 0.4;
        const std::int64_t now = expiry ? 400'000'000 : 50'000'000;
        CHECK(control.lease(lease, now).reason == (expiry ? 503 : 501));
        CHECK(gate.lease(lease, now).reason == 500);
        CHECK(gate.tick(expiry ? 500'000'000 : 60'000'000, 0.4).v == 0.4);
        if (!expiry) {
            gate.tick(300'000'000, 0.4);
            CHECK(gate.status(300'000'000).mode == guard::GateMode::stopping);
        }
    }
}
