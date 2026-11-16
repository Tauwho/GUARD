#pragma once
#include "guard/config.hpp"
#include "guard/reason_codes.hpp"
#include "guard/types.hpp"

namespace guard {
enum class GateMode : std::uint16_t { init, latched, ready, moving, stopping, stopped };
enum class LatchCause : std::uint16_t {
    none,
    boot,
    supervisor_lost,
    epoch_changed,
    zone_breach,
    sensor_lost,
    motor_timeout,
    internal
};
struct RecoveryConditions {
    bool outside_zones = false;
    bool observations_fresh = false;
    bool cause_cleared = false;
};
struct GateStatus {
    GateMode mode = GateMode::init;
    Mode experiment = Mode::guarded;
    LatchCause latch = LatchCause::none;
    std::uint32_t epoch = 0;
    std::uint64_t lease_id = 0, lease_seq = 0;
    std::int64_t lease_remaining_ns = 0;
    std::uint32_t recovery_unmet_mask = 0;
    std::array<std::uint64_t, 700> counters{};
};

class Gate {
  public:
    explicit Gate(Config config = {});
    bool register_supervisor(RegisterReply &reply);
    LeaseAck prepare_lease(const Lease &value, std::int64_t now);
    bool commit_lease(std::uint64_t sequence, std::int64_t now, bool ack_delivered);
    LeaseAck lease(const Lease &value, std::int64_t now);
    // 514/504 map to EOK; 505 to EPERM; 200 to EINVAL in the platform adapter.
    Reason passthrough(const PassthruCmd &value, std::int64_t now);
    bool revoke(const Revoke &value);
    RecoverReply recover(const RecoverRequest &request, RecoveryConditions conditions);
    MotorCmd tick(std::int64_t now, double v_meas, double w_meas = 0);
    void supervisor_lost();
    void motor_timeout();
    GateStatus status(std::int64_t now) const;

  private:
    void record(Reason reason);
    void stop(Reason reason, LatchCause latch = LatchCause::none);
    void expire(std::int64_t now);
    Config config_;
    GateStatus status_{};
    Lease lease_{};
    Lease pending_{};
    bool pending_valid_ = false;
    bool registered_ = false;
    MotorCmd motor_{};
    PassthruCmd held_{};
    std::int64_t last_passthrough_ = -1;
    std::uint32_t standstill_ticks_ = 0;
    std::int64_t last_tick_ = -1;
};
} // namespace guard
