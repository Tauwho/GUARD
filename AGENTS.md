<!-- managed-by: guard-bootstrap v1 -->
# AGENTS.md — rules for humans and AI agents working on GUARD

This file is binding for every contributor and every AI agent acting for one. If anything conflicts with it, stop and ask in the issue.

## 1. Hard rules (never break these)
1. **Never commit QNX SDP files, images, licences or credentials.** This includes headers, libraries, `.ifs`/`.qcow2`/`.vmdk`/`.img`, licence keys and `qnxsdp-env.*`. Ship scripts and instructions instead.
2. **Keep the portable core free of QNX-only calls, and keep the QNX layer free of business logic.**
   - `core/` contains no `sys/neutrino.h`, `sys/iofunc.h`, `sys/dispatch.h`, `Msg*`, `resmgr_*`, `slog2*`, clock reads or file I/O. Time is always passed in.
   - `qnx/` only wires IPC, timers, resource managers and privileges to `core/`.
3. **Keep all safety claims within the stated limits** (README "Limits"). Never write: "is safe", "guarantee(s)", "certified", "compliant", "prevents collisions", "hard real-time", "faster than Linux". The only exception is negated text placed after a `<!-- claims-ok: negated -->` marker. The marker exempts everything from that line to the next Markdown heading.
4. **Every change that affects the QNX path must attach target evidence.** The QNX path is `core/`, `qnx/`, `image/`, `cmake/` and `contracts/`. Follow `evidence/README.md`.

## 2. Licence constraints (QNX non-commercial licence)
- **Personal licences.** Each person uses their own free licence, on at most 2 machines they own or control. Never share keys.
- **No QNX in CI.** The QNX SDP never runs in GitHub Actions or any other cloud CI. CI is host-only.
- **No published performance results.** Do not publish performance or functional evaluation results *of QNX itself* (latency, jitter, QNX-vs-Linux) unless DECISIONS.md records BlackBerry's written approval. Functional results of GUARD (violations, reason codes) are allowed.
- **Simulation only.** Never connect GUARD to real actuators.

## 3. Engineering rules
- **Read order before any work:** this file → the issue → linked contracts and ADRs → the code.
- **Scope.** Touch only the paths listed in your issue's Deliverables, plus their tests. Anything else needs a new issue.
- **Contracts.** Never guess a wire format. Ask in the contract issue with a comment starting `CONTRACT-Q:`. After v1.0, changes need the `contract-change` label, an ADR, and approval from both @phatdat220707 and @baofrovn.
- **Dependencies.** No new dependency without an ADR. Allowed now:
  - C++: doctest
  - Python: stdlib, pytest, hypothesis, websockets, PyYAML, jsonschema, ruff
- **Determinism.** Core logic is a pure function of its inputs plus injected time. Per-tick paths use fixed-capacity containers.
- **QNX layer:**
  - priorities ≤ 63, following the priority map in `docs/architecture.md`;
  - every blocking IPC bounded by `TimerTimeout`;
  - the critical path never blocks on telemetry;
  - privileges dropped and abilities locked after init.
- **Evidence and binaries.** No binaries in git. Evidence is text only, ≤ 1 MiB per bundle. The one exception is the Devpost confirmation PNG under `evidence/submission/`.
- **$0 budget.** Free tiers and open-source only.

## 4. Communication protocol
All communication happens in GitHub issues and PRs. Decisions made elsewhere must be written into the issue within 24 h.

| When | Do this |
|---|---|
| Starting an issue | Assign yourself. Set board Status to In progress. Comment `START:` with a plan of ≤ 5 bullets and an ETA |
| Daily, while active (work longer than 1 day) | Comment `UPDATE:` with done / next / risks |
| Blocked | Add `status:blocked`. Comment `BLOCKED: <what> · needs <who/what> · by <date>` and mention the owner. After 24 h, mention @dngvmnh |
| Question about an interface | Comment on the contract issue starting `CONTRACT-Q:`. Do not proceed on a guess |
| Need something from another stream | Open a new issue with the work-package template, assign the owning stream, and link both ways. Never edit another stream's paths |
| A decision is needed | Open a `decision` issue (ADR proposal). The PO accepts or rejects; the result goes to `docs/adr/` and `DECISIONS.md` |
| PR opened | Title `<type>(<area>): <summary> (#issue)`. Fill in the template. Request CODEOWNER review. Add `status:needs-review` |
| Review received | Respond within 24 h. Resolve every conversation before merging |
| End of every agent session | Comment `HANDOFF:` with done / next / open risks / evidence paths |
| Freeze active (see `docs/architecture.md` REF-FREEZES) | Only fixes and docs. Anything else needs `freeze-exception` and PO approval |

Never put secrets, licence keys or personal data in comments, logs or screenshots.

## 5. Agent identity and disclosure
- Agents act through their human's account. That human is accountable for every change merged.
- Tick "AI-assisted" in the PR template whenever an agent wrote any part of the change.
- Commit trailers for AI tools are optional and not required.

## 6. When unsure
Stop. Ask in the issue. A wrong guess costs more than a 2-hour wait.
