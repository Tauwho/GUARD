#include "guard/gate_fsm.hpp"
#include <doctest/doctest.h>
#include <limits>

namespace {
std::uint16_t code(guard::Reason reason) { return static_cast<std::uint16_t>(reason); }
guard::Lease lease(std::uint64_t sequence = 1, std::uint32_t epoch = 1,
                   std::int64_t issued = 50'000'000) {
    guard::Lease value;
    value.lease_id = value.lease_seq = value.proposal_id = sequence;
    value.epoch = epoch;
    value.issued_ns = issued;
    value.expires_ns = issued + 250'000'000;
    value.v_cmd = value.v_cap = 0.8;
    return value;
}
void ready(guard::Gate &gate) {
    guard::RegisterReply registration;
    REQUIRE(gate.register_supervisor(registration));
    for (unsigned i = 0; i < 5; ++i)
        gate.tick(i * 10'000'000, 0);
    guard::RecoverRequest request;
    request.request_id = 1;
    REQUIRE(gate.recover(request, {true, true, true}).reason ==
            code(guard::Reason::GATE_RECOVERED));
}
void moving(guard::Gate &gate) {
    ready(gate);
    REQUIRE(gate.lease(lease(), 50'000'000).reason == code(guard::Reason::GATE_LEASE_ACCEPTED));
    gate.tick(50'000'000, 0.8);
}
} // namespace

TEST_CASE("R1 R2 R3 boot latch and recovery mask") {
    guard::Gate gate;
    CHECK(gate.status(0).mode == guard::GateMode::latched);
    guard::RecoverRequest request;
    request.request_id = 1;
    CHECK(gate.recover(request, {}).unmet_mask == 31);
    ready(gate);
    CHECK(gate.status(40'000'000).mode == guard::GateMode::ready);
    CHECK(gate.recover(request, {true, true, true}).reason == code(guard::Reason::GATE_NOT_READY));
}

TEST_CASE("R4 R5 accepted and renewed leases") {
    guard::Gate gate;
    moving(gate);
    auto next = lease(2);
    CHECK(gate.lease(next, 60'000'000).reason == code(guard::Reason::GATE_LEASE_ACCEPTED));
    CHECK(gate.status(60'000'000).lease_id == 2);
    CHECK(gate.status(60'000'000).lease_remaining_ns == 240'000'000);
}

TEST_CASE("R6 R15 expiry emits a bounded nonincreasing braking command") {
    guard::Gate gate;
    moving(gate);
    CHECK(gate.tick(299'999'999, 0.8).v == 0.8);
    auto motor = gate.tick(300'000'000, 0.8);
    CHECK(gate.status(300'000'000).mode == guard::GateMode::stopping);
    CHECK(motor.v == doctest::Approx(0.79));
    auto renewed = lease(2, 1, 300'000'000);
    CHECK(gate.lease(renewed, 300'000'000).reason == code(guard::Reason::GATE_NOT_READY));
    CHECK(gate.tick(310'000'000, 0.85).v <= motor.v);
}

TEST_CASE("R7 R16 R17 R18 revoke and standstill preserve latch policy") {
    for (bool sensor_fault : {false, true}) {
        guard::Gate gate;
        moving(gate);
        guard::Revoke request;
        request.epoch = 1;
        request.reason = sensor_fault ? 400 : 403;
        REQUIRE(gate.revoke(request));
        for (unsigned i = 6; i < 11; ++i)
            gate.tick(i * 10'000'000, 0);
        CHECK(gate.status(100'000'000).mode == guard::GateMode::stopped);
        gate.tick(110'000'000, 0);
        CHECK(gate.status(110'000'000).mode ==
              (sensor_fault ? guard::GateMode::latched : guard::GateMode::ready));
        request.epoch = 0;
        CHECK_FALSE(gate.revoke(request));
    }
}

TEST_CASE("R8 R9 R10 independent fault transitions latch") {
    for (int fault : {8, 9, 10}) {
        guard::Gate gate;
        moving(gate);
        if (fault == 8) {
            guard::RegisterReply reply;
            REQUIRE(gate.register_supervisor(reply));
        }
        if (fault == 9)
            gate.supervisor_lost();
        if (fault == 10)
            gate.motor_timeout();
        CHECK(gate.status(60'000'000).mode == guard::GateMode::stopping);
        CHECK(gate.status(60'000'000).latch != guard::LatchCause::none);
    }
}

TEST_CASE("R11 R12 R13 R14 refused leases cannot change motion") {
    guard::Gate gate;
    moving(gate);
    CHECK(gate.lease(lease(2, 2), 60'000'000).reason == code(guard::Reason::GATE_EPOCH_STALE));
    CHECK(gate.lease(lease(), 60'000'000).reason == code(guard::Reason::GATE_LEASE_REPLAY));
    auto expired = lease(3, 1, 0);
    expired.expires_ns = 50'000'000;
    CHECK(gate.lease(expired, 60'000'000).reason ==
          code(guard::Reason::GATE_LEASE_EXPIRED_ON_ARRIVAL));
    CHECK(gate.status(60'000'000).lease_id == 1);
    CHECK(gate.status(60'000'000).lease_seq == 1);
    CHECK(gate.lease(lease(2), 60'000'000).reason == code(guard::Reason::GATE_LEASE_ACCEPTED));
    guard::Gate boot;
    guard::RegisterReply reply;
    REQUIRE(boot.register_supervisor(reply));
    CHECK(boot.lease(lease(), 50'000'000).reason == code(guard::Reason::GATE_NOT_READY));
    CHECK(boot.status(50'000'000).lease_seq == 0);
    for (unsigned i = 0; i < 5; ++i)
        boot.tick(i * 10'000'000, 0);
    guard::RecoverRequest recover;
    recover.request_id = 1;
    REQUIRE(boot.recover(recover, {true, true, true}).reason ==
            code(guard::Reason::GATE_RECOVERED));
    CHECK(boot.lease(lease(), 50'000'000).reason == code(guard::Reason::GATE_LEASE_ACCEPTED));
}

TEST_CASE("ACK delivery failure leaves lease inactive and fences retries") {
    guard::Gate gate;
    ready(gate);
    CHECK(gate.prepare_lease(lease(), 50'000'000).reason ==
          code(guard::Reason::GATE_LEASE_ACCEPTED));
    CHECK(gate.status(50'000'000).mode == guard::GateMode::ready);
    CHECK_FALSE(gate.commit_lease(1, 50'000'000, false));
    CHECK(gate.status(50'000'000).lease_id == 0);
    CHECK(gate.status(50'000'000).counters[516] == 1);
    CHECK(gate.lease(lease(), 50'000'000).reason == code(guard::Reason::GATE_LEASE_REPLAY));
}

TEST_CASE("supervisor loss blocks recovery until a new registration") {
    guard::Gate gate;
    moving(gate);
    gate.supervisor_lost();
    for (unsigned i = 6; i < 12; ++i)
        gate.tick(i * 10'000'000, 0);
    guard::RecoverRequest request;
    request.request_id = 1;
    CHECK((gate.recover(request, {true, true, true}).unmet_mask & 4u) != 0);
}

TEST_CASE("invalid measurements cannot confirm standstill") {
    guard::Gate gate;
    moving(gate);
    for (unsigned i = 6; i < 20; ++i)
        gate.tick(i * 10'000'000, std::numeric_limits<double>::quiet_NaN());
    CHECK(gate.status(190'000'000).mode == guard::GateMode::stopping);
}
