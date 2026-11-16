<!-- managed-by: guard-bootstrap v1 -->
# GUARD architecture reference

Reference architecture for GUARD. Contracts in `contracts/` take precedence once v1.0 is tagged.

## REF-LAYOUT

```
contracts/  messages.md, reason_codes.yaml, *.schema.json, guardctl.md, fixtures/, CHANGELOG.md
core/       include/guard/, src/, tests/      portable C++17; no QNX headers, clocks or I/O
qnx/        common/ sim/ gate/ supervisor/ planner/ bridge/ guardctl/ hog/   adapters only
host/       guard-replay (virtual-time harness)
scenarios/  maps/*.yaml, F0..F8 *.yaml
eval/       run_matrix.py, metrics.py, report.py, tests/
dashboard/  index.html, app.js, relay.py
image/      mkqnximage snippets, build-image.sh, run-demo.sh, secpol/ (P1), slm/ (P1)
tools/      capture-evidence, env-check, check-core-portable, check-no-qnx-files, claims-lint.txt
evidence/   <yyyymmdd>-<sha7>-<topic>/  (text only)
codelab/    lease-gate/ (P1)
docs/       architecture.md, adr/, setup.md, parameters.md, limits.md
cmake/      qnx-x86_64.cmake
```

## REF-DATAMODEL

All target times are `CLOCK_MONOTONIC` in ns, on a single clock (the sim runs on the target).

**Wire format:**
- Fixed-size little-endian POD structs.
- Header `{magic 'GD' u16, type u16, version u16, size u16}`.
- No pointers.

| Type | Key fields | Flow |
|---|---|---|
| Observation | `obs_id u64`, `seq u64`, `stamp_ns i64`, pose `{x,y,theta}`, `v_meas`, `w_meas`, `map_version u32`, `obstacles[<=16]{x,y,r}`, `sensor_ok` | sim → supervisor and sim → planner (separate feeds) |
| Proposal | `proposal_id u64`, `planner_id u32`, `seq u64`, `based_on{obs_id, obs_seq, obs_stamp_ns, map_version}`, `created_ns`, `v_cmd`, `w_cmd`, `lease_req_ms` | planner → supervisor |
| Decision | `proposal_id`, `verdict{AUTHORIZED,REJECTED,UNVERIFIABLE}`, `primary_reason u16`, `reasons_mask u64`, `lease_id`, `epoch`, `expires_ns`, `decided_ns` | supervisor → planner (reply) |
| Lease | `lease_id u64`, `epoch u32`, `lease_seq u64`, `proposal_id`, `v_cmd`, `w_cmd`, `v_cap`, `issued_ns`, `expires_ns` | supervisor → gate |
| MotorCmd | `cmd_seq u64`, `v`, `w`, `gate_mode`, `stamp_ns` | gate → sim, every 10 ms |
| GateStatus | `mode`, `epoch`, `lease_id`, `lease_remaining_ns`, `latch_cause`, `recovery_unmet_mask`, per-reason counters | gate → status file, gate → bridge |
| RouteProposal (P1, reserved) | `route_id`, `based_on{obs_id, map_version, stamp_ns}`, `waypoints[<=16]`, `created_ns` | remote planner → supervisor |
| TelemetryEvent | `t_ns`, `src`, `kind`, payload (JSONL) | shm ring → bridge → host |

## REF-CONTRACTS

