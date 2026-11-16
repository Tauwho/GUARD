#pragma once
#include "guard/plant.hpp"
#include <string_view>

namespace guard {
using Name = std::array<char, 33>;
std::string_view name_view(const Name &name);
struct Window {
    double from = 0, to = 601;
};
struct Crossing {
    Name id{};
    Point from, to;
    double speed = 1, radius = 0.3;
};
struct MapSpec {
    Name id{};
    Scene scene;
    Pose start, goal;
    std::array<Name, 32> zone_names{};
    std::array<std::array<Window, 8>, 32> zone_windows{};
    std::array<std::size_t, 32> window_counts{};
    std::array<Point, 16> waypoints{};
    std::size_t waypoint_count = 0;
    std::array<Crossing, 16> crossings{};
    std::size_t crossing_count = 0;
};
enum class ObstacleAction { spawn, despawn, crossing, stop };
struct ObstacleEvent {
    double at = 0;
    Name id{};
    ObstacleAction action = ObstacleAction::spawn;
    Point position;
    double radius = 0.3;
    std::size_t crossing = 0;
};
struct ZoneEvent {
    double at = 0;
    std::size_t zone = 0;
    bool active = false;
};
struct FaultEvent {
    double at = 0;
    std::uint32_t duration_ms = 0, lag_ms = 600, rate_hz = 1000;
    unsigned fault = 0;
    bool supervisor_feed = false;
    double v_cmd = 1.2;
};
struct ScenarioSpec {
    unsigned id = 0;
    std::uint64_t seed = 0;
    double duration = 60, cruise_v = 0.8;
    std::uint32_t lease_req_ms = 250, rate_hz = 20;
    enum class Planner { rule, aggressive, shortcut } planner = Planner::rule;
    std::array<ObstacleEvent, 64> obstacles{};
    std::array<ZoneEvent, 32> zones{};
    std::array<FaultEvent, 16> faults{};
    std::size_t obstacle_count = 0, zone_count = 0, fault_count = 0;
};
// Loaders accept schema-validated JSON buffers normalized from the shared YAML files.
bool load_map(std::string_view buffer, MapSpec &output);
bool load_scenario(std::string_view buffer, const MapSpec &map, ScenarioSpec &output);

class World {
  public:
    World(MapSpec map, ScenarioSpec scenario);
    bool update(std::int64_t now);
    const Scene &scene() const { return scene_; }
    const MapSpec &map() const { return map_; }
    const ScenarioSpec &scenario() const { return scenario_; }
    const FaultEvent *active_fault(unsigned fault, std::int64_t now) const;

  private:
    struct Actor {
        Name id{};
        Point from, to;
        double speed = 0, radius = 0.3, began = 0;
        bool active = false;
    };
    MapSpec map_;
    ScenarioSpec scenario_;
    Scene scene_;
    std::array<Actor, 16> actors_{};
    std::array<int, 32> zone_overrides_{};
    std::size_t next_obstacle_ = 0, next_zone_ = 0;
    std::int64_t last_update_ = -1;
};
} // namespace guard
