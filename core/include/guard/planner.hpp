#pragma once
#include "guard/scenario.hpp"

namespace guard {
enum class PlannerMode { nominal, lag, flood, shortcut, aggressive, replay };
struct PlannerOptions {
    PlannerMode mode = PlannerMode::nominal;
    std::uint32_t lag_ms = 600, rate_hz = 10;
    double cruise_v = 0.8;
    std::uint32_t lease_req_ms = 250;
};
// Receives only planner observations and the static route; no supervisor state.
class Planner {
  public:
    explicit Planner(MapSpec map, Config config = {}, std::uint32_t id = 1);
    bool observe(const Observation &observation);
    bool propose(std::int64_t now, const PlannerOptions &options, Proposal &output);
    bool at_goal() const;
    // Consume the C1 reply to our latest proposal, never supervisor state.
    void decision(const Decision &reply);

  private:
    MapSpec map_;
    Config config_;
    std::uint32_t id_;
    std::uint32_t planned_map_version_ = 0;
    bool retry_nominal_ = false;
    std::array<Observation, 64> history_{};
    std::size_t count_ = 0, next_ = 0, waypoint_ = 0;
    std::uint64_t sequence_ = 0;
    std::int64_t last_proposal_ = -1;
    Proposal previous_{};
};
} // namespace guard
