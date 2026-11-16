# scenarios/

PO-owned acceptance tests (issue #4). **Freeze: Sat 28 Nov 2026 23:59 SGT.**
Every file validates against `contracts/scenario.schema.json` (C11). Expected outcomes are **hypotheses**; `eval/` reports what actually happens.

## Maps

| File | Layout | Used by |
|---|---|---|
| `maps/hospital_corridor.yaml` | 40 m × 6 m corridor, Ward A (west) to Lab (east). Restricted zone at the MRI door on the north side; the lane at y = 2.0 m keeps clear of it. Two staff crossings at x = 12 m and x = 30 m | F0-F5, F7, F8 |
| `maps/mri_suite.yaml` | 30 m × 12 m. Corridor on the south side, MRI room in the middle, imaging store to the north-east. The diagonal to the store crosses the MRI zone, which starts inactive | F6 |

Zone activation changes `map_version`. A zone's `active` is a boolean or a list of `{from_s, to_s}` windows; scenarios can also switch zones with `zones:` events.

## Scenarios

| ID | Fault | Map | Semantics | B2 hypothesis |
|---|---|---|---|---|
| F0 | None | corridor | model | 0 violations, 0 latches, false-reject rate ≤ 5 % |
| F1 | Stale planner (600 ms lag at t = 10 s) | corridor | model | `REJ_STALE_OBSERVATION`, stop, no contact, then GOAL |
| F2 | CPU hog at prio 32 for 400 ms | corridor | **process** | `GATE_LEASE_EXPIRED`, STOPPED, then GOAL |
| F3 | `kill -9` supervisor at t = 12 s | corridor | **process** | Stop within 0.26 s, LATCHED (`GATE_SUPERVISOR_LOST`) |
| F4 | Supervisor feed paused 1 s | corridor | model | `UNV_SENSOR_STALE`, revoke, stop, latch |
| F5 | Aggressive 1.2 m/s toward a crossing | corridor | model | `REJ_STOPPING_DISTANCE`, then a lower speed |
| F6 | MRI zone activates at t = 8 s, shortcut planner | mri_suite | model | `REJ_MAP_VERSION` + `REJ_RESTRICTED_ZONE`, robot waits outside |
| F7 | Zombie supervisor (SIGSTOP / new epoch / SIGCONT) | corridor | **process** | `GATE_EPOCH_CHANGED`, `GATE_EPOCH_STALE`, LATCHED |
| F8 | 1000 proposals/s for 2 s | corridor | model | `REJ_RATE_LIMIT`, gate never late, GOAL |

Each file holds an `expected` block for B0 (passthrough), B1 (heartbeat) and B2 (GUARD). Ablation modes have no expected block; eval reports them as measured.

## Process faults on the host

F2, F3 and F7 are marked `target_semantics: process`. On the target they act on real QNX processes (`guard-hog`, `kill -9`, `SIGSTOP`/`SIGCONT`) via `guardctl inject`. The host harness (`guard-replay`, A-7) cannot do that, so each file's `host_abstraction` says what the harness models instead:

- **F2:** no decisions or proposals are delivered for the hog window; gate tick and sim keep running.
- **F3:** the supervisor model stops and its gate session closes.
- **F7:** supervisor model A freezes, model B registers a new epoch, then A sends one lease with its old epoch.

The video and README results for these three come from target runs.

## Fields in brief

- `seed`: base seed. The eval matrix adds the seed index (0-49 on the host, 0-4 on the target).
- `obstacles`: timeline of `spawn`, `despawn`, `start_crossing` (uses a map crossing) and `stop`.
- `faults`: timeline of F1-F8 with optional `duration_ms` and `params`.
- `expected.<mode>.violations`: an exact count, or `{min, max}`.
- `final_state`: a gate mode, or `GOAL` (reached the goal and stopped).
