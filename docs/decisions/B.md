# Stream B decisions (@baofrovn)

Decide, log one line, continue (D26). Never edit `DECISIONS.md`.

| Date | Decision | Reason |
|---|---|---|
| 2026-11-17 | `guard-gate` passes its own last `MotorCmd` as `v_meas` to `Gate::tick` | C2-C5 give the gate no observation feed; braking reaches standstill from the commanded ramp. Revisit with an additive `contracts/ext/B-*` gate feed if B3/F-runs need measured speed |
| 2026-11-17 | Recover conditions `outside_zones` and `observations_fresh` are `true` in `guard-gate` until `guard-sim` exists | No pose or freshness source on the target yet; standstill and registration are still checked by the core |
| 2026-11-17 | C4 snapshot is taken at the first read at offset 0 of each open, held in an 8-slot per-OCB table | Same reader-visible result as "snapshot at open" without a custom OCB type; a ninth concurrent reader gets `EAGAIN` |
| 2026-11-17 | Gate telemetry ring and `slog2` deferred to B5 with `guard-bridge`; mode changes go to stdout | The ring layout is consumed only by the bridge; nothing reads it before B5 |
| 2026-11-17 | Root-built `guard_core` is pulled into each `qnx/<proc>` project with `add_subdirectory(../.. EXCLUDE_FROM_ALL)` | Uses A's own source list, so no copy of it in B's files |
