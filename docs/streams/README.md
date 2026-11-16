# GUARD zero-handoff redesign (16 Nov 2026)

This replaces the PO-in-the-loop operating model (handoff prompts v4-v9, autopilot, CONTRACT-Q, PO reviews, nudges, board sync, CK gates) with three autonomous streams:

```
A (Đạt, host vertical)   ──merge when green──►  main  ──consumed by──►  P (PO, package vertical)
B (Bảo, target vertical) ──merge when green──►  main  ──consumed by──►  P
B also consumes A's core from main (one-way, pinned to the last green commit).
Nobody waits on P. P approves nothing.
```

Files in this folder:

| File | What |
|---|---|
| `README.md` (this) | Investigation, audit, Phase 0, harness, integration plan, risks |
| `phase0/` | Files P copies into the repo in Phase 0 (contracts of the process, checks, scope) |
| `BRIEF-A.md`, `BRIEF-B.md`, `BRIEF-P.md` | First message for each stream's agent. Self-contained |

---

## 1. Investigation

### 1a. Rules, tools, limits (what can block a stream)

The hackathon site blocks automated fetches (Cloudflare 403 on 15 Nov), so rules below are from the 15 Nov research and the QNX licence text, not re-fetched today.

| Item | Fact | Blocks whom | Design answer |
|---|---|---|---|
| Deadline | 15 Dec 14:00 EST = **Wed 16 Dec 03:00 SGT**. Internal hard stop Tue 15 Dec 18:00 SGT | all | Code freeze Thu 10 Dec 18:00, package done Sun 13 Dec |
| QNX SDP licence (non-commercial) | Per person, max 2 machines, no key sharing; **no SDP in cloud CI**; no redistribution of SDP files or images; §4.6(i) **no publishing QNX performance or functional evaluation results** without written approval; no high-risk use (simulation only) | B, P | Target work runs only on B's and P's own machines. CI is host-only. Public numbers only from the host (Linux) harness (D11). Evidence bundles hold pass/fail transcripts and test counts, never timings |
| QEMU + KVM on WSL2 | Works on B's and P's hosts; no sudo needed with user-mode networking (Option C) | B, P | `image/run-demo.sh` (on main) boots without sudo |
| A has no QNX setup (#15 never started) | A cannot build or run QNX binaries | A | **A never needs a target.** All target evidence belongs to B (vertical) |
| GitHub | Public repo, Actions minutes free; branch protection; teammates' tokens lack `project` scope | all | Drop the project board as a coordination tool; progress lives in files |
| Ollama + Qwen2.5-1.5B-Instruct (D23, Apache-2.0) | Local CPU only, ~1 GB pull, slow on laptops | A (P1) | Recorded-response mode is the default demo path; live model is optional |
| Python deps | PyYAML 6.0.3, jsonschema 4.26.0, websockets 17.3, pytest 9.0.2, ruff 0.14.6 (pinned) | all | Pinned in CI and `tools/check.py` |
| Budget | $0 | all | No paid APIs, no keys to provision |

No stream needs an API key, an approval or a paid account. The only external gate is the QNX licence, which is per person and already in place for B and P.

### 1b. Audit: every point where someone waits on someone

| # | Current handoff / wait | Who waits on whom | Eliminated by |
|---|---|---|---|
| 1 | PO comment reviews on every teammate PR (`po-review v1`), blockers to fix | A, B ← P | **Automated verification**: `make check` + required CI; each stream's adversarial self-review sub-agent. P stops reviewing |
| 2 | `CONTRACT-Q:` on #3 / #125, waiting for `PO-DECISION:` | A, B ← P | **Contracts frozen in Phase 0** (v1.2 tagged now). Changes are additive-only in stream-owned `contracts/ext/` files, checked by `check_contract_compat.py`; otherwise an adapter on the stream's own side. Nobody asks |
| 3 | v1.2 freeze waits for review window / "no more CONTRACT-Q" | P ← A, B | **P freezes v1.2 in Phase 0** without waiting |
| 4 | v1.3 `RouteProposal` (D24): A drafts layout, B confirms, PO accepts | A ↔ B ↔ P | **Scope cut**: no new wire type. `guard-planner` checks the route with A's pure `check_route` and turns waypoints into normal C1 `Proposal`s; checkers 1-11 still gate every motion. Route admission in the supervisor is dropped |
| 5 | B's processes #38-#42 wait for A's core (#18/#20/#21) on main | B ← A | **Phase 0 lands the core** (A's consolidation, or P lands it once). After that B consumes `main`; core public headers are a frozen interface (additive-only) |
| 6 | D18/D25: core PRs "finished" only with target evidence, captured by the PO | A ← P | **Vertical ownership**: B owns all target evidence, including `core-on-target`, captured from `main` as part of B's own milestones. A's PRs merge on host checks only |
| 7 | A's setup evidence #15 / M0 gate #12 / D19 follow-ups | P ← A | **Scope cut**: A never needs a target, so A's setup is dropped |
| 8 | M0/CK-M3/CK-M4 gates: PO decides keep/cut, go/no-go | A, B ← P | **Pre-planned cut lines** in `SCOPE.yaml`; `tools/scope_gate.py` marks items DROPPED by date and done-checks. Agents self-descope |
| 9 | PO unblock comments, nudges, "post START/BLOCKED" | A, B ← P | **Blocker rule** (3 approaches, then mock/descope) + `progress/<stream>.md`. No pings |
| 10 | Board sync by PO (teammates lack project scope) | P ← A, B | **Dropped**: `progress/*.md` + `scope_gate.py` report replace the board |
| 11 | AGENTS.md rule 5 "ask in issues with @-mention" | everyone | **Replaced** by "decide, log in your own `docs/decisions/<stream>.md`, continue" |
| 12 | Shared `DECISIONS.md` edited by all (merge conflicts, D-number races) | A ↔ B ↔ P | **Strict ownership**: `DECISIONS.md` is P's and frozen after Phase 0; A and B log in their own files |
| 13 | F5/F6 expectation mismatches need PO decisions (scenario YAML is PO's) | A ← P | **Vertical**: A owns `scenarios/` expectations and the planner, constrained by an automated invariant check (safety `forbidden_reasons` may never be removed) |
| 14 | `host/prepare_inputs.py` (#140, A) feeds B's image packaging (#141) | B ← A | **Contract + fixture**: the normalized-inputs format is frozen in `INTERFACES.md`; the tool is on main; B consumes it from main |
| 15 | Dashboard live view needs B's bridge; target traces | A ← B | **Contract + mock**: C8 schema + fixture JSONL; relay `--trace` (mock) vs `--bridge` (real) is a flag |
| 16 | Remote planner #49 (B) needs A's advisor on TCP 7701 | B ← A | **Contract + mock**: C9 schema + `host/llm_advisor.py --recorded` on main is the mock server. #49 is STRETCH |
| 17 | Target scenario runner (#44) waits for `guardctl` (#42); target F0-F8 runs (#45) need B's chain + A's #20/#25 | B ← B, A | Inside B's vertical after core is on main. Host side of the runner moves to A against the `guardctl` contract (C14) with a local mock target |
| 18 | README Evidence Map, Results, video need A's eval and B's evidence | P ← A, B | **One-way consumption**: P generates README sections from files on main (`evidence/`, eval artifact). Missing inputs render as `planned`, never block |
| 19 | Weekly snapshots, triage, PO to-do lists | P ← A, B | Replaced by `scope_gate.py` + `progress/*.md`, read by P's agent |
| 20 | CI workflow edits by A (shared `ci.yml`) | A ↔ P | **Ownership**: `ci.yml` (required checks) is P's and frozen; streams add jobs in `ci-a.yml` / `ci-b.yml` |

**Not removable (flagged):**
- **QNX target runs need a licensed person's own machine.** The licence forbids SDP in cloud CI, so target checks cannot run in GitHub Actions. They run on B's machine (B's agent drives them) and on P's for footage. This is a machine dependency inside one stream, not a handoff, but it means B's laptop must be on while B's agent works.
- **Phase 0 itself** (admin settings, branch protection, freezing contracts) needs P as repo admin. Once.
- **Recording and uploading the video** needs P's hands. It sits inside P's stream and depends only on `main`.

### 1c. Questions

None. Assumptions instead:
1. A and B accept the briefs as their full instructions, and their agents can run for hours with `gh` push rights.
2. A's core is on main (#71 merged 16 Nov, `0215b4a`), so B consumes it from main from now on.
3. P's exams (Mon 23 Nov - Fri 4 Dec) reduce P to a few short agent runs; nothing upstream depends on P, so A and B are unaffected.
4. The project board, issue comments and @-mentions stop being part of the workflow. Issues stay as a backlog index only.
5. Previous decisions D1-D25 stay valid except where this design overrides them (recorded as D26 in Phase 0).

---

## 2. Phase 0 checklist for P (do all of it now; target: done by Tue 17 Nov 23:59 SGT)

Everything below is one PO PR (`po/phase0-zero-handoff`) plus settings and two tags. After it merges, nothing waits on P.

| # | Do | Command / file | Done when |
|---|---|---|---|
| 0.1 | Freeze contracts v1.2 now | Merge `origin/main` into `po/contracts-v1-2`, mark ADR 0018 Accepted, CHANGELOG "frozen", `gh pr ready 127`, merge, `git tag contracts-v1.2 <sha> && git push origin contracts-v1.2`; close #125 | Tag exists |
| 0.2 | Close the v1.3 path | D26 row (below) cuts `RouteProposal`; close #3 discussion as settled | D26 on main |
| 0.3 | Core on main | **Done 16 Nov 17:49 SGT**: A merged #71 (`0215b4a`, whole stack). Nothing to do | `core/src/safety/supervisor.cpp` on main |
| 0.4 | Tag a green baseline | `git tag green-phase0 <main sha after 0.1-0.3>`; push | Tag exists |
| 0.5 | Copy `phase0/` files into the repo (the PR needs label `phase0-override`, because these paths are frozen afterwards) | `OWNERSHIP.yaml`, `SCOPE.yaml`, `INTERFACES.md`, `ENV.md`, `contracts/ext/README.md`, `contracts/ext/CHANGES-A.md`, `CHANGES-B.md`, `progress/A.md`, `B.md`, `P.md`, `docs/decisions/A.md`, `B.md`, `tools/check.py`, `tools/check_ownership.py`, `tools/check_contract_compat.py`, `tools/scope_gate.py`, `tools/fresh_clone_demo.sh`, `Makefile`, `.github/workflows/ci-ownership.yml`, `.github/workflows/nightly.yml`, AGENTS.md section | Files on main |
| 0.6 | D26 in `DECISIONS.md` | Text in `phase0/DECISIONS-D26.md` | Row on main |
| 0.7 | Required checks | Add `ownership` and `contract-compat` to the 11 required checks (`PUT /repos/Tauwho/GUARD/branches/main/protection`, keep everything else) | 13 required checks |
| 0.8 | Update CODEOWNERS | Replace with comment-only ownership note pointing to `OWNERSHIP.yaml` (no review requests) | No auto review requests |
| 0.9 | Retire the old machinery | Disable the Kiro Crew GUARD cron jobs; mark handoff prompts v4-v9 and the autopilot superseded; stop board sync | No scheduled PO runs |
| 0.10 | Close coordination issues | Close #12, #13, #14 ("replaced by SCOPE.yaml cut lines, D26"), #15 ("not needed, D26"); leave feature issues open as backlog index | Closed |
| 0.11 | Verify | `python tools/check.py` on main passes; `bash tools/fresh_clone_demo.sh` passes; `python tools/scope_gate.py` prints the table | Command output saved in `progress/P.md` |
| 0.12 | Hand out briefs | Send `BRIEF-A.md` to Đạt and `BRIEF-B.md` to Bảo **once, as the first message for their agents** (paste link to the file on main: `docs/streams/BRIEF-A.md`, `BRIEF-B.md`) | Both have it |

Accounts, keys, env: nothing new. `ENV.md` lists every variable; none is secret.

---

## 3. Contract files

Actual content is in `phase0/`:

| File | Purpose |
|---|---|
| `phase0/OWNERSHIP.yaml` | Path globs per stream; enforced by `tools/check_ownership.py` in CI |
| `phase0/INTERFACES.md` | Every interface between streams, its frozen spec, its mock, and the flag that swaps mock for real |
| `phase0/ENV.md` | Every environment variable and default |
| `phase0/SCOPE.yaml` | MUST/SHOULD/STRETCH items, owner, executable done-check, cut date |
| `phase0/contracts-ext-README.md` | Additive-extension rule (goes to `contracts/ext/README.md`) |
| `phase0/DECISIONS-D26.md` | The D26 row |
| `phase0/AGENTS-zero-handoff.md` | Section replacing AGENTS.md rule 5 |
| `phase0/tools/*.py`, `fresh_clone_demo.sh`, `Makefile`, workflows | The verification harness |

The message/telemetry/scenario contracts themselves already exist on main (`contracts/`, C1-C14, tag `contracts-v1.2` after 0.1) and are not rewritten.

---

## 4. Stream briefs

`BRIEF-A.md`, `BRIEF-B.md`, `BRIEF-P.md`. Copy them to `docs/streams/` in Phase 0 so agents can reread them from the repo.

Balance (remaining effort estimate, hours):

| Stream | MUST | SHOULD | Total | Notes |
|---|---|---|---|---|
| A | 26 | 14 | 40 | Gains the host side of the target runner, input prep and scenario ownership |
| B | 38 | 10 | 48 | Loses #49 C1 forwarding (cut) and the host runner side; target work is heavier per item |
| P | 22 | 8 | 30 | Plus exams 23 Nov - 4 Dec; P's heavy work (README, video) is 5-13 Dec, after A and B have produced inputs |

---

## 5. Verification harness

`make check` = `python tools/check.py` (same on every machine; CI runs the same commands):

| Stage | Command | Owner of the code |
|---|---|---|
| format | `python tools/check_format.py` | P (existing) |
| policy | `python tools/check_policy.py all` (core-portable, no-qnx-files, claims-lint, schemas, evidence) | P (existing) |
| contract compat | `python tools/check_contract_compat.py --base contracts-v1.2` | P |
| ownership | `python tools/check_ownership.py --base origin/main` (CI; local run uses the current branch prefix) | P |
| brief sync | `python tools/check_brief_sync.py` (brief milestone ids, tiers and dates match `SCOPE.yaml`; `tools/sync_briefs.py` rewrites the Due/Tier cells) | P |
| host build + tests | `cmake --preset host-debug && cmake --build --preset host-debug && ctest --preset host-debug` | A |
| Python | `python -m ruff check . && python -m pytest` | A |
| scope report | `python tools/scope_gate.py` (report only, never fails the build) | P |

**Contract tests** (run inside pytest/ctest, so every PR runs them):
- Binary message fixtures (`contracts/fixtures/messages/*.json`) encode byte-exact through A's codecs (existing generated tests).
- Every JSON a stream produces validates against `contracts/*.schema.json`: host telemetry (A's `eval/tests`), target telemetry (B's evidence check validates committed C8 traces), scenario/config files.
- `check_contract_compat.py` fails if any change to `contracts/` removes or renames a field, adds a required field, removes an enum value, renumbers a reason code or changes a fixture's byte size compared with tag `contracts-v1.2`.
- Mock/real parity: `eval/tests/test_target_runner.py` (A) runs the runner in `--target local` mode; B's `image/target-check.sh` runs the same runner in `--target ssh:guard-target` mode and commits the trace; the `evidence` check validates it with the same schema.

**Target checks (not in CI, licence):** `image/target-check.sh` (B) builds `qnx-x86_64`, boots the image, runs `guard_core_tests` and the scenario smoke, and writes evidence bundles with `tools/capture-evidence.py`. CI validates the committed bundles (schema, size, SHA rule D21).

**Fresh-clone demo test:** `tools/fresh_clone_demo.sh` (P) clones `main` into a temp dir and runs the README host quick start: build, `host/guard-replay` on F0 and F3, `eval/run_matrix.py --seeds 1 --modes B2` (records mismatches, does not fail on them), and a headless relay smoke (`dashboard/relay.py --trace`, fetch `/meta.json`). Runs nightly on main (`nightly.yml`) and before every tag.

**Green tags:** `nightly.yml` tags the newest main commit whose checks passed as `green-<yyyymmdd>`. Streams base work on the newest green tag, so a red main never blocks anyone.

---

## 6. Integration and submission plan

| When (SGT) | Automated checkpoint (`scope_gate.py` evaluates done-checks) | A | B | P |
|---|---|---|---|---|
| Tue 17 Nov 23:59 | **Phase 0 done** | core on main | branch process work from main | Phase 0 merged, briefs out |
| Sun 22 Nov 23:59 | **CP1** | A1-A3 done (blockers fixed, F5/F6, runner local mode) | B1-B2 (core-on-target from main, `guard-gate` on target) | P1 (README generator + fresh-clone nightly green) |
| Sun 29 Nov 23:59 | **CP2** | A4-A5 (Linux matrix artifact, dashboard final) | B3-B4 (sim, supervisor, planner; F0 on target) | P2 (demo script, storyboard commands from main) |
| Fri 4 Dec 23:59 | **Cut line 1**: SHOULD items with no commit on their branch are DROPPED | SHOULD in flight only | F1, F3 on target; guardctl + bridge | — |
| Mon 7 Dec 23:59 | **Cut line 2**: all STRETCH DROPPED; if MUST target evidence (F0/F1/F3) is missing, demo falls back to host replay + `library on target` evidence automatically | fixes | secpol (MUST-lite) | README generator picks the fallback |
| Wed 9 Dec 23:59 | **Feature freeze**: only `a/fix-*`, `b/fix-*`, `po/fix-*` branches merge | fixes | fixes | rehearsal |
| Thu 10 Dec 18:00 | **Code freeze**: P's agent tags `v1.0-rc1` on the newest green main commit that also passes `fresh_clone_demo.sh` | — | — | tag |
| Fri 11 - Sun 13 Dec | Package | — | — | footage on `v1.0-rc1`, video, final README, `make package-check` |
| Tue 15 Dec 18:00 | Hard stop | — | — | — |

**Glue and demo owner:** P. P owns `demo/` (demo script, recorded demo trace pointers), README, docs, video, release tags, and the final package check. A owns host demo commands; B owns `image/run-demo.sh`. P only consumes them.

**Final package checklist (`make package-check`, P):**
1. `python tools/check.py` passes on the tagged commit.
2. `bash tools/fresh_clone_demo.sh v1.0-rc1` passes.
3. README: no `TBD`; every relative link and evidence folder exists (`tools/check_readme_links.py`, P); Results cite the Linux eval artifact; Limits section present; AI-use disclosure present.
4. `claims-lint` and `no-qnx-files` pass; no timing numbers in any evidence bundle.
5. Video file is under 3:00 (`ffprobe` duration) and its link is in the README.
6. `LICENSE` present; no SDP files; repo public.

---

## 7. Risk register

| # | Risk | Trigger (automated) | Pre-planned fallback (no human decision) |
|---|---|---|---|
| 1 | Core on main breaks on QNX (landed `0215b4a` 16 Nov; the pre-landing head `ad28b00` passed 85/85 on target) | B1 `core-on-target` fails | B works around it in `qnx/` and logs it; A sees the failing bundle transcript on `main` and fixes in `core/` (A reads main, nobody messages). If unfixable, B pins its process builds to tag `core-snap-1` (`ad28b00`) |
| 2 | B's target chain late (most hours, one machine) | CP2: F0 target evidence missing | Cut line 1 drops B's SHOULD items; cut line 2 switches the demo to host replay + dashboard + `library on target` evidence; B's MUST shrinks to `guard-gate` + F3 latch on target |
| 3 | `main` goes red | nightly or PR checks fail on main | Nobody waits: streams base on the newest `green-*` tag. The owner of the failing path fixes first (ownership map names them); P's tag step only ever tags green commits |
| 4 | A stream needs a contract change | Compat check fails on a PR | Additive extension in `contracts/ext/<stream>-*`; or an adapter on the stream's own side. Breaking changes are impossible to merge |
| 5 | Licence breach (QNX-measured numbers published) | `claims-lint`, evidence check (no timing fields), README generator reads only the Linux host artifact | CI blocks the merge; README sections without allowed sources render as `planned` |

Also covered: LLM advisor too slow or poor (recorded-response mode is the demo default; live model is STRETCH); P unavailable during exams (P has no upstream consumers; P's MUST work is scheduled 5-13 Dec).
