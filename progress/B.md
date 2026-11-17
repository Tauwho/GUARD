# Stream B progress

Brief: `docs/streams/BRIEF-B.md`. Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream B`.

## Now
- B3: `guard-supervisor`, then `guard-planner`; then `guard-bridge` + `guardctl` so F0 runs through `eval/target_runner.py`.

## Done (with evidence: command + last lines, commit or run link)
- 16 Nov: spikes S2-S8 answered (DECISIONS.md spike log), `qnx/common` (#37), `qnx/hog`, `tools/capture-evidence.py`, image layout + `run-demo.sh` (#43).
- B1 core-on-target, main 5f957d9: `evidence/20261117-5f957d9-core-on-target/` (#189). `/tmp/guard_core_tests` -> `[doctest] test cases: 112 | 112 passed | 0 failed`, `Status: SUCCESS!`.
- B0 `image/target-check.sh`: `bash image/target-check.sh --help` prints usage; `bash image/target-check.sh --dry-run core-on-target` prints steps 1/6-6/6; a real `--keep core-on-target` run builds, copies with `ssh guard-target 'cat > ...'`, runs and writes a bundle.

- B2 `guard-gate` (`qnx/gate`): `bash image/target-check.sh --project qnx/gate --copy guard-gate --copy guard_gate_check --run "/tmp/guard_gate_check /tmp/guard-gate" process-gate` -> 16 CHECK lines PASS, `RESULT PASS (0 failed)`; `evidence/20261117-2395caa-process-gate/`.
- B3 part 1 `guard-sim` (`qnx/sim`, #38) and the `qnx/common` telemetry ring: `bash image/target-check.sh --project qnx/sim --copy guard-sim --copy guard_sim_check --run "slay -f guard-gate guard-sim; /tmp/guard_sim_check /tmp/guard-sim /system/guard/scenarios/F0.yaml" process-sim` -> 35 CHECK lines PASS, `RESULT PASS (0 failed)`; `evidence/20261117-f948929-process-sim/`. `qnx/common` tests 23/23 (`evidence/20261117-f948929-qnx-common-tlm/`). Image packs the 13 normalized inputs (#141).
## Next
- B3 sim / supervisor / planner, then B4 F0 on target.

## Known issues
- `Supervisor` is about 300 KB; keep it in static storage in `guard-supervisor`, not on a thread stack (two on the QNX main stack overflowed, fixed in tests by #188).
- `br0` is lost on WSL restart; default to `--net user` (ssh alias `guard-target`, port 2222).
