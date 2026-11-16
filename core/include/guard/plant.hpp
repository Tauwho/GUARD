#pragma once
#include "guard/config.hpp"
#include "guard/types.hpp"

namespace guard {
struct Point {
    double x = 0, y = 0;
};
struct Pose {
    double x = 0, y = 0, theta = 0;
};
struct Rect {
    Point min, max;
};
struct Zone {
    Rect rect;
    bool active = true;
    bool restricted = true;
};
struct Scene {
    std::array<Rect, 64> walls{};
    std::array<Zone, 32> zones{};
    std::array<Obstacle, 16> obstacles{};
    std::size_t wall_count = 0, zone_count = 0, obstacle_count = 0;
    std::uint32_t map_version = 1;
};
struct GroundTruth {
    bool contact = false;
    bool zone_breach = false;
    double clearance = 1000;
};
double segment_distance(Point from, Point to, Point point);
double segment_distance(Point from, Point to, const Rect &rectangle);
GroundTruth swept_ground_truth(Point from, Point to, double radius, const Scene &scene);
bool valid(const Scene &scene);

class Plant {
  public:
    explicit Plant(Config config = {}, Pose start = {}, std::int64_t now = 0);
    bool command(const MotorCmd &command, std::int64_t now);
    bool step(std::int64_t now, const Scene &scene);
    Observation sample(std::int64_t now, const Scene &scene, bool sensor_ok = true);
    Pose pose() const { return pose_; }
    double linear_velocity() const { return v_; }
    double angular_velocity() const { return w_; }
    bool watchdog_active() const { return watchdog_; }
    GroundTruth ground_truth() const { return truth_; }

  private:
    Config config_;
    Pose pose_;
    MotorCmd command_{};
    std::int64_t last_step_, last_motor_;
    double v_ = 0, w_ = 0;
    std::uint64_t observation_id_ = 0;
    bool watchdog_ = false;
    GroundTruth truth_{};
};
} // namespace guard
