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

TEST_CASE("swept footprint reports contact for overlapping obstacles and crossed walls") {
    guard::Scene scene;
    scene.obstacle_count = 1;
    scene.obstacles[0] = {1, 0.4, 0.2};
    auto touching = guard::swept_ground_truth({0, 0}, {2, 0}, 0.25, scene);
    CHECK(touching.contact);
    CHECK(touching.clearance == doctest::Approx(-0.05));
    CHECK_FALSE(touching.zone_breach);
    scene.obstacles[0].y = 0.5;
    CHECK_FALSE(guard::swept_ground_truth({0, 0}, {2, 0}, 0.25, scene).contact);
    scene.obstacle_count = 0;
    scene.wall_count = 1;
    scene.walls[0] = {{1.5, -1}, {1.6, 1}};
    auto wall = guard::swept_ground_truth({0, 0}, {2, 0}, 0.25, scene);
    CHECK(wall.contact);
    CHECK_FALSE(wall.zone_breach);
    CHECK_FALSE(guard::swept_ground_truth({0, 0}, {1, 0}, 0.25, scene).contact);
}

TEST_CASE("plant ground truth reports contact once the footprint reaches an obstacle") {
    guard::Plant plant;
    guard::Scene scene;
    scene.obstacle_count = 1;
    scene.obstacles[0] = {1.0, 0, 0.2};
    const double reach = 1.0 - 0.2 - guard::Config{}.r_robot_m;
    unsigned first_contact = 0;
    for (unsigned tick = 1; tick <= 200 && first_contact == 0; ++tick) {
        const auto now = guard::milliseconds(tick * 10);
        guard::MotorCmd command;
        command.cmd_seq = tick;
        command.v = 0.8;
        command.stamp_ns = now;
        REQUIRE(plant.command(command, now));
        REQUIRE(plant.step(now, scene));
        if (plant.ground_truth().contact)
            first_contact = tick;
        else
            CHECK(plant.pose().x < reach);
    }
    REQUIRE(first_contact > 100);
    CHECK(plant.pose().x >= reach);
    CHECK(plant.pose().x < reach + 0.01);
    CHECK(plant.ground_truth().clearance <= 0);
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
