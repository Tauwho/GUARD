#pragma once
#include "guard/plant.hpp"
#include <string_view>

namespace guard {
// C9 JSON representation; not a new binary IPC message.
struct RouteProposal {
    std::uint64_t request_id = 0, route_id = 0, observation_id = 0;
    std::int64_t observation_stamp_ns = 0, created_ns = 0;
    std::uint32_t map_version = 0;
    std::array<Point, 16> waypoints{};
    std::size_t waypoint_count = 0;
};
bool load_route(std::string_view buffer, RouteProposal &output);
// Source must be the caller's retained own observation, never a planner-supplied copy.
// Returns zero for a usable advisory route; this never grants motion permission.
std::uint16_t check_route(const RouteProposal &route, const Observation &source,
                          const Observation &current, const Scene &scene, const Config &config,
                          std::int64_t now);
} // namespace guard
