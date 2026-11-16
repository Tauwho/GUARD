# ADR-0008: Telemetry through shared-memory rings; read-only bridge and dashboard

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Every process emits telemetry. If emitting meant IPC or network I/O on the critical path, a slow host or dashboard could delay the gate or supervisor.

## Decision
Each process writes a single-writer, overwrite-oldest ring in `/dev/shmem/guard.<proc>.ring`. `guard-bridge` (prio 8) reads the rings and streams JSONL to the host. Bridge and dashboard have no control path.

## Alternatives considered
- **Telemetry over IPC from the critical path** (rejected). Simpler to write, but the critical path could block on telemetry.

## Consequences
- The critical path never blocks on telemetry; drops are counted and reported.
- The dashboard cannot command the robot.
- Ring layout is internal to `qnx/common` (B-3), not a frozen contract.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-SECURITY](../architecture.md#ref-security)
- [docs/architecture.md: REF-PRIORITIES](../architecture.md#ref-priorities)
