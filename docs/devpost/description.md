# Devpost description -- draft

Status: **draft** (front-loaded, issue #8). Paste into Devpost on Sun 6 Dec after the autopilot refreshes it. Every claim here also appears in the README (draft #64).

**Title:** GUARD -- Gated Unexpired Authorization for Robotic Drives

**Tagline** (≤ 60 characters): Stale decisions can't move the robot.

**Category:** Robotics & Medical

**Built with:** qnx, c++, cmake, python, qemu, mermaid, github-actions (+ ollama, only if the P1 LLM advisor ships)

**Try it out:**
- Repository: https://github.com/Tauwho/GUARD
- Codelab on GitHub Pages: planned (only if the codelab is GO at CK-M3, #13)
- Demo video: TBD (#7)

---

## Inspiration

AI planners are getting good at choosing a path. The hard part is knowing whether the world they planned for still exists. In a hospital corridor, a nurse can step out, or an MRI zone can switch on, between the moment a plan is made and the moment the wheels turn. A heartbeat tells you the planner is alive. It does not tell you its decision is still valid.

## What it does

GUARD is a runtime supervisor for a simulated hospital delivery robot on QNX 8.0. An untrusted planner only proposes motion. GUARD checks each proposal against its own view of the world and grants a short, epoch-fenced lease. A separate gate process stops the robot with its own kernel timer when the lease runs out, even if the supervisor has died.

GUARD is a **simulation-only research prototype**. It is not a medical device and is not for real robots or patients.

## How it works

1. The planner proposes a speed and turn rate, and names the observation it used.
2. The supervisor runs 11 checkers against its own observation history: provenance and freshness, map version, sensor health, speed envelope, restricted zones and stopping distance.
3. If every check passes, the supervisor sends the gate a lease that expires after `T_lease` (default 250 ms). The gate accepts it only if its epoch and sequence number are current.
4. The gate sends a motor command every tick. When the lease expires, is revoked or comes from an old epoch, the gate stops the robot and, for some causes, latches until an operator recovers it.
5. If the gate dies, the simulated drive brakes on its own watchdog.

The README has the diagram: https://github.com/Tauwho/GUARD#how-it-works

## QNX technologies

See the QNX Evidence Map in the README (https://github.com/Tauwho/GUARD#qnx-evidence-map). In short:
- native message passing (`MsgSend` / `MsgReceive` / `MsgReply`) between planner and supervisor;
- timer pulses for the gate tick and lease expiry;
- `TimerTimeout` on every blocking call;
- resource managers for `/dev/guard/*` and `/dev/sim/*`, including delayed replies;
- a fixed priority map with priority inheritance;
- client-credential checks and `procmgr_ability` locks;
- `slog2` logging and shared-memory telemetry rings;
- a boot image built with `mkqnximage`.

Target evidence for each row: TBD (from #45 target evidence).

## How we built it

Three people, three streams. The portable C++17 core holds all the logic, with time passed in, so the same code runs in a deterministic host harness and on the QNX target. The QNX layer only wires IPC, timers, resource managers and privileges to the core. Interfaces were frozen as contracts first (messages, reason codes, schemas). Scenarios F0-F8 were written as acceptance tests before the code. CI runs on the host only; QNX runs happen on our own machines and are committed as text evidence bundles.

## Results (functional only)

Results come only from the **host (Linux) harness**. Runs on the QNX target are shown as behaviour (logs, reason codes and the video), not as numbers.

TBD (from #23 eval report): violation episodes and final state per scenario F0-F8, for no supervisor (B0), a heartbeat (B1) and GUARD (B2).

## Challenges

TBD (from the build, by 6 Dec).

## Accomplishments that we're proud of

TBD (from the build, by 6 Dec).

## What we learned

TBD (from the build, by 6 Dec).

## What's next

- Signed proposals, so a planner cannot lie about which observation it used.
- Polygon zones and an occupancy grid instead of rectangles and circles.
- TBD (from the CK-M3 P1 decision, #13).

## Limits
<!-- claims-ok: negated -->
- No certified functional safety. GUARD claims no compliance with ISO 13849, IEC 61508, ISO 3691-4 or IEC 62304, and no SIL or PL rating.
- No real actuators, braking or vehicles. This is a simulation-only prototype.
- No hard real-time guarantee on hardware. QEMU timing is not hardware timing.
- No formal verification. Assurance comes from unit, scenario and property tests only.
- No defence against a compromised kernel, supervisor or gate. Only the planner is treated as untrusted.
- No QNX performance claims and no QNX-vs-Linux comparisons without BlackBerry's written approval.

## AI-use disclosure

We built GUARD with AI coding assistants. Assistants drafted documents, contracts and code under human direction. A human team member merged every change once CI was green. The rule-based planner is the baseline; an LLM route advisor is an optional extra.
