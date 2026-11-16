#include "json_fields.hpp"

namespace guard {
std::string_view name_view(const Name &name) {
    const auto end = std::find(name.begin(), name.end(), '\0');
    return {name.data(), static_cast<std::size_t>(end - name.begin())};
}
bool load_map(std::string_view buffer, MapSpec &output) {
    using namespace json_fields;
    BufferJson json(buffer);
    if (!json.valid() || json.text(json.member(0, "kind")) != "map")
        return false;
    std::uint64_t version;
    if (!json.unsigned_integer(json.member(0, "version"), version) || version != 1)
        return false;
    MapSpec map;
    Point size;
    if (!name(json, json.member(0, "id"), map.id) || !point(json, json.member(0, "size"), size) ||
        size.x <= 0 || size.y <= 0 || !pose(json, json.member(0, "start"), map.start) ||
        !pose(json, json.member(0, "goal"), map.goal))
        return false;
    if (!array(
            json, json.member(0, "walls"), 64,
            [&](auto index, auto i) { return rectangle(json, index, map.scene.walls[i]); },
            map.scene.wall_count))
        return false;
    if (!array(
            json, json.member(0, "zones"), 32,
            [&](auto index, auto i) {
                auto &zone = map.scene.zones[i];
                if (!name(json, json.member(index, "id"), map.zone_names[i]) ||
                    !rectangle(json, json.member(index, "rect"), zone.rect))
                    return false;
                for (std::size_t prior = 0; prior < i; ++prior)
                    if (map.zone_names[prior] == map.zone_names[i])
                        return false;
                const auto kind = json.text(json.member(index, "kind"));
                if (kind != "restricted" && kind != "slow" && kind != "info")
                    return false;
                zone.restricted = kind == "restricted";
                const auto active = json.member(index, "active");
                if (json.node(active).kind == BufferJson::Kind::boolean)
                    return json.boolean(active, zone.active);
                zone.active = false;
                return array(
                    json, active, 8,
                    [&](auto window, auto w) {
                        auto &interval = map.zone_windows[i][w];
                        if (!number(json, window, "from_s", interval.from) || interval.from < 0)
                            return false;
                        const auto end = json.member(window, "to_s");
                        if (end != BufferJson::missing &&
                            json.node(end).kind != BufferJson::Kind::null_value &&
                            (!json.number(end, interval.to) || interval.to < interval.from))
                            return false;
                        if (interval.from == 0 && interval.to > 0)
                            zone.active = true;
                        return true;
                    },
                    map.window_counts[i]);
            },
            map.scene.zone_count))
        return false;
    if (!array(
            json, json.member(0, "waypoints"), 16,
            [&](auto index, auto i) { return point(json, index, map.waypoints[i]); },
            map.waypoint_count, true))
        return false;
    if (!array(
            json, json.member(0, "crossings"), 16,
            [&](auto index, auto i) {
                auto &crossing = map.crossings[i];
                if (!name(json, json.member(index, "id"), crossing.id) ||
                    !point(json, json.member(index, "from"), crossing.from) ||
                    !point(json, json.member(index, "to"), crossing.to) ||
                    !number(json, index, "speed", crossing.speed) ||
                    !bounded(crossing.speed, 0.1, 2))
                    return false;
                const auto radius = json.member(index, "radius");
                return radius == BufferJson::missing ||
                       (json.number(radius, crossing.radius) && bounded(crossing.radius, 0.05, 2));
            },
            map.crossing_count, true))
        return false;
    if (!valid(map.scene))
        return false;
    output = map;
    return true;
}
} // namespace guard
