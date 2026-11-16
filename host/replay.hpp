#pragma once
#include "guard/scenario.hpp"
#include <iosfwd>

namespace guard::host {
struct ReplayResult {
    bool goal = false;
    std::uint64_t contacts = 0, zone_breaches = 0, proposals = 0, rejected = 0;
    std::int64_t ended_ns = 0;
};
ReplayResult replay(const MapSpec &map, const ScenarioSpec &scenario, Config config,
                    std::uint64_t seed, std::ostream &output);
} // namespace guard::host
