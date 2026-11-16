#include "guard/supervisor.hpp"
#include <algorithm>
#include <cmath>

namespace guard {
const Observation *Supervisor::latest() const {
    return count_ == 0 ? nullptr : &history_[(next_ + history_.size() - 1) % history_.size()];
}
const Observation *Supervisor::find(std::uint64_t id) const {
    for (std::size_t offset = 0; offset < count_; ++offset) {
        const auto &observation =
            history_[(next_ + history_.size() - 1 - offset) % history_.size()];
        if (observation.obs_id == id)
            return &observation;
    }
    return nullptr;
}
bool Supervisor::observe(const Observation &observation, std::int64_t now) {
    std::array<std::uint8_t, sizeof(Observation)> packet{};
    const auto *current = latest();
    if (!valid(config_) || !encode(observation, packet) || observation.feed != 0 || now < 0 ||
        observation.stamp_ns > now ||
        (current && (observation.obs_id <= current->obs_id || observation.seq <= current->seq ||
                     observation.stamp_ns < current->stamp_ns)))
        return false;
    route_sources_[route_next_] = {observation.obs_id, observation.stamp_ns,
                                   observation.map_version, observation.sensor_ok != 0};
    route_next_ = (route_next_ + 1) % route_capacity_;
    route_count_ = std::min(route_count_ + 1, route_capacity_);
    history_[next_] = observation;
    next_ = (next_ + 1) % history_.size();
    count_ = std::min(count_ + 1, static_cast<std::size_t>(config_.obs_history));
    return true;
}
std::uint16_t Supervisor::admit_route(const RouteProposal &route, const Scene &scene,
                                      std::int64_t now) const {
    if (!valid(config_))
        return static_cast<std::uint16_t>(Reason::UNV_INTERNAL);
    const auto *current = latest();
    if (!current)
        return static_cast<std::uint16_t>(Reason::UNV_SENSOR_STALE);
    for (std::size_t i = 0; i < route_count_; ++i) {
        const auto &saved =
            route_sources_[(route_next_ + route_capacity_ - 1 - i) % route_capacity_];
        if (saved.id != route.observation_id)
            continue;
        Observation source;
        source.obs_id = source.seq = saved.id;
        source.stamp_ns = saved.stamp;
        source.map_version = saved.map;
        source.sensor_ok = saved.sensor_ok;
        return check_route(route, source, *current, scene, config_, now);
    }
    return static_cast<std::uint16_t>(Reason::REJ_UNKNOWN_OBSERVATION);
}
SafetyEvaluation Supervisor::evaluate(const Proposal &p, const Scene &scene, std::int64_t now) {
    SafetyEvaluation result;
    auto fail = [&](std::size_t checker, Reason reason) {
        result.reasons[checker - 1] = static_cast<std::uint16_t>(reason);
        result.reasons_mask |= std::uint64_t{1} << (checker - 1);
    };
    std::array<std::uint8_t, sizeof(Proposal)> packet{};
    const bool readable = encode(p, packet);
    if (!readable || now < 0 || p.created_ns > now)
        fail(1, Reason::REJ_SCHEMA);
    if (!valid(config_) || !valid(scene) || now < last_evaluation_)
        fail(8, Reason::UNV_INTERNAL);
    if (now >= 0)
        last_evaluation_ = std::max(now, last_evaluation_);
    PlannerState *planner = nullptr;
    for (auto &state : planners_)
        if (state.id == p.planner_id && state.id != 0)
            planner = &state;
    if (!planner && readable) {
        for (auto &state : planners_)
            if (state.id == 0) {
                state.id = p.planner_id;
                planner = &state;
                break;
            }
    }
    if (!planner)
        fail(2, Reason::REJ_RATE_LIMIT);
    else {
        std::size_t recent = 0;
        for (std::size_t i = 0; i < planner->count; ++i)
            if (now >= planner->arrivals[i] && now - planner->arrivals[i] < 1'000'000'000)
                ++recent;
        if (recent >= config_.rate_limit_hz)
            fail(2, Reason::REJ_RATE_LIMIT);
        if (p.seq <= planner->last_sequence)
            fail(3, Reason::REJ_SEQ_REPLAY);
        if (readable && now >= 0) {
            planner->last_sequence = std::max(planner->last_sequence, p.seq);
            planner->arrivals[planner->next] = now;
            planner->next = (planner->next + 1) % planner->arrivals.size();
            planner->count = std::min(planner->count + 1, planner->arrivals.size());
        }
    }
    const auto *source = find(p.based_on.obs_id);
    const auto *current = latest();
    if (!config_.ablate_freshness) {
        if (!source)
            fail(4, Reason::REJ_UNKNOWN_OBSERVATION);
        else {
            if (!source->sensor_ok || source->stamp_ns != p.based_on.obs_stamp_ns)
                fail(4, Reason::REJ_PROVENANCE_MISMATCH);
            if (now < source->stamp_ns ||
                now - source->stamp_ns > milliseconds(config_.max_obs_age_ms))
                fail(5, Reason::REJ_STALE_OBSERVATION);
            std::size_t newer = 0;
            for (std::size_t i = 0; i < count_; ++i)
                if (history_[(next_ + history_.size() - 1 - i) % history_.size()].obs_id >
                    source->obs_id)
                    ++newer;
            if (newer > config_.supersession_k)
                fail(6, Reason::REJ_SUPERSEDED_OBSERVATION);
        }
        if (!current || p.based_on.map_version != current->map_version ||
            p.based_on.map_version != scene.map_version)
            fail(7, Reason::REJ_MAP_VERSION);
    }
    const bool healthy = current && current->sensor_ok && now >= current->stamp_ns &&
                         now - current->stamp_ns <= milliseconds(config_.sensor_stale_ms);
    if (!healthy)
        fail(8, Reason::UNV_SENSOR_STALE);
    if (p.v_cmd < 0 || p.v_cmd > config_.v_max || std::abs(p.w_cmd) > config_.w_max ||
        (current && current->v_meas < -0.02))
        fail(9, Reason::REJ_SPEED_ENVELOPE);
    else if (current && p.v_cmd > current->v_meas + config_.a_max / config_.obs_rate_hz)
        fail(9, Reason::REJ_ACCEL_ENVELOPE);
    if (readable && current && valid(config_) && valid(scene)) {
        Scene world = scene;
        world.obstacle_count = current->obstacle_count;
        world.obstacles = current->obstacles;
        const auto stopping = stopping_check(*current, p, world, config_);
        result.required_distance = stopping.required;
        result.free_distance = stopping.available;
        if (stopping.budget_exceeded)
            fail(11, Reason::UNV_CHECK_BUDGET);
        else {
            Scene zones = world;
            zones.wall_count = zones.obstacle_count = 0;
            const auto swept = stopping_check(*current, p, zones, config_);
            if (swept.available < swept.required)
                fail(10, Reason::REJ_RESTRICTED_ZONE);
            if (!config_.ablate_stopping && stopping.available < stopping.required)
                fail(11, Reason::REJ_STOPPING_DISTANCE);
        }
    }
    std::uint16_t primary = 0;
    for (auto reason : result.reasons)
        if (reason >= 300 && reason < 400) {
            primary = reason;
            break;
        }
    if (primary == 0)
        for (auto reason : result.reasons)
            if (reason != 0) {
                primary = reason;
                break;
            }
    result.authorized = primary == 0;
    result.primary_reason =
        result.authorized ? Reason::OK_AUTHORIZED : static_cast<Reason>(primary);
    return result;
}
} // namespace guard
