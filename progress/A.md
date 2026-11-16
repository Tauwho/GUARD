# Stream A progress

Brief: `docs/streams/BRIEF-A.md`. Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream A`.

## Now
- A4: continuous Linux matrix, explicit B2 gate, unmeasured margin handling and isolated fresh-clone CI.
- A1/A2: verify existing fixes against frozen v1.2, add passthrough refusal regression.
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
- A3 target runner local/SSH, A4 continuous Linux matrix, A5 dashboard bridge resilience.

## Known issues

A4: local checks passed; Linux main artifact pending (not yet done).
- `python tools/check.py`: RESULT: GREEN (out/d26-a4-check.txt).
- run_start.ablate is already emitted under frozen v1.2; preserve it.
