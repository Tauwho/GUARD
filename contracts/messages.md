# GUARD wire messages, C1-C7 (contracts v1.0)

Status: **v1.0 draft**, frozen when the PR for #3 merges (target Fri 20 Nov 2026 23:59 SGT).
Source of truth for both streams. If this file and `docs/architecture.md` disagree, this file wins after the freeze.

## 1. Common rules

| Rule | Value |
|---|---|
| Byte order | Little-endian for every integer and float |
| Floats | IEEE-754 binary64 (`f64`). NaN and Inf are invalid in every field (`REJ_SCHEMA`) |
| Integers | `u8`, `u16`, `u32`, `u64` unsigned; `i64` signed two's complement |
| Booleans | `u8`, 0 = false, 1 = true. Any other value is invalid |
| Layout | Fixed-size POD structs, natural alignment, explicit `reserved` padding. No pointers, no variable-length parts |
| Reserved fields | Sender writes 0. Receiver ignores the value in v1 (reserved for later minor versions) |
| Strings | Fixed-size `char[N]`, UTF-8, NUL-padded. At least one NUL byte required |
| Time | `CLOCK_MONOTONIC` nanoseconds on the target (`i64`). The host harness uses virtual ns with the same meaning |
| Size checks | Every struct size is checked with `static_assert` on both host and target (A-2, B-3) |
| Version | `version = 1` for every message in contracts v1.0 |

### 1.1 Header (all binary messages)

| Offset | Field | Type | Value / range |
|---|---|---|---|
| 0 | `magic` | u16 | `0x4447` (bytes `47 44`, ASCII "GD") |
| 2 | `type` | u16 | Message type id, table 1.2 |
| 4 | `version` | u16 | `1` |
| 6 | `size` | u16 | Total message size in bytes, including the header |

Header size: **8 bytes**.

**Receiver checks, in order:** magic, type, version, size equals the fixed size for that type. The first failure decides the error (section 1.4).

### 1.2 Message type ids

| Type | Name | Contract | Size (bytes) |
|---|---|---|---|
| `0x0001` | `Observation` | C6 | 464 |
| `0x0002` | `Proposal` | C1 | 88 |
| `0x0003` | `Decision` | C1 | 56 |
| `0x0010` | `RegisterReply` | C2 | 24 |
| `0x0011` | `Lease` | C2 | 80 |
| `0x0012` | `LeaseAck` | C2 | 24 |
| `0x0013` | `Revoke` | C2 | 24 |
| `0x0014` | `PassthruCmd` | C2 (B0/B1 only) | 40 |
| `0x0020` | `RecoverRequest` | C3 | 16 |
| `0x0021` | `RecoverReply` | C3 | 24 |
| `0x0030` | `MotorCmd` | C5 | 48 |
| `0x0040` | `WorldLoad` | C7 | 120 |
| `0x0041` | `WorldInject` | C7 | 24 |
| `0x0042` | `WorldPause` | C7 | 16 |

Type ids are never reused. New types take the next free id in their contract's block.

### 1.3 Shared enums

**`gate_mode` (u16)**

| Value | Mode |
|---|---|
| 0 | `INIT` |
| 1 | `LATCHED` |
| 2 | `READY` |
| 3 | `MOVING` |
| 4 | `STOPPING` |
| 5 | `STOPPED` |

**`verdict` (u8)**

| Value | Verdict |
|---|---|
| 1 | `AUTHORIZED` |
| 2 | `REJECTED` |
| 3 | `UNVERIFIABLE` (treated as rejected) |

**`latch_cause` (u16, C4 text uses the name)**

| Value | Cause |
|---|---|
| 0 | `NONE` |
| 1 | `BOOT` |
| 2 | `SUPERVISOR_LOST` |
| 3 | `EPOCH_CHANGED` |
| 4 | `ZONE_BREACH` |
| 5 | `SENSOR_LOST` |
| 6 | `MOTOR_TIMEOUT` |
| 7 | `INTERNAL` |

**`recovery_unmet_mask` (u32).** A bit is set when that condition is **not** met.