| ID | Contract | Endpoint | Mechanism | Bound |
|---|---|---|---|---|
| C1 | Proposal → Decision | planner → `guard/supervisor` (`name_attach`) | `MsgSend`/`MsgReceive`/`MsgReply`; client `TimerTimeout` 50 ms | 1 in flight per planner; ≤ 50/s |
| C2 | Register / lease / revoke | supervisor → `/dev/guard/gate` | `devctl`: `DCMD_GUARD_REGISTER` (`__DIOF`), `DCMD_GUARD_LEASE` (`__DIOTF`), `DCMD_GUARD_REVOKE` (`__DIOT`) | Client `TimerTimeout` 5 ms |
| C3 | Operator | `guardctl` → `/dev/guard/gate` | `devctl` `DCMD_GUARD_RECOVER` (`__DIOTF`) | op uid or root |
| C4 | Status | any reader → `/dev/guard/status` | `read()`: `key=value` text, one snapshot per `open` | ≤ 2 KiB |
| C5 | Motor | gate → `/dev/sim/motor` | `devctl` `DCMD_SIM_MOTOR` (`__DIOT`); `TimerTimeout` 5 ms | 100 Hz; gate uid only |
| C6 | Observations | sim → `/dev/sim/obs/{supervisor,planner}` readers | Blocking `read()`; sim replies later with `MsgReply` on the saved `rcvid` | 20 Hz |
| C7 | World control | eval / `guardctl` → `/dev/sim/world` | `devctl`: load scenario, inject fault, pause; `read` for status | Test image only |
| C8 | Telemetry | bridge → host TCP 7700 | JSONL per `telemetry.schema.json`; the harness emits the same schema | Best effort; drops counted |
| C9 | Remote planner (P1) | planner ↔ host TCP 7701 | JSONL request `{obs}`, response `{proposal or route}` | Host may be slow |
| C10 | Reason codes | `reason_codes.yaml` | Generates the header and the docs | Never renumbered |
| C11 | Scenario | `scenario.schema.json` | YAML | — |
| C12 | Config | `config.schema.json` | YAML (parameters, mode, ablations) | — |
| C13 | Evidence manifest | `evidence-manifest.schema.json` | JSON | — |
| C14 | `guardctl` CLI | `guardctl.md` | Commands, exit codes, outputs | — |

## REF-REASONCODES

| Range | Codes |
|---|---|
| 1xx | `OK_AUTHORIZED` |
| 2xx | `REJ_SCHEMA`, `REJ_RATE_LIMIT`, `REJ_SEQ_REPLAY`, `REJ_UNKNOWN_OBSERVATION`, `REJ_PROVENANCE_MISMATCH`, `REJ_STALE_OBSERVATION`, `REJ_SUPERSEDED_OBSERVATION`, `REJ_MAP_VERSION`, `REJ_SPEED_ENVELOPE`, `REJ_ACCEL_ENVELOPE`, `REJ_RESTRICTED_ZONE`, `REJ_STOPPING_DISTANCE`, `REJ_GATE_NOT_READY` |
| 3xx (= reject) | `UNV_SENSOR_STALE`, `UNV_CHECK_BUDGET`, `UNV_INTERNAL` |
| 4xx | `REVOKE_<checker>` |
| 5xx | `GATE_LEASE_ACCEPTED`, `GATE_EPOCH_STALE`, `GATE_LEASE_REPLAY`, `GATE_LEASE_EXPIRED_ON_ARRIVAL`, `GATE_NOT_READY`, `GATE_UNAUTHORIZED_CLIENT`, `GATE_LEASE_EXPIRED`, `GATE_REVOKED`, `GATE_SUPERVISOR_LOST`, `GATE_EPOCH_CHANGED`, `GATE_MOTOR_TIMEOUT`, `GATE_STANDSTILL`, `GATE_RECOVERED`, `GATE_RECOVERY_UNMET` |
| 6xx | `SIM_ACTUATOR_WATCHDOG`, `SIM_ZONE_BREACH`, `SIM_CONTACT` |

Every code is a stable string plus a number.

## REF-PARAMS

Defaults are an assumption at hospital-AMR scale. Sources go in `docs/parameters.md`.

