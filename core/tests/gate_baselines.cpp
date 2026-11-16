#include "guard/gate_fsm.hpp"
#include <doctest/doctest.h>

TEST_CASE("P1 P2 P4: B0 holds commands and ignores supervisor loss") {
    guard::Config config;
    config.mode = guard::Mode::passthrough;
    guard::Gate gate(config);
    CHECK(gate.status(0).mode == guard::GateMode::ready);
    guard::PassthruCmd command;
    command.cmd_seq = 1;
    command.v = 0.8;
    CHECK(gate.passthrough(command, 0) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
    CHECK(gate.tick(0, 0).v == 0.8);
    gate.supervisor_lost();
    CHECK(gate.tick(5'000'000'000, 0.8).v == 0.8);
    CHECK(gate.status(5'000'000'000).experiment == guard::Mode::passthrough);
    CHECK(gate.passthrough(command, 5'000'000'000) == guard::Reason::REJ_SCHEMA);
}
TEST_CASE("P3 P5: B1 stops at the heartbeat deadline and drops commands during braking") {
    guard::Config config;
    config.mode = guard::Mode::heartbeat;
    guard::Gate gate(config);
    guard::PassthruCmd command;
    command.cmd_seq = 1;
    command.v = 0.8;
    REQUIRE(gate.passthrough(command, 0) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
    CHECK(gate.tick(499'000'000, 0.8).v == 0.8);
    CHECK(gate.tick(500'000'000, 0.8).v < 0.8);
    CHECK(gate.status(500'000'000).mode == guard::GateMode::stopping);
    CHECK(gate.status(500'000'000).counters[515] == 1);
    ++command.cmd_seq;
    CHECK(gate.passthrough(command, 500'000'000) == guard::Reason::GATE_NOT_READY);
    for (unsigned tick = 51; tick <= 57; ++tick)
        gate.tick(tick * 10'000'000, 0);
    CHECK(gate.status(570'000'000).mode == guard::GateMode::ready);
    ++command.cmd_seq;
    CHECK(gate.passthrough(command, 570'000'000) == guard::Reason::GATE_PASSTHRU_ACCEPTED);
}
TEST_CASE("baseline and guarded control interfaces cannot be mixed") {
    guard::Gate guarded;
    guard::PassthruCmd command;
    command.cmd_seq = 1;
    CHECK(guarded.passthrough(command, 0) == guard::Reason::GATE_UNAUTHORIZED_CLIENT);
    guard::Config config;
    config.mode = guard::Mode::heartbeat;
    guard::Gate baseline(config);
    guard::RegisterReply reply;
    CHECK_FALSE(baseline.register_supervisor(reply));
    CHECK(baseline.lease({}, 0).reason == 505);
    CHECK_FALSE(baseline.revoke({}));
    CHECK(baseline.status(0).counters[505] == 3);
}
