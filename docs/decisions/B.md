# Stream B decisions (@baofrovn)

Decide, log one line, continue (D26). Never edit `DECISIONS.md`.

| Date | Decision | Reason |
|---|---|---|
| 2026-11-17 | `guard-gate` passes its own last `MotorCmd` as `v_meas` to `Gate::tick` | C2-C5 give the gate no observation feed; braking reaches standstill from the commanded ramp. Revisit with an additive `contracts/ext/B-*` gate feed if B3/F-runs need measured speed |
| 2026-11-17 | Recover conditions `outside_zones` and `observations_fresh` are `true` in `guard-gate` until `guard-sim` exists | No pose or freshness source on the target yet; standstill and registration are still checked by the core |
| 2026-11-17 | C4 snapshot is taken at the first read at offset 0 of each open, held in an 8-slot per-OCB table | Same reader-visible result as "snapshot at open" without a custom OCB type; a ninth concurrent reader gets `EAGAIN` |
| 2026-11-17 | Gate telemetry ring and `slog2` deferred to B5 with `guard-bridge`; mode changes go to stdout | The ring layout is consumed only by the bridge; nothing reads it before B5 |
| 2026-11-17 | B2 lands as one PR of about 700 lines | D18 needs the process and its evidence together; the on-target check is half of it and cannot run without the server |
| 2026-11-17 | Root-built `guard_core` is pulled into each `qnx/<proc>` project with `add_subdirectory(../.. EXCLUDE_FROM_ALL)` | Uses A's own source list, so no copy of it in B's files |
| 2026-11-17 | Telemetry: one lock-free shared-memory ring per process (`/guard_tlm_<src>`, 512 lines), single writer, read-only readers; a full ring overwrites the oldest line and the reader counts the drop | The writer never blocks or waits (AGENTS.md: the critical path never blocks on telemetry); `guard-bridge` needs no connect (REF-SECURITY `bridge_t`) |
| 2026-11-17 | `guard-sim` writes `run_start` and a `run_end` with its own fields (contacts, zone breaches, margin, duration, GOAL); `guard-bridge` fills in the C1 counters, latches and the non-GOAL final state from the events it forwarded | Only the bridge sees every process's events; the sim has no C1 or gate connection (REF-SECURITY edges) |
| 2026-11-17 | `scenario stop` (WorldPause 1) ends the run (`run_end`, state ENDED); a new run needs `scenario load` | target_runner calls stop only after run_end or on abort; a paused-and-resumed clock would need a C7 change |
| 2026-11-17 | `/dev/sim/world` `read()` is open to every uid; `devctl` stays uid 1106 or root | Supervisor and planner read the world status (zones, map version, active faults) without a new wire type; REF-SECURITY already lists `planner_t -> sim_t` world status read |
| 2026-11-17 | Observation telemetry is written once per sample (feed `planner`, `seq` = `obs_id`) | Matches the host harness, which logs one record per sample |
| 2026-11-17 | Injected F6 activates the first map zone whose name contains `mri`; scenario F6 uses the World timeline | C7 names no zone; the MRI zone is the one REF-SCENARIOS describes |
| 2026-11-17 | `guard-sim` accepts C5 `MotorCmd` with EOK while no scenario is loaded | The gate sends every tick from boot; an error would latch `MOTOR_TIMEOUT` before any run |
| 2026-11-17 | `WorldLoad.scenario_path` must be under `/system/guard/scenarios/` or `/data/guard/scenarios/` and contain no `..`; anything else is `ENOENT` | The single-threaded sim must never open a device path (its own path would block the loop) |
| 2026-11-17 | `guard-supervisor` holds its one state lock (priority inheritance) across the 5 ms-bounded gate `devctl` | The gate answers in the same handler call (ADR 0017); releasing the lock mid-lease would let a revoke race the install |
| 2026-11-17 | A revoke the gate did not answer stays pending and is resent on each 20 ms monitor pulse until EOK, EINVAL or lease expiry | A lost revoke must not leave motion running on the supervisor's word alone |
| 2026-11-17 | `guard-supervisor` takes map, config, ablations and zone state from `/dev/sim/world`; a new run builds a new `guard::Supervisor` and revokes any lease left from the last run | One source of truth for the loaded scenario; no new wire type |
| 2026-11-17 | `guardctl scenario start` sends one C3 recover first when the gate is LATCHED | The host harness recovers the BOOT latch itself; `eval/target_runner.py` calls only load/start/stop, so a run after boot or after F3/F4 would never move. The gate still checks every recovery condition |
| 2026-11-17 | `guard-bridge` holds lines 200 ms, sends them in `t_ns` order, and raises a late line's `t_ns` to the last one sent | C8 needs a non-decreasing stream per run; four writers at different priorities finish lines out of order |
| 2026-11-17 | Scenario bundles put the C8 trace in `results.json` with `t_ns` replaced by the event index and every `*_ns` payload field and time summary field removed | C13 lists no trace file, and D11 forbids publishing anything measured on QNX; order, reasons and counts stay |
| 2026-11-17 | `guardctl` bounds `open()` with a 100 ms interval timer as well as each `devctl` with `TimerTimeout` | `open()` is also a message to the resource manager; a stopped sim blocked it for ever (exit 4 now) |
| 2026-11-17 | F3 and F7 use `slay` (`SIGKILL`; `SIGSTOP`, new supervisor, `SIGCONT` after the duration in a detached child) | No pid files; the image has one supervisor except during F7, where `SIGCONT` to both is harmless |
| 2026-11-17 | Process-fault events (F2, F3, F7) come from `guardctl inject` (ring `/guard_tlm_guardctl`, src `harness`, stamped before it acts); the sim no longer emits them from the scenario timeline | The timeline time is not when the fault happened on the target: the ssh round trip made F3's stop time look 0.5 s late |
| 2026-11-17 | F2 starts one `guard-hog` per CPU, each pinned | QEMU runs `-smp 2`; one hog leaves a CPU free and the fault does nothing |
| 2026-11-17 | A second `guard-supervisor` (F7) finds `guard/supervisor` taken, registers with the gate at once (new epoch) and waits for the name | `name_attach` gives `EEXIST` while the stopped supervisor lives; the epoch change is what F7 tests |
