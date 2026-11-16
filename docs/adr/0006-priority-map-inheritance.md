# ADR-0006: Priority map with default priority inheritance on the proposal channel

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Proposal handling must not be able to starve the gate or the sim. If proposal work ran at a fixed high priority, a flooding planner could steal CPU from the monitor and gate.

## Decision
Fixed `SCHED_FIFO` map, all ≤ 63: sim 40, gate 35, hog 32, supervisor monitor 30, planner 10, bridge 8. The supervisor's proposal channel uses default inheritance, so proposal work runs at the planner's priority. `_NTO_CHF_FIXED_PRIORITY` is not used.

## Alternatives considered
- **`_NTO_CHF_FIXED_PRIORITY` on the proposal channel** (rejected). Predictable server priority, but planner traffic could then compete with the monitor.

## Consequences
- Starving the planner path can only let leases lapse, which stops the robot; it never extends motion.
- F2 (hog at 32) and F8 (flood) exercise this.
- Shared supervisor state needs mutexes with priority inheritance (to verify in spike S3).

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-PRIORITIES](../architecture.md#ref-priorities)