| Parameter | Value |
|---|---|
| `v_max` | 1.2 m/s |
| `w_max` | 1.0 rad/s |
| `a_max` | 0.5 m/s² |
| `a_brake` | 1.0 m/s² |
| `T_lease` | 250 ms (configurable 50–1000) |
| `t_tick` | 10 ms |
| `t_act` | 20 ms |
| `margin` | 0.20 m |
| `r_robot` | 0.35 m |
| Observation rate | 20 Hz |
| `max_obs_age` | 200 ms |
| K | 2 |
| Sensor stale threshold | 150 ms |
| Standstill | v_meas < 0.02 m/s for 5 ticks |
| Actuator watchdog | 50 ms |
| Rate limit | 50 proposals/s |
| Observation history | 64 |
| Zones | ≤ 32 |
| Obstacles | ≤ 16 |
| Route max age (P1) | 5 s |

## REF-CHECKERS

**Checker contract:**
- The interface is `check(ctx) -> {verdict, reason, detail}`.
- `ctx` holds the proposal, the supervisor's own world view (latest observation + history of 64), the config and `now_ns`.
- Each checker is a pure function.
- The registry is compile-time and in a fixed order. **All checkers run.**
- The primary reason is the first non-pass. UNVERIFIABLE dominates and is treated as REJECTED.

| # | Checker | Fails when | Code |
|---|---|---|---|
| 1 | schema | Bad magic, size or version; NaN/Inf; over-bound inputs | `REJ_SCHEMA` |
| 2 | rate | > 50/s per planner | `REJ_RATE_LIMIT` |
| 3 | sequence | `seq` not greater than the last | `REJ_SEQ_REPLAY` |
| 4 | provenance | `obs_id` not in history, or the claimed stamp differs from the recorded one | `REJ_UNKNOWN_OBSERVATION` / `REJ_PROVENANCE_MISMATCH` |
| 5 | freshness | `now − recorded_stamp > max_obs_age` | `REJ_STALE_OBSERVATION` |
| 6 | supersession | More than K newer observations exist | `REJ_SUPERSEDED_OBSERVATION` |
| 7 | map version | Differs from the current version | `REJ_MAP_VERSION` |
| 8 | sensor health | Own latest observation > 150 ms old, or `sensor_ok` false | `UNV_SENSOR_STALE` |
| 9 | envelope | abs(v) > `v_max`, abs(w) > `w_max`, or implied acceleration > `a_max` | `REJ_SPEED_ENVELOPE` / `REJ_ACCEL_ENVELOPE` |
| 10 | restricted zone | Swept footprint over the lease horizon enters an active zone | `REJ_RESTRICTED_ZONE` |
| 11 | stopping distance | Free distance along the swept arc < `d_req` | `REJ_STOPPING_DISTANCE` |

**Stopping distance:**
- `t_r = T_lease + t_tick + t_act`
- `v_pk = min(v_cmd, v_0 + a_max·t_r)`
- `d_req = v_pk·t_r + v_pk²/(2·a_brake) + margin + r_robot`
- Free distance is measured along the commanded arc, sampled every 5 cm, to the first inflated obstacle or active zone edge.
- Example: at 1.2 m/s, `d_req` = 1.61 m.
- A shorter lease means a smaller `d_req`, so more speed is allowed near hazards.

**Re-check:** on every new observation, re-run checks 8–11 against the active lease. A failure sends `REVOKE_<cause>`. Three consecutive UNVERIFIABLE results while MOVING also revoke.

**Honest limit:** provenance is declared by the planner and cross-checked against the supervisor's own history. A lying planner is still bounded by checks 8–11, which judge the command against the current world.

## REF-FSM

**States:** INIT, LATCHED, READY, MOVING, STOPPING, STOPPED.

