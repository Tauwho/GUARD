#include "guard/scenario.hpp"
#include <algorithm>
#include <cmath>

namespace guard {
World::World(MapSpec map, ScenarioSpec scenario)
    : map_(map), scenario_(scenario), scene_(map.scene) {
    zone_overrides_.fill(-1);
}

const FaultEvent *World::active_fault(unsigned fault, std::int64_t now) const {
    if (now < 0 || scenario_.fault_count > scenario_.faults.size())
        return nullptr;
    const double time = now / 1e9;
    for (std::size_t i = 0; i < scenario_.fault_count; ++i) {
        const auto &event = scenario_.faults[i];
        if (event.fault == fault && time >= event.at &&
            (event.duration_ms == 0 || time < event.at + event.duration_ms / 1000.0))
            return &event;
    }
    return nullptr;
}
bool World::update(std::int64_t now) {
    if (now < 0 || now < last_update_ || !valid(scene_) ||
        scenario_.obstacle_count > scenario_.obstacles.size() ||
        scenario_.zone_count > scenario_.zones.size() ||
        scenario_.fault_count > scenario_.faults.size())
        return false;
    if (map_.crossing_count > map_.crossings.size())
        return false;
    for (std::size_t z = 0; z < scene_.zone_count; ++z)
        if (map_.window_counts[z] > map_.zone_windows[z].size())
            return false;
    last_update_ = now;
    const double time = now / 1e9;
    while (next_zone_ < scenario_.zone_count && scenario_.zones[next_zone_].at <= time) {
        const auto &event = scenario_.zones[next_zone_++];
        if (event.zone >= scene_.zone_count)
            return false;
        zone_overrides_[event.zone] = event.active ? 1 : 0;
    }
    for (std::size_t z = 0; z < scene_.zone_count; ++z) {
        bool active = map_.scene.zones[z].active;
        if (map_.window_counts[z] > 0) {
            active = false;
            for (std::size_t w = 0; w < map_.window_counts[z]; ++w) {
                const auto &window = map_.zone_windows[z][w];
                active = active || (time >= window.from && time < window.to);
            }
        }
        if (zone_overrides_[z] >= 0)
            active = zone_overrides_[z] != 0;
        if (scene_.zones[z].active != active) {
            scene_.zones[z].active = active;
            ++scene_.map_version;
        }
    }
    while (next_obstacle_ < scenario_.obstacle_count &&
           scenario_.obstacles[next_obstacle_].at <= time) {
        const auto &event = scenario_.obstacles[next_obstacle_++];
        Actor *actor = nullptr;
        for (auto &existing : actors_)
            if (existing.id == event.id)
                actor = &existing;
        if (!actor)
            for (auto &existing : actors_)
                if (!existing.active) {
                    actor = &existing;
                    break;
                }
        if (!actor)
            return false;
        if (event.action == ObstacleAction::despawn) {
            actor->active = false;
            continue;
        }
        if (event.action == ObstacleAction::stop) {
            const double length =
                std::hypot(actor->to.x - actor->from.x, actor->to.y - actor->from.y);
            const double fraction =
                length == 0
                    ? 0
                    : std::clamp((event.at - actor->began) * actor->speed / length, 0.0, 1.0);
            actor->from = {actor->from.x + fraction * (actor->to.x - actor->from.x),
                           actor->from.y + fraction * (actor->to.y - actor->from.y)};
            actor->to = actor->from;
            actor->speed = 0;
            continue;
        }
        *actor = {};
        actor->id = event.id;
        actor->active = true;
        actor->began = event.at;
        actor->from = actor->to = event.position;
        actor->radius = event.radius;
        if (event.action == ObstacleAction::crossing) {
            if (event.crossing >= map_.crossing_count)
                return false;
            const auto &crossing = map_.crossings[event.crossing];
            actor->from = crossing.from;
            actor->to = crossing.to;
            actor->speed = crossing.speed;
            actor->radius = crossing.radius;
        }
    }
    scene_.obstacle_count = 0;
    for (auto &actor : actors_) {
        if (!actor.active)
            continue;
        const double length = std::hypot(actor.to.x - actor.from.x, actor.to.y - actor.from.y);
        const double distance = (time - actor.began) * actor.speed;
        if (actor.speed > 0 && distance >= length) {
            actor.active = false;
            continue;
        }
        const double fraction = length == 0 ? 0 : std::clamp(distance / length, 0.0, 1.0);
        scene_.obstacles[scene_.obstacle_count++] = {
            actor.from.x + fraction * (actor.to.x - actor.from.x),
            actor.from.y + fraction * (actor.to.y - actor.from.y), actor.radius};
    }
    return true;
}
} // namespace guard
