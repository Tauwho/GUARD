#pragma once
#include "guard/codec.hpp"
#include "guard/plant.hpp"
#include "guard/reason_codes.hpp"

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
    explicit Supervisor(Config config = {}) : config_(config) {}
    bool observe(const Observation &observation, std::int64_t now);
    SafetyEvaluation evaluate(const Proposal &proposal, const Scene &scene, std::int64_t now);
    // Returns a 4xx reason when the caller must revoke, or zero when the lease may continue.
    std::uint16_t recheck(const Lease &lease, const Scene &scene, std::int64_t now);
    const Observation *latest() const;
    const Observation *find(std::uint64_t id) const;

  private:
    struct PlannerState {
        std::uint32_t id = 0;
        std::uint64_t last_sequence = 0;
        std::array<std::int64_t, 1000> arrivals{};
        std::size_t count = 0, next = 0;
    };
    Config config_;
    std::array<Observation, 64> history_{};
    std::size_t count_ = 0, next_ = 0;
    std::array<PlannerState, 16> planners_{};
    std::int64_t last_evaluation_ = -1;
    std::uint32_t unverifiable_count_ = 0;
};
} // namespace guard
