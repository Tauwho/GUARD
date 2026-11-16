#include "guard/gate_fsm.hpp"
#include "guard/codec.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace guard {
Gate::Gate(Config config) : config_(config) {
    status_.experiment = config.mode;
    status_.mode = config.mode == Mode::guarded ? GateMode::latched : GateMode::ready;
    status_.latch = config.mode == Mode::guarded ? LatchCause::boot : LatchCause::none;
}
void Gate::record(Reason reason) { ++status_.counters[static_cast<unsigned>(reason)]; }
void Gate::stop(Reason reason, LatchCause latch) {
    status_.mode = GateMode::stopping;
    if (latch != LatchCause::none)
        status_.latch = latch;
    status_.lease_id = 0;
    standstill_ticks_ = 0;
    record(reason);
}
bool Gate::register_supervisor(RegisterReply &reply) {
    if (config_.mode != Mode::guarded) {
        record(Reason::GATE_UNAUTHORIZED_CLIENT);
        return false;
    }
    if (!valid(config_) || status_.epoch == std::numeric_limits<std::uint32_t>::max())
        return false;
    ++status_.epoch;
    registered_ = true;
    pending_valid_ = false;
    if (status_.mode == GateMode::moving)
        stop(Reason::GATE_EPOCH_CHANGED, LatchCause::epoch_changed);
    reply.epoch = status_.epoch;
    reply.gate_mode = static_cast<std::uint16_t>(status_.mode);
    reply.last_lease_seq = status_.lease_seq;
    return true;
}
void Gate::expire(std::int64_t now) {
    if (config_.mode == Mode::heartbeat && status_.mode == GateMode::moving &&
        last_passthrough_ >= 0 &&
        now - last_passthrough_ >= milliseconds(config_.heartbeat_timeout_ms))
        stop(Reason::GATE_HEARTBEAT_LOST);
    if (config_.mode == Mode::guarded && status_.mode == GateMode::moving &&
        !config_.ablate_expiry && now >= lease_.expires_ns)
        stop(Reason::GATE_LEASE_EXPIRED);
}
LeaseAck Gate::prepare_lease(const Lease &value, std::int64_t now) {
    expire(now);
    pending_valid_ = false;
    Reason reason = Reason::GATE_LEASE_ACCEPTED;
    std::array<std::uint8_t, sizeof(Lease)> packet{};
    if (config_.mode != Mode::guarded)
        reason = Reason::GATE_UNAUTHORIZED_CLIENT;
    else if (!valid(config_) || !encode(value, packet) || !registered_ || now < 0 ||
             now < last_tick_ || value.issued_ns > now)
        reason = Reason::GATE_NOT_READY;
    else if (!config_.ablate_epoch && value.epoch != status_.epoch)
        reason = Reason::GATE_EPOCH_STALE;
    else if (value.lease_seq <= status_.lease_seq)
        reason = Reason::GATE_LEASE_REPLAY;
    else {
        status_.lease_seq = value.lease_seq;
        if (!config_.ablate_expiry && now >= value.expires_ns)
            reason = Reason::GATE_LEASE_EXPIRED_ON_ARRIVAL;
        else if (status_.mode != GateMode::ready && status_.mode != GateMode::moving)
            reason = Reason::GATE_NOT_READY;
        else {
            pending_ = value;
            pending_valid_ = true;
        }
    }
    if (reason != Reason::GATE_LEASE_ACCEPTED)
        record(reason);
    LeaseAck reply;
    reply.lease_id = value.lease_id;
    reply.reason = static_cast<std::uint16_t>(reason);
    reply.gate_mode = static_cast<std::uint16_t>(pending_valid_ ? GateMode::moving : status_.mode);
    reply.epoch = status_.epoch;
    return reply;
}
bool Gate::commit_lease(std::uint64_t sequence, std::int64_t now, bool ack_delivered) {
    if (!pending_valid_ || sequence != pending_.lease_seq)
        return false;
    pending_valid_ = false;
    if (!ack_delivered) {
        record(Reason::GATE_ACK_UNDELIVERED);
        return false;
    }
    expire(now);
    if (now < last_tick_ || (!config_.ablate_epoch && pending_.epoch != status_.epoch) ||
        (!config_.ablate_expiry && now >= pending_.expires_ns) ||
        (status_.mode != GateMode::ready && status_.mode != GateMode::moving))
        return false;
    lease_ = pending_;
    status_.lease_id = lease_.lease_id;
    status_.mode = GateMode::moving;
    record(Reason::GATE_LEASE_ACCEPTED);
    return true;
}
LeaseAck Gate::lease(const Lease &value, std::int64_t now) {
    auto reply = prepare_lease(value, now);
    if (reply.reason == static_cast<std::uint16_t>(Reason::GATE_LEASE_ACCEPTED))
        commit_lease(value.lease_seq, now, true);
    return reply;
}
bool Gate::revoke(const Revoke &value) {
    if (config_.mode != Mode::guarded) {
        record(Reason::GATE_UNAUTHORIZED_CLIENT);
        return false;
    }
    if (!valid(value) || value.epoch != status_.epoch)
        return false;
    if (status_.mode != GateMode::moving ||
        (value.lease_id != 0 && value.lease_id != status_.lease_id))
        return true;
    const bool sensor_fault = value.reason == 400 || value.reason == 404;
    stop(Reason::GATE_REVOKED, sensor_fault ? LatchCause::sensor_lost : LatchCause::none);
    return true;
}
RecoverReply Gate::recover(const RecoverRequest &request, RecoveryConditions conditions) {
    RecoverReply reply;
    reply.request_id = request.request_id;
    reply.reason = static_cast<std::uint16_t>(Reason::GATE_NOT_READY);
    if (valid(request) && status_.mode == GateMode::latched) {
        std::uint32_t mask = 0;
        if (standstill_ticks_ < config_.standstill_ticks)
            mask |= 1u;
        if (!conditions.outside_zones)
            mask |= 2u;
        if (config_.mode == Mode::guarded && !registered_)
            mask |= 4u;
        if (!conditions.observations_fresh)
            mask |= 8u;
        if (!conditions.cause_cleared)
            mask |= 16u;
        status_.recovery_unmet_mask = mask;
        reply.unmet_mask = mask;
        reply.reason =
            static_cast<std::uint16_t>(mask ? Reason::GATE_RECOVERY_UNMET : Reason::GATE_RECOVERED);
        if (!mask) {
            status_.mode = GateMode::ready;
            status_.latch = LatchCause::none;
        }
        record(static_cast<Reason>(reply.reason));
    }
    reply.gate_mode = static_cast<std::uint16_t>(status_.mode);
    return reply;
}
void Gate::supervisor_lost() {
    if (config_.mode != Mode::guarded)
        return;
    registered_ = false;
    pending_valid_ = false;
    if (status_.mode == GateMode::moving)
        stop(Reason::GATE_SUPERVISOR_LOST, LatchCause::supervisor_lost);
    else {
        status_.latch = LatchCause::supervisor_lost;
        if (status_.mode != GateMode::stopping)
            status_.mode = GateMode::latched;
    }
}
void Gate::motor_timeout() {
    if (status_.mode == GateMode::moving)
        stop(Reason::GATE_MOTOR_TIMEOUT, LatchCause::motor_timeout);
}
MotorCmd Gate::tick(std::int64_t now, double v_meas, double w_meas) {
    if (now < 0 || now <= last_tick_)
        return motor_;
    last_tick_ = now;
    const bool measured_valid = std::isfinite(v_meas) && std::isfinite(w_meas);
    if (!valid(config_) || !measured_valid) {
        stop(Reason::GATE_REVOKED, LatchCause::internal);
        v_meas = motor_.v;
        w_meas = motor_.w;
    }
    expire(now);
    if (measured_valid && std::abs(v_meas) <= config_.standstill_v &&
        std::abs(w_meas) <= config_.standstill_v)
        standstill_ticks_ = std::min(standstill_ticks_ + 1, config_.standstill_ticks);
    else
        standstill_ticks_ = 0;
    ++motor_.cmd_seq;
    motor_.stamp_ns = now;
    if (status_.mode == GateMode::moving) {
        motor_.v = config_.mode == Mode::guarded
                       ? std::clamp(lease_.v_cmd, -lease_.v_cap, lease_.v_cap)
                       : held_.v;
        motor_.w = config_.mode == Mode::guarded ? lease_.w_cmd : held_.w;
    } else if (status_.mode == GateMode::stopping) {
        const double speed = std::min(std::abs(v_meas), std::abs(motor_.v));
        motor_.v = std::copysign(
            std::max(0.0, speed - config_.a_brake * config_.t_tick_ms / 1000.0), v_meas);
        motor_.w = 0;
        if (standstill_ticks_ >= config_.standstill_ticks) {
            status_.mode = GateMode::stopped;
            record(Reason::GATE_STANDSTILL);
        }
    } else {
        motor_.v = motor_.w = 0;
        if (status_.mode == GateMode::stopped)
            status_.mode = status_.latch == LatchCause::none ? GateMode::ready : GateMode::latched;
    }
    motor_.gate_mode = static_cast<std::uint16_t>(status_.mode);
    return motor_;
}
GateStatus Gate::status(std::int64_t now) const {
    auto result = status_;
    if (config_.mode == Mode::guarded && result.mode == GateMode::moving)
        result.lease_remaining_ns = std::max<std::int64_t>(0, lease_.expires_ns - now);
    return result;
}
Reason Gate::passthrough(const PassthruCmd &value, std::int64_t now) {
    if (config_.mode == Mode::guarded) {
        record(Reason::GATE_UNAUTHORIZED_CLIENT);
        return Reason::GATE_UNAUTHORIZED_CLIENT;
    }
    std::array<std::uint8_t, sizeof(PassthruCmd)> packet{};
    if (!valid(config_) || !encode(value, packet) || value.cmd_seq <= held_.cmd_seq || now < 0 ||
        now < last_tick_ || now < last_passthrough_ || value.stamp_ns > now)
        return Reason::REJ_SCHEMA;
    expire(now);
    held_.cmd_seq = value.cmd_seq;
    if (status_.mode != GateMode::ready && status_.mode != GateMode::moving) {
        record(Reason::GATE_NOT_READY);
        return Reason::GATE_NOT_READY;
    }
    held_ = value;
    last_passthrough_ = now;
    status_.mode = GateMode::moving;
    record(Reason::GATE_PASSTHRU_ACCEPTED);
    return Reason::GATE_PASSTHRU_ACCEPTED;
}
} // namespace guard
