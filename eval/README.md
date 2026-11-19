# Functional evaluation

Build the host executable first (host/README.md). From the repository root:

```sh
cmake --preset host-release && cmake --build --preset host-release
python eval/run_matrix.py --seeds 50 --jobs 4 --binary build/host-release/host/guard-replay-core --out out/matrix
python eval/report.py out/matrix/results.json --out out/eval-report
```

Without `--binary` the matrix uses the Debug build, which produces byte-identical traces
but takes about three times as long. Each record carries `replay_seconds` (the wall time of
that one replay process). The printed summary reports `max_replay_seconds` and `wall_seconds`
for the host that ran it.

The default matrix executes 9 scenarios × 50 seeds × 7 configurations (3,150 runs).
`--seeds 1` is the 63-run smoke matrix. Exit 1 means mismatches were recorded;
check `results.json` rather than treating them as a successful evaluation.
Each execution writes raw C8 JSONL. Identical telemetry bodies reuse schema validation
and measurement by SHA256; every simulation executes and each header is validated.

The report includes functional episode counts, clearance, stop/standstill intervals,
goal time, F0 rejection rate, a PNG matrix and explicit mismatches. Operator recovery
trials are not implemented; their result stays unavailable. All nine B2 scenarios match on the Linux host harness; baseline F1
still differs from the shared hypothesis (issue #22). Ablations use B2 as reference.

Seeds now select bounded, reproducible host perturbations: a common staff-event
start offset within +/-100 ms and planner-cadence delay of 0..5 ms. The scenario
reference seed remains unchanged; flood cadence stays exact. All modes use the
same staff offset for a given seed. These are seeded trials, not a claim of
statistical independence; some modes may share traces. Compare trace-body hashes
and retain the configured variation limits in reports. Repeating a seed is byte-identical.

ADR0011/D11 applies: publish numeric results only from the host Linux harness. The tool
records the real host platform and marks Windows reports preliminary/internal. Target
verification is described separately in words; target numeric tables are not imported.
Generated JSONL, JSON reports and PNGs belong under ignored `out/`, not in git.

## Property tests

`pytest eval/tests -k hypothesis` needs the Debug build (`guard-core-probe`). Five
properties draw 500 examples each against the core probe. One draws 40 shifted
F0-F8 schedules (faults and zones, and staff crossings, moved separately) with
seeds 0..49 and replays each under B2. It asserts zero zone breaches and no contact
while the gate is MOVING. `tests/counterexamples/` holds the shrunk scenarios where
B2 still records a contact (#215). They are strict xfails, so once a fix makes them
pass, the suite fails until the xfail mark is removed. They are kept out of `scenarios/` so the image does not install
them.


## C14 target runner

```sh
python eval/target_runner.py --target local --scenario scenarios/F0.yaml --out out/local-F0.jsonl
python eval/target_runner.py --target local --scenario scenarios/F3.yaml --out out/local-F3.jsonl
python eval/target_runner.py --target ssh:guard-target --bridge 127.0.0.1:7700 --scenario scenarios/F3.yaml --remote-path /system/guard/scenarios/F3.yaml --out out/target-F3.jsonl
```

Local mode exercises the real host core in virtual time behind a C14 command log;
process faults are modeled, not real process kills. SSH mode requires an existing
SSH config alias, normalized scenario/map files already installed on the target and
a locally forwarded C8 bridge. No credentials are accepted or stored. It issues
load/start, schedules C14 injection for process faults F2/F3/F7, and always attempts
stop. Model faults remain in the loaded World timeline to avoid double injection.
The bridge must emit run_start/run_end; traces and lines have bounded size/deadlines.
Every C8 event and the requested scenario/mode are validated. Exit 0 = expectations
met, 1 = mismatches, 2 = capture/protocol/input failure. Local evidence is not a target
run. Keep target trace/results local; do not publish QNX timing numbers.
