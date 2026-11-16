#include "replay.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

int main() {
    try {
        guard::MapSpec map;
        map.goal = {5, 0, 0};
        guard::ScenarioSpec scenario;
        scenario.duration = 30;
        std::ostringstream first, second;
        const auto result = guard::host::replay(map, scenario, {}, 42, first);
        guard::host::replay(map, scenario, {}, 42, second);
        if (!result.goal || result.contacts || result.zone_breaches ||
            first.str() != second.str() ||
            result.authorized + result.rejected + result.unverifiable != result.proposals) {
            const auto text = first.str();
            std::cerr << text.substr(text.size() > 4000 ? text.size() - 4000 : 0);
            throw std::runtime_error("nominal goal or deterministic output failed");
        }
        if (result.margin_measured || first.str().find("min_margin_m") != std::string::npos)
            throw std::runtime_error("empty world must not publish a synthetic margin");
        map.goal = {38, 0, 0};
        scenario.duration = 20;
        scenario.fault_count = 1;
        scenario.faults[0].fault = 3;
        scenario.faults[0].at = 12;
        std::ostringstream killed;
        const auto stopped = guard::host::replay(map, scenario, {}, 42, killed);
        if (stopped.goal || stopped.contacts || stopped.zone_breaches || stopped.latches != 1 ||
            killed.str().find("GATE_SUPERVISOR_LOST") == std::string::npos ||
            killed.str().find("\"final_state\":\"LATCHED\"") == std::string::npos)
            throw std::runtime_error("supervisor loss did not stop and latch");
        scenario.faults[0].fault = 7;
        scenario.faults[0].duration_ms = 2000;
        std::ostringstream zombie;
        const auto replaced = guard::host::replay(map, scenario, {}, 42, zombie);
        if (replaced.goal || replaced.contacts ||
            zombie.str().find("GATE_EPOCH_CHANGED") == std::string::npos ||
            zombie.str().find("GATE_EPOCH_STALE") == std::string::npos ||
            zombie.str().find("\"final_state\":\"LATCHED\"") == std::string::npos)
            throw std::runtime_error("zombie lease did not preserve the latch");
        for (const auto mode : {guard::Mode::passthrough, guard::Mode::heartbeat}) {
            guard::Config config;
            config.mode = mode;
            std::ostringstream baseline;
            const auto moving = guard::host::replay(map, scenario, config, 42, baseline);
            if (moving.goal || moving.contacts || moving.proposals != 0 ||
                moving.passthrough_commands == 0 ||
                baseline.str().find("\"final_state\":\"MOVING\"") == std::string::npos ||
                baseline.str().find("\"kind\":\"decision\"") != std::string::npos)
                throw std::runtime_error("baseline must bypass supervisor decisions");
        }
        scenario.faults[0].fault = 2;
        scenario.faults[0].duration_ms = 400;
        std::ostringstream stalled;
        guard::host::replay(map, scenario, {}, 42, stalled);
        if (stalled.str().find("\"primary_reason\":\"REJ_GATE_NOT_READY\"") == std::string::npos ||
            stalled.str().find("\"primary_reason\":\"GATE_NOT_READY\"") != std::string::npos)
            throw std::runtime_error("gate refusal must use the C1 decision reason");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
