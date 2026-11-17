# GUARD demo video: storyboard

Source of truth for what is run on screen: [demo/DEMO.md](../../demo/DEMO.md). Every
scene below maps to one DEMO step; `python tools/check_demo_commands.py` keeps those
commands on `main`. Shots: [shot-list.md](shot-list.md). Captions: [captions.md](captions.md).

Supersedes the v1 draft in PR #65 (scenes kept, re-cut to the DEMO steps).

## Rules for every frame (D11, ADR 0011)

- Nothing measured on QNX is shown as a number: no latency, jitter, CPU or memory
  figures, no QNX-vs-Linux comparison. On-target scenes show behaviour only: modes,
  reason codes, logs.
- Numbers appear only in the results scene, from the host (Linux) harness, labelled so.
- GUARD is a simulation-only research prototype. It is never shown as a medical device
  or as something for real robots or patients.
- No QNX logo. "QNX" appears only as plain text and in normal terminal output.
- Every claim is backed by a file on `main` (claims table below).

Total **2:58**, hard limit under 3:00. Footage is recorded from tag `v1.0-rc1` (P3).

## Scenes

| # | Time | DEMO step | On screen | Caption text |
|---|---|---|---|---|
| 1 | 0:00-0:10 | 1 | Hospital map in the dashboard, robot at Ward A | "An AI planner can be right about a world that no longer exists." |
| 2 | 0:10-0:35 | 1 | Host terminal: F3 replay under B2, then under B1; final states highlighted | "Supervisor killed: GUARD latches. A heartbeat baseline keeps moving." |
| 3 | 0:35-1:10 | 2 | Dashboard replay of `out/F3-B2.jsonl`: lease bar drains, STOPPING, LATCHED, reason journal | "No lease renewal, so the gate stops the robot on its own timer." |
| 4 | 1:10-1:45 | 3 | Split: F6 under B2 (robot waits, `REJ_MAP_VERSION`) and under B0 (robot crosses the active MRI zone) | "Stale map: refused, with a reason code." |
| 5 | 1:45-2:30 | 4 | Target terminal + live dashboard (speed and lease-time readouts cropped): `guardctl status` filtered to mode and latch cause, `inject F3`, `status`, `recover` | "Same fault on QNX 8.0 in QEMU: behaviour only." |
| 6 | 2:30-2:55 | 5 | README Results block (host Linux harness), then the Limits list | "Results: host (Linux) harness. Simulation-only research prototype." |
| end | 2:55-2:58 | - | End card: repository URL | "Stale decisions can't move the robot." |

## Narration (about 300 words)

1. "An AI planner can be right about a world that no longer exists. In a hospital, that
   world changes fast."
2. "GUARD sits between the planner and the simulated robot's motor commands. The planner only proposes motion. Here
   we kill the supervisor mid-run. With GUARD, the robot comes to a stop and latches.
   With a heartbeat-only baseline, the planner is still alive, so the robot keeps moving."
3. "The supervisor grants short leases. Nothing renews the lease once it is gone, so the
   gate, a separate process with its own timer, stops the robot and stays latched until an
   operator recovers it."
4. "Now the MRI zone switches on while the planner still cites its old map. GUARD refuses
   the proposal, says why, and the robot waits outside. Without a supervisor, the robot
   drives straight through the zone."
5. "The same fault on QNX 8.0 under QEMU. guardctl injects it, the gate latches, and
   recovery is refused until its conditions hold. We show behaviour here, not timings."
6. "Every number in our results comes from the host Linux harness and is labelled that
   way. GUARD is a simulation-only research prototype, not for real robots or patients."
7. "Stale decisions can't move the robot."

## Claims

| # | Claim | Backed by (on `main`) |
|---|---|---|
| C1 | The planner only proposes; the supervisor authorizes with a lease | README "How it works" 1-3; `docs/architecture.md` |
| C2 | F3 under B2 ends LATCHED with `GATE_SUPERVISOR_LOST`; under B1 it ends MOVING | `scenarios/F3.yaml` `expected`; DEMO step 1 exit 0 |
| C3 | The gate stops the robot on its own timer when no lease is renewed | README intro; `docs/architecture.md` |
| C4 | F6 under B2 is refused with `REJ_MAP_VERSION` and has 0 violations; under B0 at least 1 | `scenarios/F6.yaml` `expected`; DEMO step 3 |
| C5 | `guardctl recover` is refused with `GATE_RECOVERY_UNMET` until recovery conditions hold | `contracts/guardctl.md` |
| C6 | Results are from the host (Linux) harness | README Results; `docs/results/host-linux.json` |
| C7 | Simulation-only research prototype | README Limits; `docs/limits.md` |

C5 is backed by the contract; the scene 5 footage itself is behaviour only and is
backed by the F3 target evidence bundle that B5 adds.
