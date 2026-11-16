# Adversarial self-review (all streams)

Every stream runs this before each merge, in a **fresh context** (a sub-agent or a new session that did not write the change). It replaces peer review (D26). Feed it `git diff origin/main...HEAD`, the milestone's acceptance output, and the stream-specific list in section 6 of your brief ([A](BRIEF-A.md), [B](BRIEF-B.md), [P](BRIEF-P.md)).

## How to run it
1. Start the reviewer with: "You did not write this change. Find what is wrong with it. Report each finding as `[blocker]`, `[should]` or `[nit]`, with `path:line`, the problem and the fix."
2. Give it this file, section 6 of your brief, the diff and the command output. Nothing else.
3. Fix every `[blocker]` and `[should]`. Re-run the milestone check and `python tools/check.py`.
4. Log skipped `[nit]`s under "Known issues" in `progress/<stream>.md`. Log the review result next to the milestone, for example: `self-review: 0 blocker, 0 should, 2 nits skipped`.
5. If a blocker survives three genuinely different fixes, apply the blocker rule in section 7 of your brief: fall back, descope one tier, log it, continue.

## Shared checklist
- **Correctness:** the change does what the milestone says. Its acceptance command passes, and the output is pasted into `progress/<stream>.md`.
- **Contracts:** nothing under `contracts/` changed, except your own `contracts/ext/` files. Sizes, field order, enums, reason-code numbers and C8 records match `contracts-v1.2`. `python tools/check_contract_compat.py --base contracts-v1.2` passes.
- **Ownership:** every changed path is yours (`python tools/check_ownership.py --base origin/main --local`).
- **Edge cases:** NaN/Inf, overflow, zero or negative durations, sequence wrap, empty, duplicate or oversized input, timeouts, and a refusal that must change nothing.
- **Tests:** each fix has a test that failed before it. Tests assert behaviour, not just that the code runs. No test is skipped or weakened to get green.
- **Determinism:** the same inputs and seed give byte-identical output.
- **Security:** no secrets, licence data, home paths or ssh fingerprints in commits or evidence. Network listeners bind to `127.0.0.1` and validate every input.
- **Claims (D11):** no QNX-measured numbers and no QNX-vs-Linux comparison anywhere. Public numbers come only from the host (Linux) harness, labelled so. `claims-lint` passes.
- **Demo-readiness:** the commands in your READMEs still work from a fresh clone (`bash tools/fresh_clone_demo.sh main`).
- **Judging fit:** the change makes the demo clearer or the evidence stronger. It adds no work a judge cannot see or verify.
- **Done means executable:** the `SCOPE.yaml` check for the item passes by itself. A `progress/<stream>.md` line alone never makes an item DONE.
