#pragma once
#include "guard/codec.hpp"
#include "guard/plant.hpp"
#include "guard/reason_codes.hpp"
#include "guard/route.hpp"

namespace guard {
struct SafetyEvaluation {
    bool authorized = false;
    Reason primary_reason = Reason::UNV_INTERNAL;
    std::uint64_t reasons_mask = 0;
    std::array<std::uint16_t, 11> reasons{};
    double required_distance = 0;
    double free_distance = 0;
};
struct StoppingCheck {
    double required = 0;
    double available = 0;
    bool budget_exceeded = false;
};
StoppingCheck stopping_check(const Observation &observation, const Proposal &proposal,
                             const Scene &scene, const Config &config);

class Supervisor {
  public:
    explicit Supervisor(Config config = {}) : config_(config) {
        if (valid(config_))
            route_capacity_ = (config_.route_max_age_ms * config_.obs_rate_hz + 999) / 1000;
    }
    bool observe(const Observation &observation, std::int64_t now);
    SafetyEvaluation evaluate(const Proposal &proposal, const Scene &scene, std::int64_t now);
    // Returns a 4xx reason when the caller must revoke, or zero when the lease may continue.
    std::uint16_t recheck(const Lease &lease, const Scene &scene, std::int64_t now);
    // Advisory admission only; never produces a Decision or grants a lease.
    std::uint16_t admit_route(const RouteProposal &route, const Scene &scene,
                              std::int64_t now) const;
    const Observation *latest() const;
    const Observation *find(std::uint64_t id) const;

  private:
    struct PlannerState {
        std::uint32_t id = 0;
        std::uint64_t last_sequence = 0;
        std::array<std::int64_t, 1000> arrivals{};
        std::size_t count = 0, next = 0;
    };
    struct RouteSource {
        std::uint64_t id = 0;
        std::int64_t stamp = 0;
        std::uint32_t map = 0;
        bool sensor_ok = false;
    };
    // Bounds follow valid(Config): 60 s at 100 Hz; active capacity is 100 by default.
    std::array<RouteSource, 6000> route_sources_{};
    std::size_t route_capacity_ = 0, route_count_ = 0, route_next_ = 0;
    Config config_;
    std::array<Observation, 64> history_{};
    std::size_t count_ = 0, next_ = 0;
    std::array<PlannerState, 16> planners_{};
    std::int64_t last_evaluation_ = -1;
    std::uint32_t unverifiable_count_ = 0;
};
} // namespace guard
