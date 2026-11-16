# Stream P progress

Brief: [BRIEF-P.md](../docs/streams/BRIEF-P.md). Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream P`.

## Now
- P0 and P1 closed; P2 (demo script, storyboard and command check) next.

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

### P1 — README generator (2026-11-16)

`README.md` now has three generated blocks, each rendered from a source already on
`main`; `--check` fails if any of them is stale.

| Block | Source | What it renders |
|---|---|---|
| `gen:quickstart` | first shell block of `host/`, `dashboard/`, `image/README.md` | the three run paths (host replay, dashboard relay, on target) |
| `gen:results` | `docs/results/host-linux.json` | per-scenario Runs / B2 mismatches / baseline mismatches / ablation differences |
| `gen:evidence-map` | `evidence/*/manifest.json` | one row per bundle; target bundles labelled "behaviour only" |

Evidence:

```
$ python tools/build_readme.py --check
build_readme: PASS (README blocks match their sources on main)

$ python tools/check_readme_links.py
readme-links: PASS

$ python -m pytest tools/tests/test_build_readme.py -q
7 passed in 0.13s

$ python tools/check.py --fast
PASS policy ... PASS contract-compat ... PASS ownership ... PASS doc-links
PASS brief-sync ... PASS ruff ... PASS pytest (23s)
RESULT: GREEN
```

- README blocks stay byte-identical after `python tools/build_readme.py` (the
  `--check` gate is the same render).
- Results source: host-evaluation run
  [43876818251](https://github.com/Tauwho/GUARD/actions/runs/43876818251) at commit
  `4f1a18d`; Linux host harness, 1 repeat per scenario/config, B2 mismatches 0 in
  every scenario. Simulation only, labelled as such in the block.
- `python tools/scope_gate.py --stream P` shows P1 DONE.
- `--check` and `check_readme_links.py` never touch the network; only `--fetch`
  calls `gh`.
- **Outstanding:** the nightly `fresh-clone-and-tag` job still needs a run on this
  merge commit before P1's second half (fresh-clone nightly green) is evidenced.

## Next
- P2: `demo/DEMO.md`, `tools/check_demo_commands.py` (+ tests),
  `docs/video/storyboard.md` with shot list and captions plan.
- Then P5 (architecture SVG + Limits), P6, P3, P4.

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
