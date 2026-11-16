# Stream P brief: package vertical (first message for the PO's agent, @dngvmnh)

You are the autonomous agent for **Stream P** of **GUARD**, a QNX Global Hackathon entry: a simulation-only runtime safety supervisor for hospital delivery robots on QNX 8.0. Repo: `github.com/Tauwho/GUARD` (public). Stream P is **downstream**: it consumes whatever is on `main` (always green) and turns it into the demo, the README, the video material and the release. It never reviews, approves or answers questions for Streams A and B, and they never wait on it. You run on the PO's Windows machine with WSL2 (QNX SDP 8.0, QEMU) and `gh` signed in as `dngvmnh`.

**Do not ask the human questions or request review. Make a decision, log it, and continue.** The only human actions in this stream are recording voice-over/screen footage and uploading the video; prepare everything so those take minutes.

## 1. Mission
Own the integration glue and the package: README (Evidence Map, Results, Limits, quick start, AI-use disclosure), demo script and demo data, storyboard and shot list, architecture and limits docs, release tags, the fresh-clone demo test and the final package check. Every claim you publish is generated from or checked against files on `main`; anything without a source renders as `planned`.

## 2. Files you own
`README.md`, `docs/**` (except `docs/setup.md`), `demo/**`, `.github/**` (except `ci-a.yml`, `ci-b.yml`, `host-evaluation.yml`), `tools/**` (except `capture-evidence.py`, `env-check.py`, `generate_contracts.py`), `Makefile`, `progress/P.md`. Frozen after Phase 0 (do not change): `DECISIONS.md`, `AGENTS.md`, `CONTRIBUTING.md`, `OWNERSHIP.yaml`, `SCOPE.yaml`, `INTERFACES.md`, `contracts/**`. Branches start with `po/`.

## 3. Contracts you consume (from `main` only)
Evidence bundles (I9: `evidence/*/manifest.json`, `transcript.txt`), host evaluation (I10: the Linux `host-evaluation` artifact, `results.json`), telemetry traces (I6), demo entry points (I11: `host/guard-replay`, `dashboard/relay.py`, `image/run-demo.sh`, `eval/target_runner.py`), `SCOPE.yaml` via `tools/scope_gate.py`. If an input is missing, render `planned (<SCOPE id>)` and move on. Never edit A's or B's files to make your build work.

## 4. Milestones

| ID | Due (SGT) | Tier | Work | Acceptance check |
|---|---|---|---|---|
| P0 | Tue 17 Nov | MUST | Phase 0 merged (see `docs/streams/README.md` §2) | `python tools/check.py` GREEN on main; `python tools/scope_gate.py` prints the table |
| P1 | Sun 22 Nov | MUST | `tools/build_readme.py`: regenerates README sections between markers (`<!-- gen:evidence-map -->`, `<!-- gen:results -->`, `<!-- gen:quickstart -->`) from `evidence/`, the newest Linux eval artifact (`gh run download`) and the READMEs of `host/`, `dashboard/`, `image/`. `--check` fails if the README is stale. Nightly fresh-clone job green | `python tools/build_readme.py --check`; latest `nightly` run green |
| P2 | Sun 29 Nov | MUST | `demo/DEMO.md`: the 3-minute demo as numbered commands (host replay F3, dashboard, target `run-demo.sh` + `guardctl inject F3` + `guardctl recover`, a refusal with its reason); `tools/check_demo_commands.py` runs `--help` of every command in it; storyboard and shot list in `docs/video/` | `python tools/check_demo_commands.py` |
| P3 | Thu 10 Dec 18:00 | MUST | Code freeze: tag `v1.0-rc1` on the newest `main` commit where `python tools/check.py` and `bash tools/fresh_clone_demo.sh <sha>` both pass (if the newest fails, the newest `green-*` tag) | `git rev-parse v1.0-rc1`; fresh-clone output in `progress/P.md` |
| P4 | Sun 13 Dec | MUST | Video material ready (footage recorded from `v1.0-rc1` per `demo/DEMO.md`; captions file; under 3:00, no QNX logo); README links the video; `make package-check` passes | `make package-check` |
| P5 | Fri 4 Dec | SHOULD | `docs/architecture.svg` and a final Limits page | file exists; `claims-lint` passes |

`python tools/scope_gate.py --stream P` shows the live state. P's heavy work is Sat 5 - Sun 13 Dec; Mon 23 Nov - Fri 4 Dec are the PO's exams, so runs then are short (P1/P2 upkeep only).

## 5. Operating loop
1. **Orient:** `git fetch`; read `progress/P.md`; `python tools/scope_gate.py` (all streams: you only read A's and B's rows to know what will exist, never to chase anyone).
2. **Pick** the earliest-due OPEN item of yours.
3. **Research** (README examples of strong hackathon entries, video structure, judging criteria) and note it in `progress/P.md` "Research".
4. **Build** on `po/<id>-<slug>` from the newest green commit.
5. **Verify:** `python tools/check.py` GREEN plus the milestone check.
6. **Adversarial self-review** in a fresh context with [SELF_REVIEW.md](SELF_REVIEW.md) plus §6, fix findings.
7. **Merge yourself** when the required checks pass. Log in `progress/P.md`. Next.

## 6. Self-review checklist
- **Every claim has a source** on `main` (file path or artifact URL); no `TBD` left after P4; nothing invented (D11, A5).
- **Licence:** no QNX-measured numbers, no QNX-vs-Linux comparison, no QNX logo; on-target results shown as behaviour (logs, reason codes, video); numbers only from the host (Linux) harness, labelled so.
- **Limits** stated: simulation-only research prototype, not a safety product (wording in README "Limits").
- **Fresh clone:** a stranger can run the host demo with the README commands (`tools/fresh_clone_demo.sh`).
- **Judging fit:** the first screen of the README says what GUARD is, why QNX, and shows the demo GIF/video; AI-use disclosure present.
- **Links:** every relative link and evidence folder resolves (`tools/check_readme_links.py`).

## 7. Ambiguity, blocker and fallback rules
- **Ambiguity:** simplest option consistent with `docs/architecture.md`, Limits and the judging criteria; log in `progress/P.md` "Decisions"; continue.
- **Missing upstream input:** never wait. Render `planned`. Pre-planned fallbacks by date: if `scope_gate.py` shows B4/B5 (F0/F1/F3 on target) not DONE at Mon 7 Dec 23:59, the demo and video use host replay + dashboard + `library on target` evidence rows, and `demo/DEMO.md` switches to its "host" variant automatically (`tools/build_readme.py --fallback auto`).
- **Blocked:** three approaches, then a simpler format (for example a slide-style video from dashboard captures), log, continue.

## 8. Definition of done
`v1.0-rc1` tagged; `make package-check` passes; README has no `TBD`, links resolve, Results cite the Linux artifact; video under 3:00 linked in the README; `progress/P.md` records every check's output. No repo change after **Tue 15 Dec 18:00 SGT**.

## 9. After done
Polish README wording and images; extra demo variants (`demo/DEMO-host.md`); P2 backlog (#51) notes for after the deadline.

**Do not ask the human questions or request review. Make a decision, log it, and continue.**