| Bit | Condition |
|---|---|
| 0 | Standstill confirmed |
| 1 | Pose outside every active zone plus `margin` |
| 2 | Supervisor registered in the current epoch |
| 3 | Supervisor observations fresh (≤ sensor stale threshold) |
| 4 | Latch cause cleared |

Bits 5-31 are reserved and always 0.

**`reasons_mask` (u64).** Bit `n-1` is set when checker `n` (REF-CHECKERS order 1-11) did not pass. Bits 11-63 are reserved and always 0.

**`ablation_mask` (u8).** A set bit **removes** that protection (config C12 `ablate`).

| Bit | Ablation |
|---|---|
| 0 | `freshness` (checkers 4-7) |
| 1 | `stopping` (checker 11) |
| 2 | `expiry` (lease never expires) |
| 3 | `epoch` (gate skips the epoch check) |

**`mode` (u8):** 0 = `B0`, 1 = `B1`, 2 = `B2`.

### 1.4 Generic errors

| Failure | Message-passing reply (C1, C6) | `devctl` return (C2, C3, C5, C7) |
|---|---|---|
| Message shorter than the header | `MsgError(rcvid, EBADMSG)` | `EBADMSG` |
| Bad magic or unknown type | `MsgError(rcvid, EBADMSG)` | `EBADMSG` |
| `version != 1` | C1: `Decision` with `REJ_SCHEMA`; others `MsgError(rcvid, ENOTSUP)` | `ENOTSUP` |
| `size` wrong for the type | C1: `Decision` with `REJ_SCHEMA`; others `MsgError(rcvid, EMSGSIZE)` | `EMSGSIZE` |
| Field out of range, NaN or Inf | C1: `Decision` with `REJ_SCHEMA` | `EINVAL` |
| Caller uid not allowed | `MsgError(rcvid, EPERM)` | `EPERM` (gate also counts `GATE_UNAUTHORIZED_CLIENT`) |

A client that gets `ETIMEDOUT` from its `TimerTimeout` treats the call as failed. **A failed call never authorizes or extends motion.**

### 1.5 `devctl` command numbers

All commands use class `_DCMD_MISC` from `<devctl.h>`. Numbers are built with the QNX macros, so the data size is part of the command value.

| Name | Macro | Cmd byte | Data struct (size) | Contract |
|---|---|---|---|---|
| `DCMD_GUARD_REGISTER` | `__DIOF(_DCMD_MISC, 0xA0, guard_register_reply_t)` | `0xA0` | out: `RegisterReply` (24) | C2 |
| `DCMD_GUARD_LEASE` | `__DIOTF(_DCMD_MISC, 0xA1, guard_lease_t)` | `0xA1` | in: `Lease` (80); out: `LeaseAck` (24) in the same buffer | C2 |
| `DCMD_GUARD_REVOKE` | `__DIOT(_DCMD_MISC, 0xA2, guard_revoke_t)` | `0xA2` | in: `Revoke` (24) | C2 |
| `DCMD_GUARD_RECOVER` | `__DIOTF(_DCMD_MISC, 0xA3, guard_recover_t)` | `0xA3` | in: `RecoverRequest` (16); out: `RecoverReply` (24); buffer 24 | C3 |
| `DCMD_GUARD_PASSTHRU` | `__DIOT(_DCMD_MISC, 0xA4, guard_passthru_t)` | `0xA4` | in: `PassthruCmd` (40) | C2 (B0/B1) |
| `DCMD_SIM_MOTOR` | `__DIOT(_DCMD_MISC, 0xB0, guard_motor_cmd_t)` | `0xB0` | in: `MotorCmd` (48) | C5 |
| `DCMD_SIM_WORLD_LOAD` | `__DIOT(_DCMD_MISC, 0xB1, guard_world_load_t)` | `0xB1` | in: `WorldLoad` (120) | C7 |
| `DCMD_SIM_WORLD_INJECT` | `__DIOT(_DCMD_MISC, 0xB2, guard_world_inject_t)` | `0xB2` | in: `WorldInject` (24) | C7 |
| `DCMD_SIM_WORLD_PAUSE` | `__DIOT(_DCMD_MISC, 0xB3, guard_world_pause_t)` | `0xB3` | in: `WorldPause` (16) | C7 |

