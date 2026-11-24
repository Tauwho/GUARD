# Stream P progress

Brief: [BRIEF-P.md](../docs/streams/BRIEF-P.md). Resume from this file after any restart. Live scope: `python tools/scope_gate.py --stream P`.

## Now
- Video ready at C:/Data/Singapore/SMU/GUARD/video/GUARD-demo.mp4 (2:46); waiting for
  LISTEN-NOTES.txt (PO listening check), then the upload URL in VIDEO-URL.txt.
- P0, P1, P2, P3, P5 and P6 DONE (`v1.0-rc1` = `bf4c417`). Left: P4 needs only the video
  URL in README and the submission draft (`check_package.py`: readme-video, submission).

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
- Fresh-context review: 2 blockers (AI disclosure ignored the bootstrap commit and
  implied human review; diagram showed unbuilt processes as if running) and 10
  shoulds (priority labels, C4 direction, telemetry producers, CSS markers,
  Limits parser strictness, README-current pytest breaking A/B CI). All fixed;
  new `tools/check_architecture_svg.py` (6 tests), 13 build_readme tests.
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

### P6 — submission draft (2026-11-16)

- `docs/devpost/description.md`: the 12 required sections, each statement sourced on
  `main` or marked `planned (<SCOPE id>)`. Describes GUARD as a design whose logic
  runs in the portable core and host harness today; QNX processes planned (B2-B5).
  Host-only numbers (run 43876818251, n=1 per scenario/config). AI-use disclosure
  matches the README.
- `docs/devpost/screenshots.md`: diagram (on main), two dashboard frames and the GIF
  (planned (P4)), README Results.
- README intro and Why-QNX paragraph now say the same (no running-process claim).
- Fresh-context fact check: 2 blockers (QNX processes described as built; stale-epoch
  leases described as a stop instead of a refusal) and 15 shoulds, all fixed.
  Supersedes the draft in #66.

Evidence:

```
$ python tools/check_submission.py
submission: PASS (description, screenshots, video link, AI-use; upload stays human)
$ python tools/check_policy.py claims-lint
claims-lint: PASS
$ python tools/check.py --fast
RESULT: GREEN
```

### P4 (part) — package gate (2026-11-16)

`tools/check_package.py` (`make package-check` runs it after `make check` and the
fresh-clone demo) checks: README current, links, no TBD, Results block cites the
Linux run, a YouTube/Vimeo video URL, `docs/video/duration.txt` under 3:00,
`captions.srt` strictly parsed and within the video, demo commands, architecture
SVG labels, submission draft with a video URL, tag `v1.0-rc1`, no tracked QNX-logo
image. 9 tests. Fresh-context review: 2 blockers (captions failed open on a
malformed cue; the video length itself was unchecked) and 9 shoulds, fixed.
Current state: FAIL on readme-video, video-duration, captions, submission (video
URL) and release-tag, which P3 (10 Dec) and the recording (13 Dec) produce.

### S2 (early) — results refresh (2026-11-17)

