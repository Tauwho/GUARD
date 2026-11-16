#include "guard/plant.hpp"
#include "guard/codec.hpp"
#include <algorithm>
#include <cmath>

namespace guard {
namespace {
double approach_velocity(double current, double target, const Config &config) {
    const bool reversing = current * target < 0;
    if (reversing)
        target = 0;
    const double rate =
        (!reversing && std::abs(target) > std::abs(current)) ? config.a_max : config.a_brake;
    const double change = rate * config.t_tick_ms / 1000.0;
    return current + std::clamp(target - current, -change, change);
}
bool finite_pose(Pose pose) {
    return std::isfinite(pose.x) && std::isfinite(pose.y) && std::isfinite(pose.theta);
}
} // namespace

Plant::Plant(Config config, Pose start, std::int64_t now)
    : config_(config), pose_(start), last_step_(now), last_motor_(now) {}

bool Plant::command(const MotorCmd &value, std::int64_t now) {
    std::array<std::uint8_t, sizeof(MotorCmd)> packet{};
    if (!valid(config_) || !encode(value, packet) || now < last_motor_ || now < last_step_ ||
        value.stamp_ns > now || value.cmd_seq <= command_.cmd_seq)
        return false;
    command_ = value;
    last_motor_ = now;
    watchdog_ = false;
    return true;
}

bool Plant::step(std::int64_t now, const Scene &scene) {
    if (!valid(config_) || !valid(scene) || !finite_pose(pose_) || now <= last_step_ || now < 0)
        return false;
    const double dt = config_.t_tick_ms / 1000.0;
    watchdog_ = now - last_motor_ >= milliseconds(config_.actuator_watchdog_ms);
    const double target_v = watchdog_ ? 0 : command_.v;
    const double target_w = watchdog_ ? 0 : command_.w;
    const double next_v = approach_velocity(v_, target_v, config_);
    const double average_v = (v_ + next_v) / 2;
    const Point from{pose_.x, pose_.y};
    const double turn = target_w * dt;
    double sagitta = 0;
    if (std::abs(target_w) < 1e-10) {
        pose_.x += average_v * dt * std::cos(pose_.theta);
        pose_.y += average_v * dt * std::sin(pose_.theta);
    } else {
        const double radius = average_v / target_w;
        pose_.x += radius * (std::sin(pose_.theta + turn) - std::sin(pose_.theta));
        pose_.y -= radius * (std::cos(pose_.theta + turn) - std::cos(pose_.theta));
        sagitta = std::abs(radius) * (1 - std::cos(turn / 2));
    }
    pose_.theta = std::remainder(pose_.theta + turn, 2 * 3.14159265358979323846);
    v_ = next_v;
    w_ = target_w;
    truth_ = swept_ground_truth(from, {pose_.x, pose_.y}, config_.r_robot_m + sagitta, scene);
    last_step_ = now;
    return true;
}

Observation Plant::sample(std::int64_t now, const Scene &scene, bool sensor_ok) {
    Observation result;
    result.obs_id = ++observation_id_;
    result.seq = result.obs_id;
    result.stamp_ns = now;
    result.x = pose_.x;
    result.y = pose_.y;
    result.theta = pose_.theta;
    result.v_meas = v_;
    result.w_meas = w_;
    result.map_version = scene.map_version;
    result.sensor_ok = sensor_ok && valid(scene) && finite_pose(pose_);
    if (valid(scene)) {
        result.obstacle_count = static_cast<std::uint8_t>(scene.obstacle_count);
        result.obstacles = scene.obstacles;
        for (std::size_t i = scene.obstacle_count; i < result.obstacles.size(); ++i)
            result.obstacles[i] = {};
    }
    return result;
}
} // namespace guard