For `__DIOTF`, the caller passes a buffer of `max(in, out)` bytes. The server writes the reply struct at offset 0. `guard_recover_t` is a 24-byte union of `RecoverRequest` and `RecoverReply`; `guard_lease_t` is the 80-byte `Lease`, and the `LeaseAck` overwrites its first 24 bytes.

---

## 2. C1 - Proposal -> Decision

| Item | Value |
|---|---|
| Purpose | The untrusted planner asks for motion. The supervisor answers with a verdict and, if authorized, the lease it granted |
| Sender -> receiver | `guard-planner` -> `guard-supervisor` |
| Mechanism | `name_open("guard/supervisor")`, then `MsgSend(Proposal)` -> `MsgReply(Decision)` |
| Timeout | Client wraps every `MsgSend` in `TimerTimeout` of **50 ms** (send + reply blocked). On `ETIMEDOUT` the planner drops the proposal and sends a new one with a higher `seq` |
| Bounds | 1 proposal in flight per planner. Rate limit 50 proposals/s per `planner_id` (`REJ_RATE_LIMIT` beyond that) |
| Priority | Server thread inherits the planner priority (10). See ADR-0006 |
| Allowed callers | uid `guard_plan` (1104) or root. Others get `EPERM` |

### 2.1 `Proposal` (type `0x0002`, 88 bytes)

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0002`, size 88 |
| 8 | `proposal_id` | u64 | - | ≥ 1, unique per planner |
| 16 | `planner_id` | u32 | - | ≥ 1 |
| 20 | `lease_req_ms` | u32 | ms | 50-1000. The supervisor grants `min(lease_req_ms, T_lease)` |
| 24 | `seq` | u64 | - | ≥ 1, strictly increasing per `planner_id` |
| 32 | `based_on.obs_id` | u64 | - | ≥ 1; must be in the supervisor history (64) |
| 40 | `based_on.obs_seq` | u64 | - | ≥ 1. The `seq` the planner saw on its own feed. Logged for diagnosis only; checker 4 does not compare it |
| 48 | `based_on.obs_stamp_ns` | i64 | ns | ≥ 0; must equal the recorded stamp of `obs_id` |
| 56 | `based_on.map_version` | u32 | - | ≥ 1 |
| 60 | `reserved0` | u32 | - | 0 |
| 64 | `created_ns` | i64 | ns | ≥ `based_on.obs_stamp_ns` |
| 72 | `v_cmd` | f64 | m/s | -1.5 to 1.5 (schema bound; envelope `v_max` 1.2 is checker 9) |
| 80 | `w_cmd` | f64 | rad/s | -1.5 to 1.5 (schema bound; envelope `w_max` 1.0 is checker 9) |

Values outside the schema bounds give `REJ_SCHEMA`. Values inside the schema bounds but outside the envelope give `REJ_SPEED_ENVELOPE`.

### 2.2 `Decision` (type `0x0003`, 56 bytes)

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0003`, size 56 |
| 8 | `proposal_id` | u64 | - | Copied from the proposal (0 if the proposal header was unreadable) |
| 16 | `verdict` | u8 | - | 1-3 (section 1.3) |
| 17 | `reserved0` | u8 | - | 0 |
| 18 | `primary_reason` | u16 | - | A code from `reason_codes.yaml`: 100 if authorized, else 2xx or 3xx |
| 20 | `epoch` | u32 | - | Supervisor epoch in force; 0 if not registered |
| 24 | `reasons_mask` | u64 | - | Section 1.3 |
| 32 | `lease_id` | u64 | - | ≥ 1 if authorized, else 0 |
| 40 | `expires_ns` | i64 | ns | Lease expiry if authorized, else 0 |
| 48 | `decided_ns` | i64 | ns | ≥ 0 |

