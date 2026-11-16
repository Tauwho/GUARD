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
            first.str() != second.str()) {
            const auto text = first.str();
            std::cerr << text.substr(text.size() > 4000 ? text.size() - 4000 : 0);
            throw std::runtime_error("nominal goal or deterministic output failed");
        }
        map.goal = {38, 0, 0};
        scenario.duration = 20;
        scenario.fault_count = 1;
        scenario.faults[0].fault = 3;
        scenario.faults[0].at = 12;
        std::ostringstream killed;
        const auto stopped = guard::host::replay(map, scenario, {}, 42, killed);
        if (stopped.goal || stopped.contacts || stopped.zone_breaches ||
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
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
