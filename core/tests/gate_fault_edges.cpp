#include "guard/gate_fsm.hpp"
#include <doctest/doctest.h>

namespace {
void ready(guard::Gate &gate) {
    guard::RegisterReply reply;
    REQUIRE(gate.register_supervisor(reply));
    for (unsigned i = 0; i < 5; ++i)
        gate.tick(i * 10'000'000, 0);
    guard::RecoverRequest recover;
    recover.request_id = 1;
    REQUIRE(gate.recover(recover, {true, true, true}).reason == 512);
}
guard::Lease lease() {
    guard::Lease value;
    value.lease_id = value.lease_seq = value.proposal_id = value.epoch = 1;
    value.issued_ns = 50'000'000;
    value.expires_ns = 300'000'000;
    value.v_cmd = value.v_cap = 0.4;
    return value;
}
} // namespace
TEST_CASE("invalid lease metadata and clock input cannot consume a sequence") {
    guard::Gate gate;
    auto value = lease();
    CHECK(gate.lease(value, 50'000'000).reason == 504);
    ready(gate);
    value.header.version = 2;
    CHECK(gate.lease(value, 50'000'000).reason == 504);
    value = lease();
    CHECK(gate.lease(value, -1).reason == 504);
    CHECK(gate.lease(value, 30'000'000).reason == 504);
    CHECK(gate.lease(value, 45'000'000).reason == 504);
    CHECK(gate.status(50'000'000).lease_seq == 0);
    CHECK(gate.lease(value, 50'000'000).reason == 500);
}
TEST_CASE("revokes for another epoch or lease cannot stop the current lease") {
    guard::Gate gate;
    ready(gate);
    REQUIRE(gate.lease(lease(), 50'000'000).reason == 500);
    guard::Revoke revoke;
    revoke.epoch = 2;
    revoke.reason = 403;
    CHECK_FALSE(gate.revoke(revoke));
    revoke.epoch = 1;
    revoke.lease_id = 2;
    CHECK(gate.revoke(revoke));
    CHECK(gate.status(50'000'000).mode == guard::GateMode::moving);
    revoke.lease_id = 0;
    revoke.reason = 404;
    CHECK(gate.revoke(revoke));
    CHECK(gate.status(50'000'000).latch == guard::LatchCause::sensor_lost);
    CHECK(gate.revoke(revoke));
}
TEST_CASE("supervisor loss while ready or already braking preserves a recovery latch") {
    guard::Gate idle;
    ready(idle);
    idle.motor_timeout();
    CHECK(idle.status(50'000'000).mode == guard::GateMode::ready);
    idle.supervisor_lost();
    CHECK(idle.status(50'000'000).mode == guard::GateMode::latched);
    guard::RecoverRequest request;
    request.request_id = 1;
    CHECK((idle.recover(request, {true, true, true}).unmet_mask & 4) != 0);
    guard::Gate braking;
    ready(braking);
    REQUIRE(braking.lease(lease(), 50'000'000).reason == 500);
    braking.tick(300'000'000, 0.4);
    braking.supervisor_lost();
    CHECK(braking.status(300'000'000).mode == guard::GateMode::stopping);
    CHECK(braking.status(300'000'000).latch == guard::LatchCause::supervisor_lost);
}
TEST_CASE("repeated and backwards ticks do not emit another motor command") {
    guard::Gate gate;
    ready(gate);
    const auto before = gate.tick(50'000'000, 0);
    CHECK(gate.tick(50'000'000, 1).cmd_seq == before.cmd_seq);
    CHECK(gate.tick(-1, 1).cmd_seq == before.cmd_seq);
    CHECK(gate.tick(40'000'000, 1).stamp_ns == before.stamp_ns);
    guard::Gate rotating;
    guard::RegisterReply reply;
    REQUIRE(rotating.register_supervisor(reply));
    for (unsigned i = 0; i < 6; ++i)
        rotating.tick(i * 10'000'000, 0, 0.1);
    guard::RecoverRequest request;
    request.request_id = 1;
    CHECK((rotating.recover(request, {true, true, true}).unmet_mask & 1) != 0);
    request.request_id = 0;
    CHECK(rotating.recover(request, {true, true, true}).reason == 504);
}
TEST_CASE("invalid configurations cannot register and enter the internal fault path") {
    guard::Config config;
    config.t_tick_ms = 0;
    guard::Gate gate(config);
    guard::RegisterReply reply;
    CHECK_FALSE(gate.register_supervisor(reply));
    gate.tick(0, 0);
    CHECK(gate.status(0).latch == guard::LatchCause::internal);
}

