# contracts CHANGELOG

## v1.0 -- frozen on merge (target Fri 20 Nov 2026 23:59 SGT)

Initial contracts. Frozen when the PR for #3 merges (reviews optional, D12), then tagged `contracts-v1.0`.

- C1-C7 wire messages: `messages.md` (header, 14 message types, `devctl` numbers, timeouts, errors).
- C8 telemetry: `telemetry.schema.json`.
- C9 remote planner (reserved for P1): `planner-remote.schema.json`.
- C10 reason codes: `reason_codes.yaml` (100-602; 42 codes).
- C11 scenarios and maps: `scenario.schema.json`.
- C12 config: `config.schema.json` (modes B0/B1/B2, ablations).
- C13 evidence manifest: `evidence-manifest.schema.json`.
- C14 `guardctl` CLI: `guardctl.md`.
- Fixtures: `fixtures/` (valid and invalid examples for every message and schema).
- Observation identity: one `obs_id` per world sample, shared by both feeds; provenance (checker 4) compares `obs_id` and `obs_stamp_ns` only (review fix, 16 Nov).
- Baselines B0/B1: planner -> gate pass-through (`DCMD_GUARD_PASSTHRU`, `PassthruCmd` 0x0014) and codes 514-515; PO-approved 15 Nov, pending A/B agreement.
- Telemetry: gate_transition.fsm_row also accepts P1-P5 (B0/B1 rows, review fix, 16 Nov).
- Gate: revokes 400 and 404 latch SENSOR_LOST; other revokes do not (review fix, 16 Nov; matches F4).
- Ablation expiry: gate ignores expires_ns; wire format unchanged (review fix, 16 Nov).
- Lease delivery rule: a lease takes effect only after its LeaseAck is delivered; an undelivered ack discards the lease and emits new code `GATE_ACK_UNDELIVERED` (516) (CONTRACT-Q on #3, 16 Nov).
- Lease `v_cmd` range is 0 to 1.2: v1 authorizes no reverse motion (CONTRACT-Q on #3, 16 Nov). Proposal `v_cmd` keeps its schema bound; checker 9 rejects negative values.

After the freeze, every change needs a `decision` issue, an ADR, the `contract-change` label, a new version entry here, and an @-mention of both @phatdat220707 and @baofrovn on the PR (no approval required, D12).
