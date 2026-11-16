<!-- managed-by: guard-bootstrap v1 -->
# GUARD - Gated Unexpired Authorization for Robotic Drives

**Stale decisions can't move the robot.**

GUARD is a runtime supervisor for a *simulated* hospital delivery robot on QNX 8.0. An untrusted planner only proposes motion. GUARD checks each proposal against its own view of the world and grants a short, epoch-fenced lease. A separate gate process stops the robot with its own kernel timer when the lease runs out, even if the supervisor has died.

GUARD is a **simulation-only research prototype**. It is not a medical device and is not for real robots or patients.

> **Status:** draft README for the QNX Global Hackathon 2026 (Robotics & Medical). Sections marked `planned` or `TBD` are filled in from real runs before submission.

![GUARD dashboard demo (planned)](docs/media/demo.gif)

*GIF: `docs/media/demo.gif` -- planned (from the dashboard, #24).*

## The problem

An AI planner can be right about a world that no longer exists. A proposal can be sent just now but computed from an observation that is already old, from a map before a restricted zone switched on, or by a supervisor that was paused and replaced. A heartbeat only proves that the sender is alive. It does not prove that the decision being executed is still valid.

## How it works

```mermaid
flowchart LR
  P[guard-planner<br/>untrusted, prio 10] -- "Proposal (C1)" --> S[guard-supervisor<br/>checkers 1-11, prio 30]
  S -- "Decision (C1)" --> P
  S -- "REGISTER / LEASE / REVOKE (C2)" --> G[guard-gate<br/>lease timer, prio 35]
  G -- "MotorCmd every tick (C5)" --> M[guard-sim<br/>plant + watchdog, prio 40]
  M -- "Observations (C6)" --> S
  M -- "Observations (C6)" --> P
  G -. "status + telemetry rings" .-> B[guard-bridge<br/>read-only, prio 8]
  B -. "JSONL, TCP 7700 (C8)" .-> D[host dashboard<br/>read-only]
```

1. The planner proposes a speed and turn rate, and says which observation it used.
2. The supervisor runs all 11 checkers against its **own** observation history: provenance and freshness, map version, sensor health, speed envelope, restricted zones and stopping distance.
3. If every check passes, the supervisor sends the gate a lease that expires after `T_lease` (default 250 ms). The gate accepts it only if the epoch and sequence number are current. The stopping-distance check counts the whole lease time as reaction time, so a shorter lease means a shorter required distance and allows more speed near hazards.
4. The gate sends a motor command every tick. When the lease expires, is revoked or comes from an old epoch, the gate brings the robot to a stop and, for some causes, latches until an operator recovers it.
5. If the gate itself dies, the simulated drive brakes after its own watchdog timeout.

Starving the planner path, for example with a busy process above the supervisor, can only let leases lapse, which stops the robot. It never extends motion.

Details: [docs/architecture.md](docs/architecture.md), contracts in [contracts/](contracts/), decisions in [DECISIONS.md](DECISIONS.md).

## QNX Evidence Map

Every row links to the code and to target evidence once it exists.

| Feature | Where in code | Evidence |
|---|---|---|
| `MsgSend` / `MsgReceive` / `MsgReply` | `qnx/supervisor`, `qnx/planner` | planned (#45) |
| Pulses + `timer_create` | `qnx/gate` | planned (#45) |
| `TimerTimeout` on every blocking call | `qnx/common/ipc` | planned (#45) |
| Resource manager `/dev/guard/*` | `qnx/gate` | planned (#45) |
| Resource manager `/dev/sim/*` with delayed reply | `qnx/sim` | planned (#45) |
| Priority map + priority inheritance (ADR-0006) | `qnx/common/sched` | planned (#45) |
| Process isolation | target transcript | planned (#45) |
| Client credential authorization | `qnx/common/authz` | planned (#45) |
| `procmgr_ability` lock | `qnx/common/abilities` | planned (#45) |
| `slog2` logging | `qnx/common/log` | planned (#45) |
| Shared-memory telemetry rings | `qnx/common/ring` | planned (#45) |
| `mkqnximage` image | `image/` | planned (#45) |

## Quick start

All commands below are **planned**; they work once the linked issues land.

**Host-only path** (no QNX needed; deterministic replay):

```sh
git clone https://github.com/Tauwho/GUARD.git && cd GUARD
cmake --preset host && cmake --build --preset host          # planned (#16)
./build/host/guard-replay scenarios/F3.yaml --mode B2       # planned (#22)
python dashboard/relay.py --replay run.jsonl                # planned (#24)
```

**QNX path** (your own QNX SDP 8.0 licence and the quick-start target image in QEMU, x86_64):

```sh
source ~/qnx800/qnxsdp-env.sh                               # your own SDP install
image/build-image.sh                                        # planned (#43)
image/run-demo.sh                                           # planned (#43)
```

Setup steps: `docs/setup.md` (planned, #30).

## Results (functional only)

Numbers below come only from the **host (Linux) harness**. Runs on the QNX target are shown as behaviour (logs, reason codes and video), not as numbers.

| Scenario | Fault | B0 passthrough | B1 heartbeat | B2 GUARD |
|---|---|---|---|---|
| F0 | None (nominal delivery) | TBD (#23) | TBD (#23) | TBD (#23) |
| F1 | Stale planner | TBD (#23) | TBD (#23) | TBD (#23) |
| F2 | Delayed authorization | TBD (#23) | TBD (#23) | TBD (#23) |
| F3 | Supervisor killed | TBD (#23) | TBD (#23) | TBD (#23) |
| F4 | Sensor dropout | TBD (#23) | TBD (#23) | TBD (#23) |
| F5 | Insufficient stopping distance | TBD (#23) | TBD (#23) | TBD (#23) |
| F6 | Path through restricted zone | TBD (#23) | TBD (#23) | TBD (#23) |
| F7 | Zombie supervisor | TBD (#23) | TBD (#23) | TBD (#23) |
| F8 | Planner flood | TBD (#23) | TBD (#23) | TBD (#23) |

Each cell will show violation episodes and the final state, from the eval report (#23).

## Limits — read first
<!-- claims-ok: negated -->
- No certified functional safety. GUARD claims no compliance with ISO 13849, IEC 61508, ISO 3691-4 or IEC 62304, and no SIL or PL rating.
- No real actuators, braking or vehicles. This is a simulation-only prototype.
- No hard real-time guarantee on hardware. QEMU timing is not hardware timing.
- No formal verification. Assurance comes from unit, scenario and property tests only.
- No defence against a compromised kernel, supervisor or gate. Only the planner is treated as untrusted.
- No QNX performance claims and no QNX-vs-Linux comparisons without BlackBerry's written approval.

## AI-use disclosure

Three people build this project with AI coding assistants. Assistants drafted documents, contracts and code under human direction. A human code owner reviews and merges every change, and PRs written with an assistant tick the "AI-assisted" box. The rule-based planner is the baseline; an LLM route advisor is an optional extra (P1).

## Team

- [@dngvmnh](https://github.com/dngvmnh) -- product owner
- [@phatdat220707](https://github.com/phatdat220707) -- portable core and host tooling
- [@baofrovn](https://github.com/baofrovn) -- QNX target and image

## How we work

- [AGENTS.md](AGENTS.md): rules for humans and AI agents
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [DECISIONS.md](DECISIONS.md)

CI runs on the host only. QNX runs happen on our own machines, and their results are committed as text evidence bundles in `evidence/`.

## Licence

MIT. The QNX SDP is not included or redistributed. Each developer installs it under their own licence.