TEST_CASE("baseline rejects malformed future and backward commands without installing them") {
    guard::Config config;
    config.mode = guard::Mode::heartbeat;
    guard::Gate gate(config);
    guard::PassthruCmd value;
    value.cmd_seq = 1;
    value.v = 0.4;
    value.header.version = 2;
    CHECK(gate.passthrough(value, 0) == guard::Reason::REJ_SCHEMA);
    value.header.version = 1;
    CHECK(gate.passthrough(value, -1) == guard::Reason::REJ_SCHEMA);
    REQUIRE(gate.passthrough(value, 40'000'000) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
    ++value.cmd_seq;
    CHECK(gate.passthrough(value, 30'000'000) == guard::Reason::REJ_SCHEMA);
    value.stamp_ns = 50'000'000;
    CHECK(gate.passthrough(value, 40'000'000) == guard::Reason::REJ_SCHEMA);
    gate.tick(50'000'000, 0.4);
    value.stamp_ns = 0;
    CHECK(gate.passthrough(value, 40'000'000) == guard::Reason::REJ_SCHEMA);
    CHECK(gate.tick(60'000'000, 0.4).v == 0.4);
    config.t_tick_ms = 0;
    guard::Gate invalid(config);
    CHECK(invalid.passthrough(value, 0) == guard::Reason::REJ_SCHEMA);
    config.mode = guard::Mode::guarded;
    guard::Gate bad_lease(config);
    CHECK(bad_lease.lease(lease(), 50'000'000).reason == 504);
}

TEST_CASE("matching lease revokes stop and baseline motor faults recover without a supervisor") {
    guard::Gate guarded;
    ready(guarded);
    REQUIRE(guarded.lease(lease(), 50'000'000).reason == 500);
    guard::Revoke revoke;
    revoke.epoch = revoke.lease_id = 1;
    revoke.reason = 403;
    REQUIRE(guarded.revoke(revoke));
    CHECK(guarded.status(50'000'000).mode == guard::GateMode::stopping);
    guard::Config config;
    config.mode = guard::Mode::heartbeat;
    guard::Gate baseline(config);
    guard::PassthruCmd command;
    command.cmd_seq = 1;
    command.v = 0.4;
    REQUIRE(baseline.passthrough(command, 0) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
    baseline.tick(0, 0.4);
    baseline.motor_timeout();
    for (unsigned i = 1; i <= 7; ++i)
        baseline.tick(i * 10'000'000, 0);
    CHECK(baseline.status(70'000'000).mode == guard::GateMode::latched);
    guard::RecoverRequest request;
    request.request_id = 1;
    CHECK(baseline.recover(request, {true, true, true}).reason == 512);
}

TEST_CASE("heartbeat refusal while braking does not consume passthrough sequence") {
    guard::Config config;
    config.mode = guard::Mode::heartbeat;
    guard::Gate gate(config);
    guard::PassthruCmd value;
    value.cmd_seq = 1;
    value.v = 0.4;
    REQUIRE(gate.passthrough(value, 0) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
    value.cmd_seq = 2;
    value.stamp_ns = 500'000'000;
    REQUIRE(gate.passthrough(value, value.stamp_ns) == guard::Reason::GATE_NOT_READY);
    for (unsigned i = 50; i <= 60; ++i)
        gate.tick(i * 10'000'000, 0);
    REQUIRE(gate.status(600'000'000).mode == guard::GateMode::ready);
    value.stamp_ns = 600'000'000;
    CHECK(gate.passthrough(value, value.stamp_ns) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
}
