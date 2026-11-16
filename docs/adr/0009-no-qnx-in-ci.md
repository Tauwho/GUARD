# ADR-0009: No QNX SDP in CI; evidence-commit protocol instead

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
The QNX non-commercial licence ties the SDP to each person's own machines and limits how it may be run and shared (licence §4.2.6, §4.6(b), §4.6(l)). Cloud CI would break that.

## Decision
CI is host-only (`ubuntu-24.04`) and never touches the SDP. Any change to `core/`, `qnx/`, `image/`, `cmake/` or `contracts/` attaches a text-only evidence bundle produced on a member's own target (`evidence/<yyyymmdd>-<sha7>-<topic>/`).

## Alternatives considered
- **Self-hosted runner with QNX** (rejected). Automated target tests, but it conflicts with the licence terms and needs a machine online.

## Consequences
- No licence risk from CI.
- Target behaviour is proven by committed transcripts, checked for schema, size and ancestry.
- Target regressions are caught at review time, not on every push.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-EVIDENCE](../architecture.md#ref-evidence)
- [docs/architecture.md: REF-CI](../architecture.md#ref-ci)
