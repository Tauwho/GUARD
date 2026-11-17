# Stream B progress

Brief: `docs/streams/BRIEF-B.md`. Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream B`.

## Now
- B2 `guard-gate` (#39): `qnx/gate` resource manager on `qnx/common`, wrapping `guard::Gate` from `core/`.
  - `/dev/guard/gate` devctls REGISTER/LEASE/REVOKE (uid 1103), RECOVER (uid 1106 or root); `/dev/guard/status` read (C4).
  - 10 ms tick pulse at priority 35; MotorCmd to `/dev/sim/motor` via `send_bounded` 5 ms (C5), timeout -> `motor_timeout()`.
  - Ack rule (ADR 0017): `prepare_lease`, `MsgReply`, `commit_lease(seq, now, delivered)`.
  - Supervisor lost: close_ocb of the registered OCB -> `supervisor_lost()`.
  - Evidence topic `process-gate` via `image/target-check.sh --project qnx/gate`.

## Done (with evidence: command + last lines, commit or run link)
- 16 Nov: spikes S2-S8 answered (DECISIONS.md spike log), `qnx/common` (#37), `qnx/hog`, `tools/capture-evidence.py`, image layout + `run-demo.sh` (#43).
- B1 core-on-target, main 5f957d9: `evidence/20261117-5f957d9-core-on-target/` (#189). `/tmp/guard_core_tests` -> `[doctest] test cases: 112 | 112 passed | 0 failed`, `Status: SUCCESS!`.
- B0 `image/target-check.sh`: `bash image/target-check.sh --help` prints usage; `bash image/target-check.sh --dry-run core-on-target` prints steps 1/6-6/6; a real `--keep core-on-target` run builds, copies with `ssh guard-target 'cat > ...'`, runs and writes a bundle.

## Next
- B3 sim / supervisor / planner, then B4 F0 on target.

## Known issues
- `Supervisor` is about 300 KB; keep it in static storage in `guard-supervisor`, not on a thread stack (two on the QNX main stack overflowed, fixed in tests by #188).
- `br0` is lost on WSL restart; default to `--net user` (ssh alias `guard-target`, port 2222).
