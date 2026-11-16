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
trials are not implemented; their result stays unavailable. F5/F6 and baseline F1
currently differ from the shared hypotheses (issue #22). Ablations use B2 as reference.

Current seeds label deterministic repetitions without physical perturbations. Do not
interpret the matrix as 50 independent randomized trials per scenario/configuration.
Retain this limitation when using the results. Check trajectory hashes in `results.json`.

ADR0011/D11 applies: publish numeric results only from the host Linux harness. The tool
records the real host platform and marks Windows reports preliminary/internal. Target
verification is described separately in words; target numeric tables are not imported.
Generated JSONL, JSON reports and PNGs belong under ignored `out/`, not in git.