| Row | From | Event / guard | To | Emits |
|---|---|---|---|---|
| R1 | INIT | start | LATCHED | latch BOOT |
| R2 | LATCHED | RECOVER, all conditions met | READY | `GATE_RECOVERED` |
| R3 | LATCHED | RECOVER, conditions unmet | LATCHED | `GATE_RECOVERY_UNMET` + mask |
| R4 | READY | valid LEASE (epoch == current, `lease_seq` > last, now < expires) | MOVING | `GATE_LEASE_ACCEPTED` |
| R5 | MOVING | valid LEASE | MOVING | lease replaced |
| R6 | MOVING | tick or expiry pulse, now ≥ expires | STOPPING | `GATE_LEASE_EXPIRED` |
| R7 | MOVING | REVOKE | STOPPING | `GATE_REVOKED` |
| R8 | MOVING | REGISTER (new epoch) | STOPPING + latch | `GATE_EPOCH_CHANGED` |
| R9 | MOVING | registered supervisor's connection closed | STOPPING + latch | `GATE_SUPERVISOR_LOST` |
| R10 | MOVING | motor `devctl` timeout | STOPPING + latch | `GATE_MOTOR_TIMEOUT` |
| R11 | any | LEASE with epoch < current | unchanged | `GATE_EPOCH_STALE` |
| R12 | any | LEASE with `lease_seq` ≤ last | unchanged | `GATE_LEASE_REPLAY` |
| R13 | any | LEASE with now ≥ expires on arrival | unchanged | `GATE_LEASE_EXPIRED_ON_ARRIVAL` |
| R14 | LATCHED, STOPPING, STOPPED | LEASE | unchanged | `GATE_NOT_READY` |
| R15 | STOPPING | tick | STOPPING | `v = max(0, v_meas − a_brake·t_tick)`, `w → 0` |
| R16 | STOPPING | standstill confirmed from measured v | STOPPED | `GATE_STANDSTILL` |
| R17 | STOPPED | latch set | LATCHED | — |
| R18 | STOPPED | no latch | READY | — |

**Recovery conditions.** All must hold; any unmet ones are reported as a bitmask.
- Standstill confirmed.
- Pose outside every active zone, plus the margin.
- Supervisor registered in the current epoch.
- Supervisor observations fresh.
- Latch cause cleared.
- Operator RECOVER received.

**Latch causes:** BOOT, SUPERVISOR_LOST, EPOCH_CHANGED, ZONE_BREACH, SENSOR_LOST (while moving), MOTOR_TIMEOUT, INTERNAL.

**MotorCmd:** the gate emits one every tick in every state (zeros when not MOVING).

**Stop layers:**
1. The supervisor rejects or revokes.
2. The gate's own timer expires the lease.
3. The sim actuator watchdog brakes after 50 ms (a stand-in for a drive-level stop; GUARD does not implement STO).
4. P1: SLM restarts the process into LATCHED.

**Wording:** STOPPING and STOPPED are analogies to SS1 and SOS only. No compliance is claimed.

## REF-PRIORITIES

All threads use `SCHED_FIFO`, priority ≤ 63.

| Thread / event | Prio | Mechanism |
|---|---|---|
| sim tick | 40 | `timer_create` + `SIGEV_PULSE_INIT`, 10 ms |
| gate tick + lease expiry | 35 | Periodic + one-shot timer pulses |
| gate resmgr | Inherits the sender | Single-threaded dispatch |
| supervisor monitor + obs reader | 30 | 20 ms pulse; blocking `read` |
| supervisor proposal server | Inherits the planner (10) | `name_attach`; default inheritance; **not** `_NTO_CHF_FIXED_PRIORITY` |
| hog (F2) | 32 | Busy loop |
| planner | 10 | — |
| bridge | 8 | Ring reader |

**Privilege drop:** each process starts as root, then:
1. attaches its path (`PROCMGR_AID_PATHSPACE`) or public channel (`PROCMGR_AID_PUBLIC_CHANNEL`);
2. sets its priority;
3. calls `setuid` to its own uid;
4. locks its abilities with `procmgr_ability`.

**uids:** `guard_sim` 1101, `guard_gate` 1102, `guard_sup` 1103, `guard_plan` 1104, `guard_bridge` 1105, `guard_op` 1106.

**Rule:** starving the planner path can only let leases lapse, which stops the robot. It never extends motion.

## REF-SECURITY

