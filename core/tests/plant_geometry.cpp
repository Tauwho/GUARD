#include "guard/plant.hpp"
#include <cmath>
#include <doctest/doctest.h>
#include <limits>

TEST_CASE("swept footprint detects narrow zones between both endpoints") {
    guard::Scene scene;
    scene.zone_count = 1;
    scene.zones[0] = {{{1, -1}, {1.01, 1}}, true, true};
    CHECK(guard::valid(scene));
    CHECK(guard::swept_ground_truth({0, 0}, {2, 0}, 0.1, scene).zone_breach);
    CHECK_FALSE(guard::swept_ground_truth({0, 2}, {2, 2}, 0.1, scene).zone_breach);
}

TEST_CASE("circle clearance and rectangle corner distances") {
    guard::Scene scene;
    scene.obstacle_count = 1;
    scene.obstacles[0] = {1, 0.5, 0.2};
    auto result = guard::swept_ground_truth({0, 0}, {2, 0}, 0.2, scene);
    CHECK(result.clearance == doctest::Approx(0.1));
    CHECK_FALSE(result.contact);
    CHECK(guard::segment_distance({0, 0}, {1, 0}, guard::Rect{{2, 1}, {3, 2}}) ==
          doctest::Approx(std::sqrt(2.0)));
}

TEST_CASE("configuration validation rejects invalid bounds and baseline ablations") {
    guard::Config config;
    CHECK(guard::valid(config));
    config.a_brake = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(guard::valid(config));
    config = {};
    config.mode = guard::Mode::heartbeat;
    config.ablate_expiry = true;
    CHECK_FALSE(guard::valid(config));
}
