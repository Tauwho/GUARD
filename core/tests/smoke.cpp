#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("portable C++17 test entry point") {
    constexpr int observation_capacity = 64;
    static_assert(__cplusplus >= 201703L);
    CHECK(observation_capacity > 0);
}
