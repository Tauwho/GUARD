#include "guard/gate_fsm.hpp"
#include "guard/plant.hpp"
#include <doctest/doctest.h>

TEST_CASE("expired lease stops the plant without a supervisor stop message") {
    guard::Plant plant;
    guard::Gate gate;
    guard::Scene scene;
    guard::RegisterReply registration;
    REQUIRE(gate.register_supervisor(registration));
    for (unsigned tick = 1; tick <= 5; ++tick) {
        const auto now = guard::milliseconds(tick * 10);
        REQUIRE(plant.command(gate.tick(now, plant.linear_velocity()), now));
        REQUIRE(plant.step(now, scene));
    }
    guard::RecoverRequest recover;
    recover.request_id = 1;
    REQUIRE(gate.recover(recover, {true, true, true}).unmet_mask == 0);
    for (unsigned tick = 6; tick <= 250; ++tick) {
        const auto now = guard::milliseconds(tick * 10);
        if (tick <= 100 && tick % 5 == 0) {
            guard::Lease lease;
            lease.lease_id = lease.lease_seq = lease.proposal_id = tick;
            lease.epoch = registration.epoch;
            lease.v_cmd = lease.v_cap = 0.8;
            lease.issued_ns = now;
            lease.expires_ns = now + guard::milliseconds(250);
            REQUIRE(gate.lease(lease, now).reason == 500);
        }
        const auto motor = gate.tick(now, plant.linear_velocity(), plant.angular_velocity());
        REQUIRE(plant.command(motor, now));
        REQUIRE(plant.step(now, scene));
    }
    CHECK(gate.status(guard::milliseconds(2500)).counters[506] == 1);
    CHECK(gate.status(guard::milliseconds(2500)).mode == guard::GateMode::ready);
    CHECK(plant.linear_velocity() == 0);
    CHECK_FALSE(plant.watchdog_active());
}
