<!-- managed-by: guard-bootstrap v1 -->
# contracts/

Interfaces that both streams build against. **Owner:** @dngvmnh (PO); @phatdat220707 (A) and @baofrovn (B) are @-mentioned on every change; no approval is required (D12).

- **Freeze:** v1.0 on Fri 20 Nov 2026 23:59 SGT, tagged `contracts-v1.0` (issue PO-2).
- **After the freeze:** follow the contract-change process in CONTRIBUTING.md (ADR, plus an @-mention of both A and B).

## Expected files

- `messages.md` — C1–C7 wire messages
- `reason_codes.yaml` — C10
- `telemetry.schema.json` (C8), `planner-remote.schema.json` (C9, P1), `scenario.schema.json` (C11), `config.schema.json` (C12), `evidence-manifest.schema.json` (C13)
- `guardctl.md` — C14
- `fixtures/` — at least one valid and one invalid example per message and schema
- `CHANGELOG.md`

## Contract table

| ID | Contract | Endpoint | Mechanism | Bound |
|---|---|---|---|---|
| C1 | Proposal → Decision | planner → `guard/supervisor` (`name_attach`) | `MsgSend`/`MsgReceive`/`MsgReply`; client `TimerTimeout` 50 ms | 1 in flight per planner; ≤ 50/s |
| C2 | Register / lease / revoke | supervisor → `/dev/guard/gate` | `devctl`: `DCMD_GUARD_REGISTER` (`__DIOF`), `DCMD_GUARD_LEASE` (`__DIOTF`), `DCMD_GUARD_REVOKE` (`__DIOT`) | Client `TimerTimeout` 5 ms |
| C3 | Operator | `guardctl` → `/dev/guard/gate` | `devctl` `DCMD_GUARD_RECOVER` (`__DIOTF`) | op uid or root |
| C4 | Status | any reader → `/dev/guard/status` | `read()`: `key=value` text, one snapshot per `open` | ≤ 2 KiB |
| C5 | Motor | gate → `/dev/sim/motor` | `devctl` `DCMD_SIM_MOTOR` (`__DIOT`); `TimerTimeout` 5 ms | 100 Hz; gate uid only |
| C6 | Observations | sim → `/dev/sim/obs/{supervisor,planner}` readers | Blocking `read()`; sim replies later with `MsgReply` on the saved `rcvid` | 20 Hz |
| C7 | World control | eval / `guardctl` → `/dev/sim/world` | `devctl`: load scenario, inject fault, pause; `read` for status | Test image only |
| C8 | Telemetry | bridge → host TCP 7700 | JSONL per `telemetry.schema.json`; the harness emits the same schema | Best effort; drops counted |
| C9 | Remote planner (P1) | planner ↔ host TCP 7701 | JSONL request `{obs}`, response `{proposal or route}` | Host may be slow |
| C10 | Reason codes | `reason_codes.yaml` | Generates the header and the docs | Never renumbered |
| C11 | Scenario | `scenario.schema.json` | YAML | — |
| C12 | Config | `config.schema.json` | YAML (parameters, mode, ablations) | — |
| C13 | Evidence manifest | `evidence-manifest.schema.json` | JSON | — |
| C14 | `guardctl` CLI | `guardctl.md` | Commands, exit codes, outputs | — |
