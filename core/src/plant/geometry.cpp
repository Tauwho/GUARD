#include "guard/plant.hpp"
#include <algorithm>
#include <cmath>

namespace guard {
double segment_distance(Point a, Point b, Point p) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double length2 = dx * dx + dy * dy;
    const double t =
        length2 == 0 ? 0 : std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length2, 0.0, 1.0);
    return std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}
double segment_distance(Point a, Point b, const Rect &rect) {
    double first = 0, last = 1;
    bool intersects = true;
    const double start[] = {a.x, a.y}, delta[] = {b.x - a.x, b.y - a.y};
    const double low[] = {rect.min.x, rect.min.y}, high[] = {rect.max.x, rect.max.y};
    for (int axis = 0; axis < 2; ++axis) {
        if (delta[axis] == 0) {
            intersects = intersects && start[axis] >= low[axis] && start[axis] <= high[axis];
        } else {
            double enter = (low[axis] - start[axis]) / delta[axis];
            double leave = (high[axis] - start[axis]) / delta[axis];
            if (enter > leave)
                std::swap(enter, leave);
            first = std::max(first, enter);
            last = std::min(last, leave);
        }
    }
    if (intersects && first <= last)
        return 0;
    auto point_distance = [&](Point p) {
        return std::hypot(std::max({rect.min.x - p.x, 0.0, p.x - rect.max.x}),
                          std::max({rect.min.y - p.y, 0.0, p.y - rect.max.y}));
    };
    double result = std::min(point_distance(a), point_distance(b));
    for (Point p :
         {rect.min, rect.max, Point{rect.min.x, rect.max.y}, Point{rect.max.x, rect.min.y}})
        result = std::min(result, segment_distance(a, b, p));
    return result;
}
GroundTruth swept_ground_truth(Point from, Point to, double radius, const Scene &scene) {
    if (!valid(scene) || !std::isfinite(radius) || radius < 0 || !std::isfinite(from.x) ||
        !std::isfinite(from.y) || !std::isfinite(to.x) || !std::isfinite(to.y)) {
        return {true, true, -1};
    }
    GroundTruth result;
    for (std::size_t i = 0; i < scene.obstacle_count; ++i) {
        const auto &obstacle = scene.obstacles[i];
        const double gap =
            segment_distance(from, to, Point{obstacle.x, obstacle.y}) - radius - obstacle.r;
        result.clearance = std::min(result.clearance, gap);
        result.contact = result.contact || gap <= 0;
    }
    for (std::size_t i = 0; i < scene.wall_count; ++i) {
        const double gap = segment_distance(from, to, scene.walls[i]) - radius;
        result.clearance = std::min(result.clearance, gap);
        result.contact = result.contact || gap <= 0;
    }
    for (std::size_t i = 0; i < scene.zone_count; ++i) {
        const auto &zone = scene.zones[i];
        if (!zone.active || !zone.restricted)
            continue;
        const double gap = segment_distance(from, to, zone.rect) - radius;
        result.clearance = std::min(result.clearance, gap);
        result.zone_breach = result.zone_breach || gap < 0;
    }
    return result;
}
bool valid(const Scene &scene) {
    if (scene.wall_count > scene.walls.size() || scene.zone_count > scene.zones.size() ||
        scene.obstacle_count > scene.obstacles.size() || scene.map_version == 0)
        return false;
    auto rectangle_valid = [](const Rect &r) {
        return std::isfinite(r.min.x) && std::isfinite(r.min.y) && std::isfinite(r.max.x) &&
               std::isfinite(r.max.y) && r.min.x <= r.max.x && r.min.y <= r.max.y;
    };
    for (std::size_t i = 0; i < scene.wall_count; ++i)
        if (!rectangle_valid(scene.walls[i]))
            return false;
    for (std::size_t i = 0; i < scene.zone_count; ++i)
        if (!rectangle_valid(scene.zones[i].rect))
            return false;
    for (std::size_t i = 0; i < scene.obstacle_count; ++i) {
        const auto &o = scene.obstacles[i];
        if (!std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.r) || o.r < 0.05 ||
            o.r > 2)
            return false;
    }
    return true;
}
} // namespace guard
