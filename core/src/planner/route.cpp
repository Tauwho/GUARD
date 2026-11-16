#include "guard/route.hpp"
#include "guard/buffer_json.hpp"
#include "guard/codec.hpp"
#include "guard/reason_codes.hpp"
#include <cmath>
#include <limits>

namespace guard {
bool load_route(std::string_view buffer, RouteProposal &output) {
    BufferJson json(buffer);
    if (!json.valid() || json.node(0).kind != BufferJson::Kind::object ||
        json.text(json.member(0, "type")) != "route")
        return false;
    std::uint64_t version = 0;
    if (!json.unsigned_integer(json.member(0, "v"), version) || version != 1)
        return false;
    for (std::size_t key = 1; key < json.node(0).next; key = json.node(key + 1).next) {
        const auto name = json.text(key);
        if (name != "v" && name != "type" && name != "req_id" && name != "route_id" &&
            name != "based_on" && name != "created_ns" && name != "waypoints" &&
            name != "rationale")
            return false;
    }
    RouteProposal route;
    if (!json.unsigned_integer(json.member(0, "req_id"), route.request_id) || !route.request_id ||
        !json.unsigned_integer(json.member(0, "route_id"), route.route_id) || !route.route_id)
        return false;
    const auto based_on = json.member(0, "based_on");
    if (json.node(based_on).kind != BufferJson::Kind::object)
        return false;
    for (std::size_t key = based_on + 1; key < json.node(based_on).next;
         key = json.node(key + 1).next)
        if (json.text(key) != "obs_id" && json.text(key) != "stamp_ns" &&
            json.text(key) != "map_version")
            return false;
    std::uint64_t stamp = 0, created = 0, map = 0;
    if (!json.unsigned_integer(json.member(based_on, "obs_id"), route.observation_id) ||
        !route.observation_id || !json.unsigned_integer(json.member(based_on, "stamp_ns"), stamp) ||
        stamp > std::uint64_t(std::numeric_limits<std::int64_t>::max()) ||
        !json.unsigned_integer(json.member(0, "created_ns"), created) ||
        created > std::uint64_t(std::numeric_limits<std::int64_t>::max()) ||
        !json.unsigned_integer(json.member(based_on, "map_version"), map) || map == 0 ||
        map > 0xffffffffU)
        return false;
    route.observation_stamp_ns = static_cast<std::int64_t>(stamp);
    route.created_ns = static_cast<std::int64_t>(created);
    route.map_version = static_cast<std::uint32_t>(map);
    const auto waypoints = json.member(0, "waypoints");
    if (json.node(waypoints).kind != BufferJson::Kind::array)
        return false;
    for (std::size_t child = waypoints + 1; child < json.node(waypoints).next;
         child = json.node(child).next) {
        if (route.waypoint_count == route.waypoints.size() ||
            json.node(child).kind != BufferJson::Kind::array || json.node(child).next != child + 3)
            return false;
        auto &point = route.waypoints[route.waypoint_count++];
        if (!json.number(child + 1, point.x) || !json.number(child + 2, point.y) ||
            std::abs(point.x) > 100 || std::abs(point.y) > 100)
            return false;
    }
    if (!route.waypoint_count)
        return false;
    const auto rationale = json.member(0, "rationale");
    if (rationale != BufferJson::missing &&
        (json.node(rationale).kind != BufferJson::Kind::string ||
         json.text(rationale).size() > 500))
        return false;
    output = route;
    return true;
}

std::uint16_t check_route(const RouteProposal &route, const Observation &source,
                          const Observation &current, const Scene &scene, const Config &config,
                          std::int64_t now) {
    auto code = [](Reason reason) { return static_cast<std::uint16_t>(reason); };
    if (!valid(config) || !valid(scene) || !valid(source) || !valid(current) || now < 0)
        return code(Reason::UNV_INTERNAL);
    if (!current.sensor_ok || now < current.stamp_ns ||
        now - current.stamp_ns > milliseconds(config.sensor_stale_ms))
        return code(Reason::UNV_SENSOR_STALE);
    if (!route.request_id || !route.route_id || !route.waypoint_count ||
        route.waypoint_count > route.waypoints.size() || route.created_ns < 0)
        return code(Reason::REJ_SCHEMA);
    if (route.observation_id != source.obs_id)
        return code(Reason::REJ_UNKNOWN_OBSERVATION);
    if (route.observation_stamp_ns != source.stamp_ns || !source.sensor_ok)
        return code(Reason::REJ_PROVENANCE_MISMATCH);
    if (now < source.stamp_ns || now - source.stamp_ns > milliseconds(config.route_max_age_ms))
        return code(Reason::REJ_STALE_OBSERVATION);
    if (route.map_version != source.map_version || route.map_version != current.map_version ||
        route.map_version != scene.map_version)
        return code(Reason::REJ_MAP_VERSION);
    Point previous{current.x, current.y};
    for (std::size_t i = 0; i < route.waypoint_count; ++i) {
        const auto point = route.waypoints[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || std::abs(point.x) > 100 ||
            std::abs(point.y) > 100)
            return code(Reason::REJ_SCHEMA);
        for (std::size_t z = 0; z < scene.zone_count; ++z)
            if (scene.zones[z].active && scene.zones[z].restricted &&
                segment_distance(previous, point, scene.zones[z].rect) <=
                    config.r_robot_m + config.margin_m)
                return code(Reason::REJ_RESTRICTED_ZONE);
        previous = point;
    }
    return 0;
}
} // namespace guard
