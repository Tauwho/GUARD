#include "guard/supervisor.hpp"
#include <doctest/doctest.h>

TEST_CASE("active lease is revoked when a newer obstacle enters its braking path") {
    guard::Supervisor supervisor;
    guard::Observation observation;
    observation.obs_id = observation.seq = observation.map_version = observation.sensor_ok = 1;
    observation.v_meas = 0.4;
    REQUIRE(supervisor.observe(observation, 0));
    guard::Lease lease;
    lease.epoch = lease.lease_id = lease.lease_seq = lease.proposal_id = 1;
    lease.v_cmd = lease.v_cap = 0.4;
    lease.expires_ns = 250'000'000;
    CHECK(supervisor.recheck(lease, {}, 1) == 0);
    observation.obs_id = observation.seq = 2;
    observation.stamp_ns = 50'000'000;
    observation.obstacle_count = 1;
    observation.obstacles[0] = {0.9, 0, 0.3};
    REQUIRE(supervisor.observe(observation, observation.stamp_ns));
    CHECK(supervisor.recheck(lease, {}, observation.stamp_ns) ==
          static_cast<unsigned>(guard::Reason::REVOKE_STOPPING_DISTANCE));
    CHECK(supervisor.recheck(lease, {}, 500'000'000) ==
          static_cast<unsigned>(guard::Reason::REVOKE_SENSOR_HEALTH));
}

TEST_CASE("three consecutive unverifiable rechecks revoke and a pass clears the count") {
    guard::Supervisor supervisor;
    guard::Observation observation;
    observation.obs_id = observation.seq = observation.map_version = observation.sensor_ok = 1;
    REQUIRE(supervisor.observe(observation, 0));
    guard::Lease invalid;
    CHECK(supervisor.recheck(invalid, {}, 0) == 0);
    CHECK(supervisor.recheck(invalid, {}, 0) == 0);
    CHECK(supervisor.recheck(invalid, {}, 0) ==
          static_cast<unsigned>(guard::Reason::REVOKE_UNVERIFIABLE));
}
