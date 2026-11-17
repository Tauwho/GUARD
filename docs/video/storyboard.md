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

Total **2:46** (final cut, `docs/video/duration.txt`), hard limit under 3:00. Footage is
recorded from tag `v1.0-rc1` (P3). Scene 1 uses the F0 replay (a normal run) to set the scene
before any fault; the target scene uses `image/run-demo.sh --test --dashboard`.

## Scenes

| # | Time | DEMO step | On screen | Caption text |
|---|---|---|---|---|
| 1 | 0:00-0:17 | 1 | Dashboard replay of a normal F0 run (host, 1x), robot leaving Ward A | "An AI planner can be right about a world that no longer exists." / "GUARD: only fresh, checked decisions move the simulated robot." |
| 2 | 0:17-0:39 | 1 | Host terminal: F3 replay under B2, then under B1; `final_state` and exit codes highlighted | "Supervisor killed: GUARD latches. A heartbeat baseline keeps moving." |
| 3 | 0:39-1:09 | 2 | Dashboard replay of `out/F3-B2.jsonl` from 3 s (1x): supervisor lost at 12 s, STOPPING, LATCHED, reason journal | "No lease renewal, so the gate stops the robot on its own timer." |
| 4 | 1:09-1:41 | 3 | Split: F6 under B2 (robot waits, `REJ_MAP_VERSION`) and under B0 (robot crosses the active MRI zone), both from 5 s at 1x | "Stale map: refused, with a reason code." |
| 5 | 1:41-2:21 | 4 | Target terminal (start log, then the four `ssh ... guardctl` lines) + live dashboard with every numeric readout hidden | "Same fault on QNX 8.0 in QEMU: behaviour only." |
| 6 | 2:21-2:41 | 5 | `build_readme.py --check` PASS, then README Results (host Linux harness) and the Limits list on GitHub at `v1.0-rc1` | "Results: host (Linux) harness. Simulation-only research prototype." |
| end | 2:41-2:46 | - | End card: repository URL, simulation-only, synthetic-voice note | "Stale decisions can't move the robot." |

## Narration (final, 228 words, synthetic voice)

1. "An AI planner can be right about a world that no longer exists. In a hospital, that
   world changes fast. GUARD sits between the planner and a simulated robot, and only
   fresh, checked decisions can move it."
2. "Here we kill the supervisor in the middle of a run. With GUARD, the robot stops and
   latches. With a heartbeat-only baseline, the planner is still alive, so the robot keeps
   moving."
3. "The planner only proposes motion. The supervisor grants short leases. Once the
   supervisor is gone, nothing renews the lease. So the gate, a separate process with its
   own timer, stops the robot, and stays latched until an operator recovers it."
4. "Now the MRI zone switches on, while the planner still cites its old map. GUARD refuses
   the proposal with a reason code, and the robot waits outside. Without a supervisor, the
   robot drives straight through the active zone."
5. "This is the same fault on QNX 8.0, running in a QEMU virtual machine, with each part in
   its own process. guardctl injects the fault. The gate latches, and the cause is
   supervisor lost. Recovery is refused until its conditions hold. Here we show behaviour,
   not timings."
6. "Every number in our results comes from the host Linux harness, and is labelled that
   way. GUARD is a simulation-only research prototype, not for real robots or patients."
7. "Stale decisions can't move the robot."

## Claims

| # | Claim | Backed by (on `main`) |
|---|---|---|
| C1 | The planner only proposes; the supervisor authorizes with a lease | README "How it works" 1-3; `docs/architecture.md` |
| C2 | F3 under B2 ends LATCHED with `GATE_SUPERVISOR_LOST`; under B1 it ends MOVING | `scenarios/F3.yaml` `expected`; DEMO step 1 exit 0 |
| C3 | The gate stops the robot on its own timer when no lease is renewed | README intro; `docs/architecture.md` |
| C4 | F6 under B2 is refused with `REJ_MAP_VERSION` and has 0 violations; under B0 at least 1 | `scenarios/F6.yaml` `expected`; DEMO step 3 |
| C5 | `guardctl recover` is refused with `GATE_RECOVERY_UNMET` until recovery conditions hold | `contracts/guardctl.md`; on the target: `progress/P.md` F2 (`unmet: supervisor_registered`, exit 1) |
| C6 | Results are from the host (Linux) harness | README Results; `docs/results/host-linux.json` |
| C7 | Simulation-only research prototype | README Limits; `docs/limits.md` |
| C8 | The target run is shown as behaviour only (modes, latch cause, reason text) | DEMO step 4 output filters; `progress/P.md` F2 transcript; dashboard numbers hidden in capture (`docs/decisions/P.md`) |
| C9 | On the target each component is its own process | `qnx/README.md`; README Evidence Map (`process-*` bundles) |
| C10 | End card "Stale decisions can't move the robot." is the project tagline, shown after the evidence for it in simulation | README tagline; C2-C4 (stale lease: stop; stale map: refused) |
