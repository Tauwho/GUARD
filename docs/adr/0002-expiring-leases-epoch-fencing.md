# ADR-0002: Expiring leases with epoch and sequence fencing

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
A heartbeat only shows that a sender is alive. It does not bound how long one authorization stays valid, and it cannot tell a restarted supervisor from a paused old one that wakes up later.

## Decision
Every authorization is a lease with `expires_ns` (default `T_lease` 250 ms, configurable 50-1000 ms). The gate accepts a lease only if its `epoch` equals the current epoch, its `lease_seq` is above every seen value, and it has not already expired. Each supervisor registration starts a new epoch.

## Alternatives considered
- **Heartbeat watchdog** (rejected). Simple and common, but it measures liveness, not whether the decision being executed is still valid; used as baseline B1.

## Consequences
- Authority is bounded in time: with no renewal the robot stops within `T_lease` + one tick.
- A zombie supervisor (F7) is refused with `GATE_EPOCH_STALE`.
- `T_lease` becomes a safety parameter: shorter leases reduce `d_req` but need more renewals.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-FSM](../architecture.md#ref-fsm)
- [docs/architecture.md: REF-CHECKERS](../architecture.md#ref-checkers)
- [docs/architecture.md: REF-PARAMS](../architecture.md#ref-params)
