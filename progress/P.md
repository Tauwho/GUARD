# Stream P progress

Brief: [BRIEF-P.md](../docs/streams/BRIEF-P.md). Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream P`.

## Now
- P0 closed; P1 README generator next (`tools/build_readme.py`, markers, `--check`).

## Done (with evidence: command + last lines, commit or run link)

### P0 — Phase 0 merged (2026-11-16)

The zero-handoff harness and its audit fixes are on `main`; the audit itself is
in [REMEDIATION.md](../docs/streams/REMEDIATION.md).

| Audit fix | PR | Merged |
|---|---|---|
| 8 format stage SKIPPED locally, required in CI | #164 | 2026-11-16 |
| 1 scope cmd checks as argv lists | #165 | 2026-11-16 |
| 5 SELF_REVIEW + doc-link check | #166 | 2026-11-16 |
| 2 merge_group triggers + nightly red-main issue | #167 | 2026-11-16 |
| 6 `tooling-fix:` path (D27) | #168 | 2026-11-16 |
| 3 executable scope checks, MANUAL evidence | #169 | 2026-11-16 |
| 4 brief/SCOPE single source + drift check | #170 | 2026-11-16 |
| 7 P6 draft check + B4 early cut | #171 | 2026-11-16 |

Evidence:

```
$ PR_LABELS=phase0-override python tools/check.py --fast
PASS policy (2s) ... PASS contract-compat (1s) ... PASS ownership (0s)
PASS doc-links (0s) ... PASS brief-sync (0s) ... PASS ruff (0s) ... PASS pytest (19s)
RESULT: GREEN
```

- CI on `main` 4f1a18d: all 12 jobs success, including `make-check`
  (run 43876818332).
- Nightly on that commit: `tools/check.py`, `fresh_clone_demo.sh`, scope report,
  then a forced `green-YYYYMMDD` tag; run link and tag are added here when it
  lands (P1 keeps the tag fresh).
- Scope after P0: P DONE 1 (P0), OPEN 5, MANUAL 1 (P5 architecture diagram is
  still SHOULD work, not started).
- Self-review: the audit was the adversarial review for these eight PRs; each
  fix carries a test that failed before it.

## Next
- P1: `tools/build_readme.py` (`<!-- gen:evidence-map -->`,
  `<!-- gen:results -->`, `<!-- gen:quickstart -->`, `--check`), README rewrite,
  `tools/check_readme_links.py`; then P2 demo and storyboard.

## Decisions
- Logged in [docs/decisions/P.md](../docs/decisions/P.md).

## Research
- (P1: README/video structure of strong hackathon entries goes here.)

## Known issues
- **Stream B has not started:** B0 is OPEN, so every downstream B row is OPEN
  and B1's evidence correctly does not cover the core yet. P renders
  `planned (<id>)` and never waits.
- No `cmake`/`clang-format` in the local Git Bash, so a local `make check`
  without `--fast` reports host stages SKIPPED and format SKIPPED; CI is
  authoritative.
- `green-20261116` still points at 2011b07 (pre-audit). P1 refreshes it via the
  nightly run after the first P merge.
