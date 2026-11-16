# ADR-0014: Simulation only

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Licence §4.2.5 excludes high-risk uses, the team has no robot hardware, and the budget is $0.

## Decision
GUARD drives only the simulated plant in `guard-sim`. It is never connected to real motors or actuators.

## Alternatives considered
- **Hardware robot** (rejected). More convincing demo, but licence risk, cost and real physical risk.

## Consequences
- Claims stay within the README Limits (no real actuators).
- All faults can be injected safely and repeated with seeds.
- QEMU timing is not hardware timing; this is stated in Limits.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CLAIMS](../architecture.md#ref-claims)