**Errors and special cases:**
- All 11 checkers run on every readable proposal. The primary reason is the first non-pass in order.
- If any checker is UNVERIFIABLE, `verdict = 3` and the primary reason is the first 3xx code.
- If checks pass but the gate refuses the lease, `verdict = 2` with `REJ_GATE_NOT_READY`.
- If checking exceeds its budget, `verdict = 3` with `UNV_CHECK_BUDGET`.

---

## 3. C2 - Register, lease, revoke

| Item | Value |
|---|---|
| Purpose | The supervisor registers an epoch, hands leases to the gate and revokes them |
| Sender -> receiver | `guard-supervisor` -> `guard-gate` |
| Mechanism | `open("/dev/guard/gate", O_RDWR)`, then `devctl` (section 1.5) |
| Timeout | Client `TimerTimeout` of **5 ms** on each `devctl`. On `ETIMEDOUT` the supervisor treats the lease as not granted |
| Allowed callers | uid `guard_sup` (1103) or root. Others get `EPERM` and the gate counts `GATE_UNAUTHORIZED_CLIENT` |
| Session rule | The gate tracks the OCB that registered. When that OCB closes while MOVING, the gate goes to STOPPING and latches `SUPERVISOR_LOST` |

### 3.1 `DCMD_GUARD_REGISTER` -> `RegisterReply` (type `0x0010`, 24 bytes)

No input. Each successful call starts a **new epoch** (`epoch = previous + 1`). If the gate is MOVING, it goes to STOPPING and latches `EPOCH_CHANGED`.

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0010`, size 24 |
| 8 | `epoch` | u32 | - | ≥ 1, new epoch for this supervisor |
| 12 | `gate_mode` | u16 | - | 0-5 |
| 14 | `reason` | u16 | - | 0 on success |
| 16 | `last_lease_seq` | u64 | - | Highest `lease_seq` the gate has seen. The supervisor's next lease must use a larger value |

### 3.2 `DCMD_GUARD_LEASE`: `Lease` (type `0x0011`, 80 bytes) -> `LeaseAck` (type `0x0012`, 24 bytes)

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0011`, size 80 |
| 8 | `lease_id` | u64 | - | ≥ 1 |
| 16 | `epoch` | u32 | - | ≥ 1 |
| 20 | `reserved0` | u32 | - | 0 |
| 24 | `lease_seq` | u64 | - | ≥ 1, strictly increasing across all epochs |
| 32 | `proposal_id` | u64 | - | ≥ 1 |
| 40 | `v_cmd` | f64 | m/s | -1.2 to 1.2 (`v_max`) |
| 48 | `w_cmd` | f64 | rad/s | -1.0 to 1.0 (`w_max`) |
| 56 | `v_cap` | f64 | m/s | 0 to 1.2; the gate clamps abs(v) to `v_cap` |
| 64 | `issued_ns` | i64 | ns | ≥ 0 |
| 72 | `expires_ns` | i64 | ns | `issued_ns` + 50 ms to `issued_ns` + 1000 ms |

`LeaseAck`:

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0012`, size 24 |
| 8 | `lease_id` | u64 | - | Copied from the lease |
| 16 | `reason` | u16 | - | `GATE_LEASE_ACCEPTED`, or one of `GATE_EPOCH_STALE`, `GATE_LEASE_REPLAY`, `GATE_LEASE_EXPIRED_ON_ARRIVAL`, `GATE_NOT_READY` |
| 18 | `gate_mode` | u16 | - | Mode after handling |
| 20 | `epoch` | u32 | - | Gate's current epoch |

`devctl` returns `EOK` whenever a `LeaseAck` was written, including refusals. The refusal is in `reason`. Gate checks run in the REF-FSM order R11, R12, R13, R14.

### 3.3 `DCMD_GUARD_REVOKE`: `Revoke` (type `0x0013`, 24 bytes)

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0013`, size 24 |
| 8 | `lease_id` | u64 | - | Lease to revoke; 0 = whatever lease is active |
| 16 | `epoch` | u32 | - | Must equal the gate's current epoch, else `EINVAL` (stale revokes are ignored and counted) |
| 20 | `reason` | u16 | - | A 4xx code |
| 22 | `reserved0` | u16 | - | 0 |

