# ADR-0012: Compile-time checker registry; all checkers run; first failure is the primary reason

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Checkers must be deterministic, explainable and bounded in cost. Plug-ins loaded at run time, or stopping at the first failure, would make results depend on load order and hide secondary causes.

## Decision
Checkers 1-11 are registered at compile time in a fixed order. All of them run on every proposal. The primary reason is the first non-pass; any UNVERIFIABLE result makes the decision UNVERIFIABLE. Inputs are bounded (≤ 32 zones, ≤ 16 obstacles).

## Alternatives considered
- **`dlopen` plug-ins** (rejected). Extensible at run time, but non-deterministic and a larger attack surface.
- **Fail-fast** (rejected). Cheaper per proposal, but loses the full picture of what was wrong.

## Consequences
- `reasons_mask` shows every failing checker.
- Worst-case cost is O(zones + obstacles) per checker.
- Adding a checker is a code change with a new reason code.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CHECKERS](../architecture.md#ref-checkers)
- [docs/architecture.md: REF-REASONCODES](../architecture.md#ref-reasoncodes)
