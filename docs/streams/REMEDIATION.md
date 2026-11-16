# Phase-0 audit remediation

The zero-handoff harness (D26) was audited before P0 closed. Eight defects were
found: each one could make a stream believe it was green, or let the plan drift
from what the gate measures. All eight are fixed on `main`, each in its own PR,
each with a test that failed before the fix.

| # | Defect | Fix | PR |
|---|---|---|---|
| 1 | `SCOPE.yaml` `cmd` checks were shell strings, so `-k 'not slow'` split on Windows and A1 was false-OPEN | `scope_gate.argv` splits with POSIX rules, runs without a shell; test asserts the quoted argument stays one | [#165](https://github.com/Tauwho/GUARD/pull/165) |
| 2 | CI did not run on `merge_group`, and a red `main` produced no signal | merge-queue trigger added; nightly opens (and refreshes) one red-main issue | [#167](https://github.com/Tauwho/GUARD/pull/167) |
| 3 | A `progress/<stream>.md` grep counted as a done-check, so writing "done" made an item DONE | checks must be executable (file/grep/cmd); `evidence: manual` is reported MANUAL and never DONE; no progress-file greps | [#169](https://github.com/Tauwho/GUARD/pull/169) |
| 4 | Milestone tables in the briefs had drifted from `SCOPE.yaml` (B0, P0, A8, A9, P4 date) | `tools/brief_sync.py` generates the tables; `brief-sync` stage fails `make check` on drift | [#170](https://github.com/Tauwho/GUARD/pull/170) |
| 5 | No shared self-review and no link check, so dead docs shipped | `docs/streams/SELF_REVIEW.md` + `tools/check_doc_links.py` in `make check` | [#166](https://github.com/Tauwho/GUARD/pull/166) |
| 6 | While P was away, A/B hits on P's tooling had no legal path and would silently rot | D27 `tooling-fix:` PR title: A/B may touch non-frozen P tooling; gate files excluded; CI evaluates the title and runs the base branch's rules | [#168](https://github.com/Tauwho/GUARD/pull/168) |
| 7 | P6 (Devpost draft) had no scope item; the B-F0 rule ("after CP2, no SHOULD work without F0 on target") existed only in prose | P6 item + `tools/check_submission.py`; `scope_gate.b4_early_cut` drops SHOULD items after CP2 when B4 is not DONE | [#171](https://github.com/Tauwho/GUARD/pull/171) |
| 8 | A machine without `clang-format` went red locally, so "green" meant different things in and out of CI | the format stage reports SKIPPED locally when the tool is absent and still fails in CI | [#164](https://github.com/Tauwho/GUARD/pull/164) |

## Verification

```
$ PR_LABELS=phase0-override python tools/check.py --fast
PASS policy, contract-compat, ownership, doc-links, brief-sync, ruff, pytest
RESULT: GREEN
```

The authoritative run is CI on `main`: every required job, including
`make-check`, is green on the P0 commit. The nightly job runs the same
`tools/check.py`, then `tools/fresh_clone_demo.sh` on that commit, and tags the
commit `green-YYYYMMDD`; `progress/P.md` records the run link and tag.

## What the audit changed in practice

- "Done" now means a command exits 0 against this checkout, never that a
  paragraph says so.
- The briefs cannot drift from `SCOPE.yaml`; a drift is a red stage.
- An agent that stays on SHOULD work after CP2 sees DROPPED rows and stops.
- The freeze was opened exactly eight times, each PR labelled
  `phase0-override` and reviewed by the audit itself. [#171](https://github.com/Tauwho/GUARD/pull/171) is the last
  one; after Phase 0 no PR carries that label.

## Residual risks

- **Stream B has not started.** B0 (target check script) is OPEN, so every
  B-row downstream is OPEN and B1's evidence correctly does not cover the core
  yet. The plan does not wait: P renders `planned (<id>)` for missing upstream
  inputs, and from 7 Dec the demo falls back to the host variant.
- **No QNX in CI.** Licence terms forbid it. Target evidence exists only as
  bundles produced on B's and P's machines; CI checks their schema and SHAs.
- **Two P files invented before the freeze** (`tools/check_submission.py`,
  `docs/devpost/`) are checked by P6; the Devpost upload itself stays human.
