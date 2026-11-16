#include "json_fields.hpp"

namespace guard {
bool load_scenario(std::string_view buffer, const MapSpec &map, ScenarioSpec &output) {
    using namespace json_fields;
    BufferJson json(buffer);
    if (!json.valid() || json.text(json.member(0, "kind")) != "scenario" ||
        json.text(json.member(0, "map")) != name_view(map.id))
        return false;
    std::uint64_t version;
    if (!json.unsigned_integer(json.member(0, "version"), version) || version != 1)
        return false;
    const auto id = json.text(json.member(0, "id"));
    if (id.size() != 2 || id[0] != 'F' || id[1] < '0' || id[1] > '8')
        return false;
    ScenarioSpec spec;
    spec.id = static_cast<unsigned>(id[1] - '0');
    if (!json.unsigned_integer(json.member(0, "seed"), spec.seed) ||
        !number(json, 0, "duration_s", spec.duration) || !bounded(spec.duration, 1, 600))
        return false;
    const auto planner = json.member(0, "planner");
    const auto mode = json.text(json.member(planner, "mode"));
    if (mode == "aggressive")
        spec.planner = ScenarioSpec::Planner::aggressive;
    else if (mode == "shortcut")
        spec.planner = ScenarioSpec::Planner::shortcut;
    else if (mode != "rule")
        return false;
    const auto cruise = json.member(planner, "cruise_v");
    if (cruise != BufferJson::missing && !json.number(cruise, spec.cruise_v))
        return false;
    if (!bounded(spec.cruise_v, 0, 1.5) ||
        !integer(json, planner, "lease_req_ms", spec.lease_req_ms, true) ||
        spec.lease_req_ms < 50 || spec.lease_req_ms > 1000 ||
        !integer(json, planner, "rate_hz", spec.rate_hz, true) || spec.rate_hz < 1 ||
        spec.rate_hz > 50)
        return false;
    if (!array(
            json, json.member(0, "obstacles"), 64,
            [&](auto index, auto i) {
                auto &event = spec.obstacles[i];
                if (!number(json, index, "t_s", event.at) || event.at < 0 ||
                    !name(json, json.member(index, "id"), event.id))
                    return false;
                const auto action = json.text(json.member(index, "action"));
                if (action == "despawn")
                    event.action = ObstacleAction::despawn;
                else if (action == "stop")
                    event.action = ObstacleAction::stop;
                else if (action == "spawn") {
                    if (!point(json, json.member(index, "at"), event.position))
                        return false;
                } else if (action == "start_crossing") {
                    event.action = ObstacleAction::crossing;
                    const auto crossing = json.text(json.member(index, "crossing"));
                    bool found = false;
                    for (std::size_t c = 0; c < map.crossing_count; ++c)
                        if (crossing == name_view(map.crossings[c].id)) {
                            event.crossing = c;
                            found = true;
                        }
                    if (!found)
                        return false;
                } else
                    return false;
                const auto radius = json.member(index, "r");
                return radius == BufferJson::missing ||
                       (json.number(radius, event.radius) && bounded(event.radius, 0.05, 2));
            },
            spec.obstacle_count))
        return false;
    if (!array(
            json, json.member(0, "zones"), 32,
            [&](auto index, auto i) {
                auto &event = spec.zones[i];
                if (!number(json, index, "t_s", event.at) || event.at < 0 ||
                    !json.boolean(json.member(index, "active"), event.active))
                    return false;
                const auto zone = json.text(json.member(index, "zone"));
                for (std::size_t z = 0; z < map.scene.zone_count; ++z)
                    if (zone == name_view(map.zone_names[z])) {
                        event.zone = z;
                        return true;
                    }
                return false;
            },
            spec.zone_count))
        return false;
    if (!array(
            json, json.member(0, "faults"), 16,
            [&](auto index, auto i) {
                auto &event = spec.faults[i];
                const auto fault = json.text(json.member(index, "fault"));
                if (fault.size() != 2 || fault[0] != 'F' || fault[1] < '1' || fault[1] > '8')
                    return false;
                event.fault = static_cast<unsigned>(fault[1] - '0');
                if (!number(json, index, "t_s", event.at) || event.at < 0 ||
                    !integer(json, index, "duration_ms", event.duration_ms, true) ||
                    event.duration_ms > 60000)
                    return false;
                const auto parameters = json.member(index, "params");
                if (parameters != BufferJson::missing) {
                    if (!integer(json, parameters, "lag_ms", event.lag_ms, true) ||
                        event.lag_ms > 5000 ||
                        !integer(json, parameters, "rate_hz", event.rate_hz, true) ||
                        event.rate_hz < 1 || event.rate_hz > 5000)
                        return false;
                    const auto velocity = json.member(parameters, "v_cmd");
                    if (velocity != BufferJson::missing &&
                        (!json.number(velocity, event.v_cmd) || !bounded(event.v_cmd, -1.5, 1.5)))
                        return false;
                    const auto feed = json.member(parameters, "feed");
                    if (feed != BufferJson::missing) {
                        const auto text = json.text(feed);
                        if (text != "supervisor" && text != "planner")
                            return false;
                        event.supervisor_feed = text == "supervisor";
                    }
                }
                return true;
            },
            spec.fault_count))
        return false;
    auto before = [](const auto &a, const auto &b) { return a.at < b.at; };
    std::stable_sort(spec.obstacles.begin(), spec.obstacles.begin() + spec.obstacle_count, before);
    std::stable_sort(spec.zones.begin(), spec.zones.begin() + spec.zone_count, before);
    std::stable_sort(spec.faults.begin(), spec.faults.begin() + spec.fault_count, before);
    output = spec;
    return true;
}
} // namespace guard
