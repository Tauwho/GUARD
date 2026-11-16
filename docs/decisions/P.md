# Stream P decisions (@dngvmnh)

Decide, log one line, continue (D26). Never edit `DECISIONS.md`.

| Date | Decision | Reason |
|---|---|---|
| 2026-11-16 | Phase 0 is P's first deliverable; the audit fixes ship as eight numbered PRs before any package work starts | P1-P6 all read the harness; fixing it later would invalidate their evidence. |
| 2026-11-16 | The freeze is opened only by those audit PRs, each labelled `phase0-override`; #171 is the last one | The label is how the base-branch gate knows the intent; keeping it longer would turn the freeze into a suggestion. |
| 2026-11-16 | A scope check must be an executable command against this checkout; `evidence: manual` shows MANUAL and is never DONE | Audit 3: a written "done" is not evidence, and inventing numbers for manual work would break D11. |
| 2026-11-16 | After CP2, if B4 (F0 on target) is not DONE, SHOULD items are DROPPED; DONE and MANUAL keep their state | Audit 7: the demo's backbone slipping ends optional work; finished or human-evidenced work is not "dropped", it is just reported. |
| 2026-11-16 | Brief milestone tables are generated from `SCOPE.yaml` and a drift fails `make check` | Audit 4: two sources of truth for ids, tiers and dates had already drifted (B0, P0, A8, A9, P4). |
| 2026-11-16 | A/B may repair non-frozen P tooling with a `tooling-fix:` PR title; the gate files themselves are excluded (D27) | P is away during exams; silent rot in the harness is worse than a narrow, logged exception. |
| 2026-11-16 | P6 ends at a complete draft; the Devpost upload stays human (PO), and the description says so | The submission is tied to a named account and final video link; a bot cannot own it. |
| 2026-11-16 | Every public number in P's docs comes from the Linux host harness and is labelled; QNX results appear only as behaviour (logs, reason codes, video) | QNX licence §4.6(i) and D11: no QNX-measured numbers, no QNX-vs-Linux comparison. |
| 2026-11-16 | `make package-check` calls `python tools/check_package.py`, so the package gate is the same definition of green as `make check` | One place to change when stages move; P4's scope check runs the same command. |
| 2026-11-16 | Missing upstream input renders `planned (<SCOPE id>)`, never blank and never a placeholder number | P never waits; a judge can trace every gap to an open milestone. |
