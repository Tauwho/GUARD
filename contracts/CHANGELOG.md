# contracts CHANGELOG

## v1.0 -- frozen on merge (target Fri 20 Nov 2026 23:59 SGT)

Initial contracts. Frozen when the PR for #3 merges with approval from @phatdat220707 and @baofrovn, then tagged `contracts-v1.0`.

- C1-C7 wire messages: `messages.md` (header, 14 message types, `devctl` numbers, timeouts, errors).
- C8 telemetry: `telemetry.schema.json`.
- C9 remote planner (reserved for P1): `planner-remote.schema.json`.
- C10 reason codes: `reason_codes.yaml` (100-602).
- C11 scenarios and maps: `scenario.schema.json`.
- C12 config: `config.schema.json` (modes B0/B1/B2, ablations).
- C13 evidence manifest: `evidence-manifest.schema.json`.
- C14 `guardctl` CLI: `guardctl.md`.
- Fixtures: `fixtures/` (valid and invalid examples for every message and schema).
- Observation identity: one `obs_id` per world sample, shared by both feeds; provenance (checker 4) compares `obs_id` and `obs_stamp_ns` only (review fix, 16 Nov).
- Baselines B0/B1: planner -> gate pass-through (`DCMD_GUARD_PASSTHRU`, `PassthruCmd` 0x0014) and codes 514-515; PO-approved 15 Nov, pending A/B agreement.
- Telemetry: gate_transition.fsm_row also accepts P1-P5 (B0/B1 rows, review fix, 16 Nov).

After the freeze, every change needs a `decision` issue, an ADR, the `contract-change` label, a new version entry here, and approval from both @phatdat220707 and @baofrovn.