A revoke of a lease that is not active returns `EOK` and changes nothing.

**Latching revokes.** While MOVING, a revoke with reason `REVOKE_SENSOR_HEALTH` (400) or `REVOKE_UNVERIFIABLE` (404) sends the gate to STOPPING **and latches `SENSOR_LOST`** (REF-FSM R7 plus latch). Every other 4xx reason follows R7 with no latch, so the gate returns to READY after standstill.

### 3.4 Baseline modes B0 and B1: `DCMD_GUARD_PASSTHRU`

Status: **approved by the PO on 15 Nov 2026; needs agreement from A (A-10) and B (B-5) before the freeze.**

In B0 and B1 the planner commands the gate directly, so these runs show what happens **without** GUARD.

| Item | Value |
|---|---|
| Purpose | Baseline runs only: the planner's command goes straight to the gate, unchecked |
| Sender -> receiver | `guard-planner` -> `guard-gate` |
| Mechanism | `open("/dev/guard/gate", O_RDWR)`, then `devctl(DCMD_GUARD_PASSTHRU)` once per planning cycle (20 Hz) |
| Timeout | Client `TimerTimeout` of **5 ms**. On `ETIMEDOUT` the planner sends its next command in the next cycle |
| Allowed callers | uid `guard_plan` (1104) or root, **only while the gate runs in mode B0 or B1** (config C12 `mode`). In B2 the gate returns `EPERM` and counts `GATE_UNAUTHORIZED_CLIENT` |
| Supervisor | Runs, so that F3 and F7 can be injected, but takes no part: it does not register with the gate, and the planner sends it no proposals. In B0 and B1 the gate returns `EPERM` to `DCMD_GUARD_REGISTER`, `DCMD_GUARD_LEASE` and `DCMD_GUARD_REVOKE` |

