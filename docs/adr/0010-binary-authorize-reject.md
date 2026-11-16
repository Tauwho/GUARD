# ADR-0010: Binary authorize or reject

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
When a proposal fails a check, the supervisor could either refuse it or change it into something it considers acceptable.

## Decision
Each proposal is AUTHORIZED, REJECTED or UNVERIFIABLE (treated as rejected). The supervisor never rewrites a command; the planner must send a new proposal.

## Alternatives considered
- **Clamp to a safe speed** (rejected). Keeps the robot moving, but the supervisor then becomes a second planner and its outputs are harder to explain.

## Consequences
- Every decision has one primary reason code and a full reasons mask.
- The trusted part stays small.
- Throughput near hazards depends on the planner proposing lower speeds (F5).

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CHECKERS](../architecture.md#ref-checkers)
