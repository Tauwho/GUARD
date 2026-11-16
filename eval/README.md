# Functional evaluation

Build the host executable first (host/README.md). From the repository root:

```sh
python eval/run_matrix.py --seeds 50 --jobs 4 --out out/matrix
python eval/report.py out/matrix/results.json --out out/eval-report
```

The default matrix executes 9 scenarios × 50 seeds × 7 configurations (3,150 runs).
`--seeds 1` is the 63-run smoke matrix. Exit 1 means mismatches were recorded;
check `results.json` rather than treating them as a successful evaluation.
Each execution writes raw C8 JSONL. Identical telemetry bodies reuse schema validation
and measurement by SHA256; every simulation executes and each header is validated.

The report includes functional episode counts, clearance, stop/standstill intervals,
goal time, F0 rejection rate, a PNG matrix and explicit mismatches. Operator recovery
trials are not implemented; their result stays unavailable. All nine B2 scenarios match on the Linux host harness; baseline F1
still differs from the shared hypothesis (issue #22). Ablations use B2 as reference.

Current seeds label deterministic repetitions without physical perturbations. Do not
interpret the matrix as 50 independent randomized trials per scenario/configuration.
Retain this limitation when using the results. Check trajectory hashes in `results.json`.

ADR0011/D11 applies: publish numeric results only from the host Linux harness. The tool
records the real host platform and marks Windows reports preliminary/internal. Target
verification is described separately in words; target numeric tables are not imported.
Generated JSONL, JSON reports and PNGs belong under ignored `out/`, not in git.


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
