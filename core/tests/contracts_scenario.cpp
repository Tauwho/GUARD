#include "guard/scenario.hpp"
#include <doctest/doctest.h>

TEST_CASE("map buffer loader validates references and physical extents") {
    guard::MapSpec map;
    constexpr auto valid =
        R"({"kind":"map","version":1,"id":"ward","size":[40,6],"walls":[],"zones":[],"start":{"x":2,"y":2,"theta":0},"goal":{"x":38,"y":2,"theta":0}})";
    REQUIRE(guard::load_map(valid, map));
    CHECK(guard::name_view(map.id) == "ward");
    CHECK(map.goal.x == 38);
    CHECK_FALSE(guard::load_map("{}", map));
    CHECK(map.goal.x == 38);
}

TEST_CASE("scenario buffer loader rejects mismatched maps and unknown references") {
    guard::MapSpec map;
    map.id = {'w', 'a', 'r', 'd'};
    guard::ScenarioSpec scenario;
    constexpr auto valid =
        R"({"kind":"scenario","version":1,"id":"F1","map":"ward","seed":1001,"duration_s":75,"planner":{"mode":"rule"},"obstacles":[],"zones":[],"faults":[{"t_s":10,"fault":"F1","duration_ms":3000,"params":{"lag_ms":600}}]})";
    REQUIRE(guard::load_scenario(valid, map, scenario));
    CHECK(scenario.id == 1);
    CHECK(scenario.faults[0].lag_ms == 600);
    CHECK_FALSE(guard::load_scenario("{}", map, scenario));
    CHECK(scenario.id == 1);
}
