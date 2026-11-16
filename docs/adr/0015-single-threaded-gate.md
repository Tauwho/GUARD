# ADR-0015: Single-threaded gate dispatch

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
The gate handles `devctl` calls, tick pulses and lease-expiry pulses. With several threads, the order of a lease arriving and a lease expiring could vary between runs.

## Decision
The gate runs one dispatch loop on one channel. `devctl` handlers and pulses are processed in priority order, FIFO within a priority (spike S3 Q5, #89), and every handler is bounded.

## Alternatives considered
- **Thread pool** (rejected). Higher throughput, but non-deterministic ordering and locking inside the FSM.

## Consequences
- FSM transitions are strictly ordered and easy to replay.
- Handlers must stay short; slow work is not allowed in the gate.
- Gate load is bounded by the 50/s proposal rate limit upstream.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-FSM](../architecture.md#ref-fsm)
- [docs/architecture.md: REF-PRIORITIES](../architecture.md#ref-priorities)
