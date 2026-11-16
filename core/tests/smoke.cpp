#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "guard/types.hpp"
#include <doctest/doctest.h>

TEST_CASE("portable C++17 test entry point") {
    constexpr int observation_capacity = 64;
    static_assert(__cplusplus >= 201703L);
    CHECK(observation_capacity > 0);
}

TEST_CASE("wire layouts match the draft contract") {
    CHECK(guard::Observation{}.header.size == sizeof(guard::Observation));
    CHECK(guard::Proposal{}.header.size == sizeof(guard::Proposal));
    CHECK(guard::WorldLoad{}.header.size == sizeof(guard::WorldLoad));
}
