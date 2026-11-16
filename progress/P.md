# Stream P progress

Brief: [BRIEF-P.md](../docs/streams/BRIEF-P.md). Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream P`.

## Now
- P0, P1 and P2 closed; P5 in review.

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
- Merged as #173 (`c9a79a0`). Nightly on that commit: run
  [43883766653](https://github.com/Tauwho/GUARD/actions/runs/43883766653) success
  (`make check` + `fresh_clone_demo.sh` + scope report); tag `green-20261116` now
  points at `c9a79a0`. This also refreshes P0's fresh-clone-passing commit.

### P2 — demo script, command check and storyboard (2026-11-16)

- `demo/DEMO.md`: host setup, target setup (planned (B5)), then five timed steps:
  1 F3 host replay B2 vs B1, 2 dashboard replay, 3 F6 refusal `REJ_MAP_VERSION`
  (B2 vs B0, with `--map`), 4 the same F3 fault on target with `guardctl`
  (planned (B5); output filtered to mode and latch cause), 5 README Results and
  Limits. Slots end at 2:55.
- `tools/check_demo_commands.py`: `--help` of every Python entry point plus each
  `--flag` the demo uses, `bash -n` for shell scripts, `qnx/<name>/` sources for
  target programs, CMake preset names; compound lines, `\` continuations, ssh
  options and non-`sh` fences handled; a planned step fails once its SCOPE id is
  DONE. Not a `make check` stage, and its pytest never runs real CLIs, so it cannot
  fail A's or B's CI.
- `docs/video/storyboard.md`, `shot-list.md`, `captions.md`: scenes mapped to DEMO
  steps, narration, claims table with sources on `main`, shots, captions plan.
  Supersedes the draft in #65.
- Self-review in a fresh context: 2 blockers (QNX numbers from `guardctl status`
  on screen; pytest running real CLIs) and 16 shoulds, all fixed or logged in
  `docs/decisions/P.md`.

Evidence:

```
$ python tools/check_demo_commands.py
check_demo_commands: PASS (every ready command exists on main)
$ python -m pytest tools/tests/test_check_demo_commands.py -q
21 passed
$ python tools/check.py --fast
RESULT: GREEN
$ python tools/scope_gate.py --stream P   ->   P2 ... DONE
```

### P5 — architecture diagram and Limits final (2026-11-16)

- `docs/architecture.svg`: hand-written SVG (no generator, no QNX logo). Processes
  on the target (planner, supervisor, gate, simulator, bridge, guardctl) with their
  REF-PRIORITIES priorities and the C1-C8 contract arrows; dashboard and
  guard-replay on the host. Embedded in README "How it works".
- Limits: `docs/limits.md` is now the single source. README Limits is a generated
  block (`gen:limits`), so `build_readme.py --check` fails on drift. The README had
  drifted ("No timing guarantee" vs "No hard real-time guarantee"); the limits.md
  wording wins. `docs/architecture.md` REF-CLAIMS points at limits.md instead of a
  third copy.
- `build_readme.py` writes LF on Windows (it wrote CRLF before).
- README AI disclosure corrected: agents open and merge PRs under D12/D26; the old
  text said a human merged every change.

Evidence:

```
$ python tools/build_readme.py --check
build_readme: PASS (README blocks match their sources on main)
$ python -m pytest tools/tests/test_build_readme.py -q
9 passed
$ python tools/check_policy.py claims-lint
claims-lint: PASS
$ python tools/check.py --fast
RESULT: GREEN
$ python tools/scope_gate.py --stream P   ->   P5 ... DONE
```

## Next
- P5 (architecture SVG + Limits), then P6, P3 (10 Dec), P4 (13 Dec).
- Mon 7 Dec: if B5 is not DONE, switch DEMO step 4 to the host fallback by hand.

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
