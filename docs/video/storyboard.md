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

Total **2:51.02** (new local export for #222; duration.txt rounds up to 2:52),
hard limit under 3:00. Existing scenes reuse the published v1.0-rc1 footage.
The new remote scene comes from B's take2 on main `09491a1`, develop image,
secpol off, actual Ollama Qwen2.5-1.5B run. Its supplied dashboard video froze;
the new visualisation is rendered from the matching recorded target telemetry,
continuously labelled **Recorded target trace replay**, at original event pacing.
It is not a newly captured live dashboard. This explicit substitution and the
claims table require @dngvmnh review before merge/upload (#222).

## Scenes

| # | Time | DEMO step | On screen | Caption text |
|---|---|---|---|---|
| 1 | 0:00-0:16 | 1 | Retained F0 normal host replay | GUARD checks motion proposals. |
| 2 | 0:16-0:36 | 1 | Retained F3 B2 vs B1 terminal | Supervisor lost: stop and latch. |
| 3 | 0:36-1:01 | 2 | Retained F3 host dashboard | A separate gate expires motion leases. |
| 4 | 1:01-1:26 | 3 | Retained F6 B2 vs B0 split | Stale map: refused with a reason. |
| 5 | 1:26-1:56 | 4 | Retained QNX F3 footage; one silent gap removed, no speed change | Target behaviour only. |
| 6 | 1:56-2:26 | 5 | Recorded target trace replay; original pacing; route reason text plus robot poses from the same trace | Planner rejects REJ_MAP_VERSION, then a later route is ROUTE_USABLE. |
| 7 | 2:26-2:46 | 6 | Rendered excerpts of current repository Results and Limits, including obstacle-motion limitation | Host (Linux) results; simulation-only prototype. |
| end | 2:46-2:51 | - | Repo URL, simulation-only, synthetic Kokoro voice and recorded-trace disclosure | Public upload remains with P. |

## Narration (re-cut; retained narration plus new synthetic voice)

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
6. "The planner can also ask a local language model for a route. It checks every suggestion itself. This route cites an old map, so it is rejected before it ever reaches the supervisor. Motion still needs a lease. The model advises; it never authorises."
7. "Every number in our results comes from the host Linux harness, and is labelled that
   way. GUARD is a simulation-only research prototype, not for real robots or patients."
8. "Stale decisions can't move the robot."

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

| C11 | The planner rejects the stale route itself as REJ_MAP_VERSION; a later route is ROUTE_USABLE | `host/ADVISOR.md` On the target; `qnx/planner/src/main.cpp`; `evidence/20261119-36f0bf2-remote-planner/manifest.json`; B take2 C8 source retained locally |
| C12 | A usable model route never directly authorises motor motion | `host/ADVISOR.md`; ordinary C1 supervisor + gate path |
| C13 | No prediction of obstacle motion | `docs/limits.md`; decision #215 |

## New scene provenance and caveats (#222)

- Source archive: guard-222-remote-planner-take2.zip, B's actual F6 B2 run with
  Qwen2.5-1.5B, seed 1006, main 09491a1, develop image (not secure secpol).
- Window retains original trace pacing and excludes run_end and the subsequent
  MOTOR_TIMEOUT latch after the sim stopped publishing. No target counters,
  clocks, speed, timestamps, lease durations or numeric results are shown.
- The take2 dashboard file froze near the beginning. The new scene uses a custom
  replay visualisation of the recorded robot poses/map state plus route reasons
  from that same C8 trace, not simulated or fabricated replacement output. The
  map geometry comes from scenarios/maps/mri_suite.yaml. Route messages are
  deduplicated for readability; their first appearance follows trace timestamps.
- Remote mode reaches GOAL rather than F6's expected READY; the planner rejects
  the stale route before it reaches the supervisor, so the runner's required C1
  reason is absent. These two expected runner differences are not called F6 PASS.
- Results/limits cards are rendered repository excerpts, not new GitHub browser
  recordings. The old published video URL remains unchanged until P uploads this
  cut and updates README/Devpost. Package-check validates files, not that upload.
- Raw footage, logs, scripts, model and inference timings remain ignored/local.
