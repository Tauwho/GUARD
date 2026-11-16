# Stream A progress

Brief: `docs/streams/BRIEF-A.md`. Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream A`.

## Now
- A5: preserve physical JSONL framing after corrupt bridge lines, validate browser-finite numbers, smoke UI headlessly.
- A3: implement local/SSH C14 runner, validate real F0/F3, review cleanup/deadline behavior.
- A1/A2: verify existing fixes against frozen v1.2, add passthrough refusal regression.
- A4: continuous Linux matrix, explicit B2 gate, unmeasured margin handling and isolated fresh-clone CI.
- Run full tools/check.py and F5/F6 acceptance commands.
- Fresh-context adversarial review, fix findings, self-merge on green CI.

## Done (with evidence: command + last lines, commit or run link)

A1: done 4b10a4ce12d61b3537d321c062eef43495818eed
- `ctest --preset host-debug`: 2/2 passed, 111 core test cases.
- `python tools/check.py`: all stages PASS, `RESULT: GREEN` (Windows clang; clang-format 18).
- Passthrough regression failed before fix (REJ_SCHEMA instead of acceptance after braking); now passes. Lease refusal/recheck regressions from #152 still pass.
- Fresh-context review: no blocker/should findings (review_a1).

A2: done 25fc74b
- `python host/guard-replay --scenario scenarios/F5.yaml --mode B2 --out out/d26-F5.jsonl`: exit 0, mismatches [], GOAL.
- `python host/guard-replay --scenario scenarios/F6.yaml --mode B2 --out out/d26-F6.jsonl`: exit 0, mismatches [], READY, REJ_MAP_VERSION.
- Frozen v1.2 consumed unchanged; Linux evidence run https://github.com/Tauwho/GUARD/actions/runs/43847940386.

## Next
- A5 dashboard bridge resilience; A6 advisor recorded default; A7 seeded trials.

## Known issues

A4: local checks passed; Linux main artifact pending (not yet done).
- `python tools/check.py`: RESULT: GREEN (out/d26-a4-check.txt).
- run_start.ablate is already emitted under frozen v1.2; preserve it.

A3: done (local acceptance)
- `python -m pytest eval/tests/test_target_runner.py -q`: 11 passed, including actual core F0/F3.
- `python tools/check.py`: RESULT: GREEN after all review refinements (out/d26-a3-final.txt).
- Local C14 command log includes load/start/process inject/stop; SSH transport has bounded capture and BatchMode auth.
- Fresh-context review: fixed top-level duration, requested seed/multiple-run validation, out-of-duration injection and stop-after-refused-load findings. Validate optional frozen C8 run_id when supplied; enforce shared injection deadline.
- SSH hardware not claimed: tests use local core and transport mocks. Target execution belongs to B.


A5: done (host acceptance)
- `python -m pytest eval/tests/test_dashboard_relay.py -q`: 7 passed.
- Headless Node smoke: PASS (seek, lease, source arc, latch, reason feed).
- Actual localhost HTTP/TCP tested; malformed/schema-invalid/non-finite and chunked oversized lines drop without poisoning later records.
- Lease telemetry uses retained proposal source pose; no synthetic newer-pose fallback.
- Fresh-context findings fixed: oversized physical-line framing and huge integer rejection.