- `python tools/build_readme.py --fetch` -> run
  [43907070270](https://github.com/Tauwho/GUARD/actions/runs/43907070270) at `6220b91`
  (1 repeat per scenario/config, B2 mismatches 0 in every scenario); README regenerated.
- `docs/devpost/description.md` Results now cites that run and A's 50-seed run
  43887605751 (3,150 runs, 0 B2 mismatches), with the report's own independence caveat.
- Done early because A finished; S2 on Sat 5 Dec re-runs `--fetch` if main moves.

### S3 (early) — B landed: demo and package refresh (2026-11-17)

- Stream B on main at `3b61f5a`: scope B0-B7, B10, B11 DONE; B8 MANUAL with evidence in
  `progress/B.md`; B9 (STRETCH) OPEN. Main CI green.
- `demo/DEMO.md` target setup and step 4 are `ready` on `image/run-demo.sh --dashboard`;
  the fallback is gone. `python tools/check_demo_commands.py` -> `PASS (every ready
  command exists on main)`.
- README intro, Why-QNX and How-it-works, `docs/devpost/description.md` (What it does,
  QNX technologies, Results, What's next), the `docs/architecture.svg` header and the
  video shot list/storyboard now describe the built QNX processes, citing evidence
  bundles as behaviour only. `python tools/build_readme.py --check` -> PASS;
  `check_submission.py`, `check_architecture_svg.py`, `check_readme_links.py` PASS;
  `python tools/check.py --fast` -> `RESULT: GREEN`.
- S3 (B5 decision on 7 Dec) is settled early: B5 is DONE, the target scene is recorded.

### F2 — dry run of DEMO steps 1-5 (2026-11-17)

Fresh clone at `cb1b7fc` in WSL Ubuntu 22.04 (Python 3.11 venv, cmake 4.4.5); nightly
run [44098010412](https://github.com/Tauwho/GUARD/actions/runs/44098010412) on the same
commit passed `make check` and `tools/fresh_clone_demo.sh`.

- Host setup: build OK; `out/` created.
- Step 1: F3 B2 exit 0, `final_state LATCHED`; F3 B1 exit 0, `final_state MOVING`.
- Step 2: relay serves `http://127.0.0.1:8765` (checked in the step 3/4 runs).
- Step 3: F6 B2 exit 0, `final_state READY`, first rejection `REJ_MAP_VERSION`; F6 B0
  exit 0, robot reaches the goal through the zone (margin below 0).
- Target setup: `image/run-demo.sh --dashboard` builds, boots, starts sim, gate,
  supervisor, planner, bridge, loads F0 and starts the relay; the dashboard follows the
  target live (READY, decisions, journal).
- Step 4: `guardctl status` -> `mode=MOVING`, `latch_cause=NONE`; `inject F3` on the
  default image -> "process faults need the test image (run-demo.sh --test)". With
  `image/run-demo.sh --test --dashboard`: `injected F3`, then `mode=LATCHED`,
  `latch_cause=SUPERVISOR_LOST`; `recover` -> `unmet: supervisor_registered`, exit 1
  (the documented refusal path). DEMO target setup now uses `--test --dashboard`.
- Step 5: `python tools/build_readme.py --check` -> PASS.
- Local only: with cmake 4.x the QNX projects need `CMAKE_POLICY_VERSION_MINIMUM=3.5`
  in the environment (doctest's old `cmake_minimum_required`); B's own machine and CI
  are unaffected. Recorded under Known issues; no repo change by P.

### F1 — release candidate tag v1.0-rc1 (2026-11-17)

- Commit `bf4c417` (#204): CI green (host-ci, stream-a, governance, host-evaluation) and
  nightly run [44099787680](https://github.com/Tauwho/GUARD/actions/runs/44099787680)
  green (`make check`, `tools/fresh_clone_demo.sh bf4c417`).
- `git tag -a v1.0-rc1 bf4c417 -m "GUARD v1.0-rc1"`; `git push origin v1.0-rc1` ->
  `* [new tag] v1.0-rc1 -> v1.0-rc1`. Scope gate: P3 DONE.

### F3 — demo video produced (2026-11-17)

- Final cut 2:46 (166 s), 1920x1080 30 fps H.264 + AAC, recorded from `v1.0-rc1`. Kept
  outside the repo with its build scripts, claims ledger, audio licence log and QC report.
- Footage: host replays (F0, F3, F6 B2/B0) and the README on GitHub recorded as Edge's own
  rendering; terminal shots replay real captured runs verbatim (the target start log is trimmed to
  the GUARD lines, marked on screen); the target scene is a live
  QEMU run of DEMO step 4 with the dashboard's numeric readouts hidden (D11).
- Voice-over: Kokoro-82M (Apache-2.0, offline; voice af_heart) after the PO found the
  first Windows voice too robotic (synthetic, disclosed in `docs/devpost/description.md`
  and on the end card). No music, no sound effects.
- `docs/video/captions.srt` (26 cues, last ends 164.7 s) and `docs/video/duration.txt`
  (2:46); storyboard re-timed to the cut. `check_package.py`: only readme-video and
  submission (video URL) remain.

## Next
- P5 (architecture SVG + Limits), then P6, P3 (10 Dec), P4 (13 Dec).
- Mon 7 Dec: if B5 is not DONE, switch DEMO step 4 to the host fallback by hand.

## Decisions
- Logged in [docs/decisions/P.md](../docs/decisions/P.md).

## Research
- (P1: README/video structure of strong hackathon entries goes here.)

## Known issues

- Building the QNX image with cmake 4.x needs `CMAKE_POLICY_VERSION_MINIMUM=3.5` in the
  environment (FetchContent doctest). B-owned build files; P works around it locally.
- A9 (STRETCH, A): the brief's "second-laptop evidence" is replaced by the PO decision on
  #29 (issuecomment-6921570197): Dat's real Qwen2.5-1.5B run plus the CI offline/recorded
  tests are the acceptance; a second machine is optional. SCOPE.yaml is frozen, so A9 keeps
  showing MANUAL. P docs never cite a second-machine inference run.
- SCOPE.yaml P4's grep check `(?i)video` on README already matches "Video and GIF:
  planned (P4)"; SCOPE.yaml is frozen, so P4's real gate is its other check,
  `python tools/check_package.py` (video URL, duration, captions).
- **Stream B has not started:** B0 is OPEN, so every downstream B row is OPEN
  and B1's evidence correctly does not cover the core yet. P renders
  `planned (<id>)` and never waits.
- No `cmake`/`clang-format` in the local Git Bash, so a local `make check`
  without `--fast` reports host stages SKIPPED and format SKIPPED; CI is
  authoritative.
- `green-20261116` still points at 2011b07 (pre-audit). P1 refreshes it via the
  nightly run after the first P merge.


## #222 local video re-cut (2026-11-24, prepared by A on po/ ownership branch)
- Local H.264/AAC export: 1920x1080, 30 fps, duration 2:51.02. No video/raw trace/model committed.
- Retain original scenes, insert remote-LLM scene from B take2 actual C8 telemetry at original pacing. Supplied dashboard MP4 froze, so this scene is a custom recorded-target-trace visualisation, explicitly labelled throughout, not new live capture.
- REJ_MAP_VERSION is attributed to the planner; ROUTE_USABLE never authorises motion. Target numeric readouts omitted; excludes post-run MOTOR_TIMEOUT. Results and full current Limits are rendered repository excerpts, including obstacle-motion limitation.
- New Kokoro af_heart voice, existing narration retained; no music/effects. Matching 33-cue SRT; no subtitle extends beyond the exported video.
- #222 explicitly requests dngvmnh claims review before merge: keep PR draft pending that review, including approval of the replay/card substitutions. Old public video URL stays unchanged; upload/README/Devpost update belongs to P.
- Validation results recorded in the PR; raw sources and local export stay in ignored out/video222/.

- #222 validation: `python tools/check_package.py` -> `package-check: PASS`; `python tools/check_policy.py all` -> core-portable/no-qnx-files/claims-lint/schemas/evidence PASS. Full MP4 decode exits 0; audio peak below clipping. Fresh-context media/docs review: 0 blockers; required-reason wording corrected, no outstanding should findings.
