# ADR-0011: Publish functional metrics only

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Licence §4.6(i) restricts publishing performance or functional evaluations of the QNX software without BlackBerry's written approval. The question is open with the organisers (Q5, issue #2).

## Decision
Public material (README, video, Devpost, posts) reports only GUARD's functional results: violations, reason codes, stop sequences, sim-time stop durations. Internal timing histograms stay internal. This changes only if DECISIONS.md records written approval.

## Alternatives considered
- **Publish latency numbers** (rejected). Strong technical story, but a licence risk.

## Consequences
- No QNX latency, jitter or QNX-vs-Linux numbers are published.
- Claims-lint and review enforce the wording.
- If approval arrives, a new ADR can relax this.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CLAIMS](../architecture.md#ref-claims)
- [docs/architecture.md: REF-SCENARIOS](../architecture.md#ref-scenarios)
