#include "guard/scenario.hpp"
#include <doctest/doctest.h>

TEST_CASE("world applies crossing and activation timelines deterministically") {
    guard::MapSpec map;
    map.scene.zone_count = 1;
    map.scene.zones[0] = {{{5, 0}, {6, 1}}, false, true};
    map.crossing_count = 1;
    map.crossings[0].from = {2, 0};
    map.crossings[0].to = {2, 4};
    map.crossings[0].speed = 1;
    guard::ScenarioSpec scenario;
    scenario.obstacle_count = 1;
    scenario.obstacles[0].id = {'n', 'u', 'r', 's', 'e'};
    scenario.obstacles[0].action = guard::ObstacleAction::crossing;
    scenario.obstacles[0].at = 1;
    scenario.zone_count = 1;
    scenario.zones[0] = {2, 0, true};
    guard::World world(map, scenario);
    REQUIRE(world.update(0));
    CHECK(world.scene().obstacle_count == 0);
    REQUIRE(world.update(2'000'000'000));
    REQUIRE(world.scene().obstacle_count == 1);
    CHECK(world.scene().obstacles[0].y == 1);
    CHECK(world.scene().zones[0].active);
    CHECK(world.scene().map_version == 2);
    REQUIRE(world.update(2'000'000'000));
    CHECK(world.scene().map_version == 2);
    REQUIRE(world.update(5'000'000'000));
    CHECK(world.scene().obstacle_count == 0);
    CHECK_FALSE(world.update(4'000'000'000));
}
