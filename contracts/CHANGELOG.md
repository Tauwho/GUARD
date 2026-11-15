# contracts CHANGELOG

## v1.0 -- frozen on merge (target Fri 20 Nov 2026 23:59 SGT)

Initial contracts. Frozen when the PR for #3 merges with approval from @phatdat220707 and @baofrovn, then tagged `contracts-v1.0`.

- C1-C7 wire messages: `messages.md` (header, 13 message types, `devctl` numbers, timeouts, errors).
- C8 telemetry: `telemetry.schema.json`.
- C9 remote planner (reserved for P1): `planner-remote.schema.json`.
- C10 reason codes: `reason_codes.yaml` (100-602).
- C11 scenarios and maps: `scenario.schema.json`.
- C12 config: `config.schema.json` (modes B0/B1/B2, ablations).
- C13 evidence manifest: `evidence-manifest.schema.json`.
- C14 `guardctl` CLI: `guardctl.md`.
- Fixtures: `fixtures/` (valid and invalid examples for every message and schema).

After the freeze, every change needs a `decision` issue, an ADR, the `contract-change` label, a new version entry here, and approval from both @phatdat220707 and @baofrovn.
