#include "guard/codec.hpp"
#include "guard/plant.hpp"
#include <doctest/doctest.h>

namespace {
guard::MotorCmd motor(std::uint64_t sequence, double v, std::int64_t now) {
    guard::MotorCmd command;
    command.cmd_seq = sequence;
    command.v = v;
    command.stamp_ns = now;
    return command;
}
} // namespace

TEST_CASE("plant acceleration and fixed-step position integration") {
    guard::Plant plant;
    guard::Scene scene;
    for (unsigned tick = 1; tick <= 100; ++tick) {
        const auto now = guard::milliseconds(tick * 10);
        REQUIRE(plant.command(motor(tick, 0.8, now), now));
        REQUIRE(plant.step(now, scene));
    }
    CHECK(plant.linear_velocity() == doctest::Approx(0.5));
    CHECK(plant.pose().x == doctest::Approx(0.25));
}

TEST_CASE("actuator watchdog brakes exactly at the timeout boundary") {
    guard::Plant plant;
    guard::Scene scene;
    for (unsigned tick = 1; tick <= 100; ++tick) {
        const auto now = guard::milliseconds(tick * 10);
        REQUIRE(plant.command(motor(tick, 0.8, now), now));
        REQUIRE(plant.step(now, scene));
    }
    REQUIRE(plant.step(guard::milliseconds(1040), scene));
    CHECK_FALSE(plant.watchdog_active());
    const double before = plant.linear_velocity();
    REQUIRE(plant.step(guard::milliseconds(1050), scene));
    CHECK(plant.watchdog_active());
    CHECK(plant.linear_velocity() < before);
    for (unsigned tick = 106; tick <= 170; ++tick)
        REQUIRE(plant.step(guard::milliseconds(tick * 10), scene));
    CHECK(plant.linear_velocity() == 0);
    CHECK_FALSE(plant.command(motor(1, 0.8, 0), guard::milliseconds(1700)));
}

TEST_CASE("plant traces are byte-identical for identical inputs") {
    guard::Plant left, right;
    guard::Scene scene;
    for (unsigned tick = 1; tick <= 200; ++tick) {
        auto now = guard::milliseconds(tick * 10);
        auto command = motor(tick, tick < 100 ? 0.5 : -0.5, now);
        REQUIRE(left.command(command, now));
        REQUIRE(right.command(command, now));
        REQUIRE(left.step(now, scene));
        REQUIRE(right.step(now, scene));
        std::array<std::uint8_t, sizeof(guard::Observation)> a{}, b{};
        REQUIRE(guard::encode(left.sample(now, scene), a));
        REQUIRE(guard::encode(right.sample(now, scene), b));
        CHECK(a == b);
    }
}