| Threat | P0 control | P1 / P2 control |
|---|---|---|
| Planner drives the actuator | Sim checks the client uid is `guard_gate` (`ConnectClientInfo`) | secpol: no `planner_t` → `sim_t` connect |
| Planner forges a lease | Gate checks the client uid is `guard_sup` | secpol deny |
| Replay or zombie supervisor | Epoch + `lease_seq` fencing | — |
| Forged or stale provenance | Supervisor-owned history; command checked against the current world | P2: HMAC |
| Flooding | Rate checker; planner prio 10; inheritance | — |
| Telemetry injection | Bridge is send-only; dashboard is read-only | — |
| Excess privilege | Per-process uids; ability lock | `mkqnximage --secpol=secure` with our policy |
| Licence or secret leak | `.gitignore`, CI `no-qnx-files`, per-person keys, no CI secrets | — |

**Allowed connect edges (secpol, P1):**
- `planner_t` → `supervisor_t`, `sim_t` (obs/planner and world status, read)
- `supervisor_t` → `gate_t`, `sim_t` (obs/supervisor)
- `gate_t` → `sim_t` (motor)
- `bridge_t`: no connects needed (shm read)
- `guardctl_t` → `gate_t`, `sim_t` (world)

## REF-SCENARIOS

| ID | Fault | Injection | Expected under B2 (a hypothesis) |
|---|---|---|---|
| F0 | None: specimen delivery from Ward A to the Lab, past the MRI suite | — | 0 violations, 0 latches; false-reject rate reported |
| F1 | Stale planner | t = 10 s: lag 600 ms while staff step in | `REJ_STALE_OBSERVATION`, stop, no contact |
| F2 | Delayed authorization | hog at prio 32 for 400 ms | `GATE_LEASE_EXPIRED`, STOPPED, then resume |
| F3 | Supervisor killed | `kill -9` at t = 12 s | Stop within T_lease + tick; LATCHED (`SUPERVISOR_LOST`) |
| F4 | Sensor dropout | supervisor observation feed paused for 1 s | `UNV_SENSOR_STALE`, revoke, stop, latch |
| F5 | Insufficient stopping distance | aggressive planner at 1.2 m/s toward a staff crossing | `REJ_STOPPING_DISTANCE`, then a lower speed is authorized |
| F6 | Path through restricted zone | MRI zone activates at t = 8 s (`map_version++`); planner shortcut | `REJ_MAP_VERSION` / `REJ_RESTRICTED_ZONE` |
| F7 | Zombie supervisor | `SIGSTOP` A, start B (new epoch), `SIGCONT` A | A's lease rejected with `GATE_EPOCH_STALE` |
| F8 | Planner flood | 1000 proposals/s for 2 s | `REJ_RATE_LIMIT`; monitor and gate never late (internal) |

**Modes:**
- B0: passthrough, holds the last command.
- B1: heartbeat, stops after 500 ms of silence.
- B2: GUARD.
- Ablations: `-freshness` (checks 4–7), `-stopping` (check 11), `-expiry`, `-epoch`.

**Matrix:**
- Host: 9 scenarios × 50 seeds × 7 modes.
- Target: 9 scenarios × 5 seeds under B2, plus F1, F3 and F7 under B1.

**Metrics (functional only):**
- violations (zone breach and contact episodes)
- minimum margin
- fault onset → STOPPING time and STOPPING → STOPPED time (sim time)
- F0 false-reject rate
- time to goal
- recovery correctness

## REF-EVIDENCE

**When:** required on every PR touching `core/`, `qnx/`, `image/`, `cmake/` or `contracts/`.

**Bundle path:** `evidence/<yyyymmdd>-<sha7>-<topic>/`. Text only, ≤ 1 MiB.

| File | Content |
|---|---|
| `manifest.json` | sha, date, SDP version, `qcc -V`, image ID, host OS, runner role |
| `transcript.txt` | `script(1)` capture of the ssh session |
| `uname.txt` | `uname -a` on the target |
| `pidin.txt` | Process listing |
| `binaries.txt` | `file`, `readelf -h` and `sha256` for each binary |
| `slog2.txt` | `slog2info` output |
| `results.json` | Test or scenario results |

