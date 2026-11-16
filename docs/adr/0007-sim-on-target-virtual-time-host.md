# ADR-0007: The plant simulator runs on the target; the host harness uses virtual time

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Freshness and lease checks compare timestamps. If the world model ran on the host, target and host clocks would have to be compared, and CI runs would not be reproducible.

## Decision
`guard-sim` runs on the QNX target, so every target timestamp comes from one `CLOCK_MONOTONIC`. On the host, `guard-replay` runs the same `core` in a seeded discrete-event loop with a virtual clock and emits the same telemetry schema.

## Alternatives considered
- **Simulator on the host** (rejected). Easier visuals, but two clock domains and network jitter inside the control loop.

## Consequences
- One clock domain on the target; no clock sync needed.
- Host CI runs are deterministic per seed (3,150-run matrix).
- Process-level faults (F2, F3, F7) need a host abstraction; real behaviour is shown on the target.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-DATAMODEL](../architecture.md#ref-datamodel)
- [docs/architecture.md: REF-SCENARIOS](../architecture.md#ref-scenarios)