`PassthruCmd` (type `0x0014`, 40 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0014`, size 40 |
| 8 | `cmd_seq` | u64 | - | ≥ 1, strictly increasing; otherwise `EINVAL` |
| 16 | `v` | f64 | m/s | -1.2 to 1.2; otherwise `EINVAL` |
| 24 | `w` | f64 | rad/s | -1.0 to 1.0; otherwise `EINVAL` |
| 32 | `stamp_ns` | i64 | ns | ≥ 0, the planner's send time |

Gate behaviour in B0 and B1. These rows replace REF-FSM rows R1-R14 for these modes. R15, R16 and R18 still apply, and `MotorCmd` is still sent every tick.

| Row | From | Event | To | Emits |
|---|---|---|---|---|
| P1 | INIT | start | READY (no BOOT latch) | - |
| P2 | READY or MOVING | valid `PassthruCmd` | MOVING; it becomes the held command | `GATE_PASSTHRU_ACCEPTED` |
| P3 | MOVING | B1 only: no valid `PassthruCmd` for `heartbeat_timeout_ms` (C12, default 500) | STOPPING | `GATE_HEARTBEAT_LOST` |
| P4 | MOVING | B0 only: no new command | MOVING; the held command repeats every tick | - |
| P5 | STOPPING or STOPPED | valid `PassthruCmd` | unchanged; `EOK`, command dropped | `GATE_NOT_READY` |

---

## 4. C3 - Operator recover

| Item | Value |
|---|---|
| Purpose | The operator asks the gate to leave LATCHED |
| Sender -> receiver | `guardctl recover` -> `guard-gate` |
| Mechanism | `devctl(DCMD_GUARD_RECOVER)` on `/dev/guard/gate` |
| Timeout | Client `TimerTimeout` of **100 ms**; `guardctl` exits 4 on timeout |
| Allowed callers | uid `guard_op` (1106) or root |

`RecoverRequest` (type `0x0020`, 16 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0020`, size 16 |
| 8 | `request_id` | u64 | - | ≥ 1 |

`RecoverReply` (type `0x0021`, 24 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0021`, size 24 |
| 8 | `request_id` | u64 | - | Copied |
| 16 | `reason` | u16 | - | `GATE_RECOVERED`, `GATE_RECOVERY_UNMET` or `GATE_NOT_READY` (gate not LATCHED) |
| 18 | `gate_mode` | u16 | - | Mode after handling |
| 20 | `unmet_mask` | u32 | - | Section 1.3; 0 when recovered |

---

## 5. C4 - Status file

| Item | Value |
|---|---|
| Purpose | Human- and script-readable gate snapshot |
| Sender -> receiver | `guard-gate` -> any reader (`cat`, `guardctl status`, bridge) |
| Mechanism | `open("/dev/guard/status", O_RDONLY)` then `read()`. One snapshot is taken at `open`; reads return it until EOF |
| Size | ≤ 2048 bytes, ASCII |
| Allowed callers | Any uid (read-only) |

Format: one `key=value` per line, `\n` line ends, keys in this order. Unknown keys must be ignored by readers.

```
version=1
mode=MOVING
epoch=3
lease_id=1042
lease_seq=1042
lease_remaining_ns=180000000
latch_cause=NONE
recovery_unmet_mask=0x00000000
v_cmd=0.800
w_cmd=0.000
ticks=123456
late_ticks=0
count.GATE_LEASE_ACCEPTED=1040
count.GATE_LEASE_EXPIRED=2
count.<CODE>=<n>
```

- `mode` and `latch_cause` use names (section 1.3).
- `lease_remaining_ns` is 0 when no lease is active.
- `count.*` lines appear only for 5xx codes with a non-zero count.

---

## 6. C5 - Motor command

| Item | Value |
|---|---|
| Purpose | The only path that moves the simulated robot |
| Sender -> receiver | `guard-gate` -> `guard-sim` |
| Mechanism | `devctl(DCMD_SIM_MOTOR)` on `/dev/sim/motor` |
| Rate | Every gate tick (10 ms), in every state. Zeros when not MOVING or STOPPING |
| Timeout | Client `TimerTimeout` of **5 ms**. On `ETIMEDOUT` while MOVING, the gate goes to STOPPING and latches `MOTOR_TIMEOUT` |
| Allowed callers | uid `guard_gate` (1102) only. Others get `EPERM` |
| Watchdog | If the sim gets no valid `MotorCmd` for **50 ms**, it brakes at `a_brake` and records `SIM_ACTUATOR_WATCHDOG` |

`MotorCmd` (type `0x0030`, 48 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0030`, size 48 |
| 8 | `cmd_seq` | u64 | - | ≥ 1, +1 per tick. The sim rejects non-increasing values with `EINVAL` |
| 16 | `v` | f64 | m/s | -1.2 to 1.2 |
| 24 | `w` | f64 | rad/s | -1.0 to 1.0 |
| 32 | `stamp_ns` | i64 | ns | ≥ 0 |
| 40 | `gate_mode` | u16 | - | 0-5 |
| 42 | `reserved0` | u16 | - | 0 |
| 44 | `reserved1` | u32 | - | 0 |

---

## 7. C6 - Observations

| Item | Value |
|---|---|
| Purpose | The world as the sim sees it. The supervisor and the planner get **separate feeds** |
| Sender -> receiver | `guard-sim` -> readers of `/dev/sim/obs/supervisor` (uid `guard_sup`) and `/dev/sim/obs/planner` (uid `guard_plan`) |
| Mechanism | Reader calls blocking `read(fd, buf, 464)`. The sim saves the `rcvid` and replies with `MsgReply` when the next observation is published (delayed reply). One observation per `read` |
| Rate | 20 Hz per feed |
| Timeout | Readers do not use `TimerTimeout` on `read`. Staleness is detected by the supervisor monitor pulse (sensor stale threshold 150 ms) |
| Errors | Buffer smaller than 464: `EMSGSIZE`. Wrong uid for the feed: `EPERM`. Feed paused (fault F4): the read stays blocked |

`Observation` (type `0x0001`, 464 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0001`, size 464 |
| 8 | `obs_id` | u64 | - | ≥ 1. Identifies one world sample. The sim publishes each sample on **both** feeds with the same `obs_id`, `stamp_ns` and world fields |
| 16 | `seq` | u64 | - | ≥ 1, +1 per observation delivered on this feed. Used only to spot gaps on one feed; never compared across feeds |
| 24 | `stamp_ns` | i64 | ns | ≥ 0, time the sim sampled the world |
| 32 | `x` | f64 | m | -100 to 100 (map frame) |
| 40 | `y` | f64 | m | -100 to 100 |
| 48 | `theta` | f64 | rad | -π to π |
| 56 | `v_meas` | f64 | m/s | -1.5 to 1.5 |
| 64 | `w_meas` | f64 | rad/s | -1.5 to 1.5 |
| 72 | `map_version` | u32 | - | ≥ 1 |
| 76 | `sensor_ok` | u8 | bool | 0 or 1 |
| 77 | `obstacle_count` | u8 | - | 0-16 |
| 78 | `feed` | u8 | - | 0 = supervisor, 1 = planner |
| 79 | `reserved0` | u8 | - | 0 |
| 80 | `obstacles[16]` | 16 × 24 B | - | Entries `0 .. obstacle_count-1` are valid; the rest are zero |

Each obstacle entry (24 bytes): `x` f64 (m, -100 to 100) at +0, `y` f64 (m, -100 to 100) at +8, `r` f64 (m, 0.05 to 2.0) at +16.

**Feeds and faults.** Both feeds carry the same samples. Fault F1 delays delivery on the planner feed: same `obs_id` and `stamp_ns`, later arrival. Fault F4 pauses delivery on the supervisor feed: samples published during the pause are skipped on that feed, not queued. The supervisor records `obs_id` -> `stamp_ns` for the last 64 samples it received (checkers 4-6).

---

## 8. C7 - World control (test image only)

| Item | Value |
|---|---|
| Purpose | Load a scenario, inject a fault, pause or resume the sim |
| Sender -> receiver | `eval` runner or `guardctl` -> `guard-sim` |
| Mechanism | `devctl` on `/dev/sim/world` (section 1.5); `read()` returns a status snapshot in C4 style |
| Timeout | Client `TimerTimeout` of **100 ms** |
| Allowed callers | uid `guard_op` (1106) or root. Built only into the test image |

`WorldLoad` (type `0x0040`, 120 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0040`, size 120 |
| 8 | `seed` | u64 | - | Any |
| 16 | `mode` | u8 | - | 0-2 (B0, B1, B2) |
| 17 | `ablation_mask` | u8 | - | Bits 0-3 (section 1.3) |
| 18 | `reserved0` | u16 | - | 0 |
| 20 | `reserved1` | u32 | - | 0 |
| 24 | `scenario_path` | char[96] | - | Absolute path on the target to a C11 scenario file, NUL-padded |

Errors: path missing or unreadable `ENOENT`; scenario invalid `EINVAL`; sim running and not paused `EBUSY`.

`WorldInject` (type `0x0041`, 24 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0041`, size 24 |
| 8 | `fault` | u8 | - | 1, 4, 5, 6 or 8 (F1, F4, F5, F6, F8). Process faults F2, F3, F7 are done by `guardctl`, not the sim |
| 9 | `reserved0` | u8[3] | - | 0 |
| 12 | `duration_ms` | u32 | ms | 0-60000 (0 = fault default from the scenario) |
| 16 | `param` | i64 | - | Fault-specific, 0 = scenario default |

`WorldPause` (type `0x0042`, 16 bytes):

| Offset | Field | Type | Units | Range |
|---|---|---|---|---|
| 0 | header | 8 B | - | type `0x0042`, size 16 |
| 8 | `pause` | u8 | bool | 1 = pause, 0 = resume |
| 9 | `reserved0` | u8[7] | - | 0 |

---

## 9. Fixtures

`fixtures/messages/<name>.valid.json` and `<name>.invalid.json` hold field values and the exact little-endian hex. Conformance tests in `core/tests/` (A-2) must encode the valid fields to the given hex, and must reject the invalid hex with the listed error.