**CI checks:**
- The manifest validates against its schema.
- The sha is an ancestor of HEAD.
- The size limit holds.

**Redaction:** remove any licence identifiers before committing.

## REF-CI

Every CI job runs on the host on `ubuntu-24.04`. The QNX SDP is never used.

| Job | What it checks |
|---|---|
| build + test | gcc and clang, ASan/UBSan, ctest |
| format | clang-format diff |
| core-portable | Rejects QNX headers, `Msg*`, `resmgr_*`, `slog2*`, `clock_gettime` and file I/O in `core/` |
| no-qnx-files | Rejects `qnxsdp-env*`, `*.ifs`, `*.qcow2`, `*.vmdk`, `*.img`, licence and key patterns, and copied SDP headers |
| claims-lint | Banned phrases in README/docs, except text from a `claims-ok: negated` marker to the next heading |
| schemas | Contracts, fixtures, scenarios and evidence manifests |
| evidence | Ancestor sha and size limit |
| eval | pytest + ruff |

## REF-TIERS

**P0 definition of done:**
- F0–F8 meet `expected` on the host (50 seeds) and on the target (5 seeds).
- An evidence bundle for every scenario.
- `image/run-demo.sh` boots straight into GUARD.
- The README quick start works from a clean clone.
- The video uses P0 features only.

**P1, ranked** (chosen only at CK-M3, from slack; the LLM advisor is rank 2 under assumption A4, see Q4 in the DECISIONS.md clarification log):

| Rank | Item | Hours |
|---|---|---|
| 1 | secpol (B-12a) | 4 |
| 2 | LLM advisor (A-12 + B-12d) | 6 |
| 3 | SLM (B-12b) | 2 |
| 4 | Codelab (PO-9 + B-12e) | 7 |
| 5 | tracelogger (B-12c) | 2 |
| 6 | Hypothesis (A-13) | 3 |
| 7 | MkDocs site (PO) | 2 |

## REF-FREEZES

All times SGT.

| Freeze | Date |
|---|---|
| Contracts | Fri 20 Nov 23:59 |
| Scenarios | Sat 28 Nov 23:59 |
| P0 complete | Wed 9 Dec 23:59 |
| Code | Thu 10 Dec 23:59, tag `v1.0-rc1` |
| Video | Sat 12 Dec 23:59 |
| Submission filed | Sun 13 Dec 23:59 |
| Hard stop | Tue 15 Dec 18:00 |
| Deadline | Wed 16 Dec 03:00 (= Tue 15 Dec 14:00 EST) |

## REF-CLAIMS

<!-- claims-ok: negated -->

**Marker rule:** the `<!-- claims-ok: negated -->` marker exempts everything from its line to the next Markdown heading. Use it only for negated statements, like the Limits list, and for this block.

**Limits** (verbatim in README):
- No certified functional safety. GUARD claims no compliance with ISO 13849, IEC 61508, ISO 3691-4 or IEC 62304, and no SIL or PL rating.
- No real actuators, braking or vehicles. This is a simulation-only prototype.
- No hard real-time guarantee on hardware. QEMU timing is not hardware timing.
- No formal verification. Assurance comes from unit, scenario and property tests only.
- No defence against a compromised kernel, supervisor or gate. Only the planner is treated as untrusted.
- No QNX performance claims and no QNX-vs-Linux comparisons without BlackBerry's written approval.

**Banned phrases** (unless negated and marked `<!-- claims-ok: negated -->`):
- "is safe"
- "guarantee(s)"
- "certified"
- "compliant"
- "prevents collisions"
- "hard real-time"
- "faster than Linux"

**Preferred wording:** "rejects stale or unverifiable motion in simulation", "shown on the QNX target", "measured in the host (Linux) harness". Nothing measured on QNX is published (A5, D11).
