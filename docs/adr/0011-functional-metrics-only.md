# ADR-0011: Publish functional metrics only

- **Status:** Accepted; amended 2026-11-16 by D11 (see Amendment)
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Licence §4.6(i) restricts publishing performance or functional evaluations of the QNX software without BlackBerry's written approval. We asked the organisers (Q5, issue #2). They replied that they cannot answer, so the team proceeds on assumption A5: approval is treated as denied.

## Decision
Public material (README, video, Devpost, posts) reports only GUARD's functional results: violations, reason codes, stop sequences, sim-time stop durations. Internal timing histograms stay internal. This changes only if DECISIONS.md records written approval.

## Alternatives considered
- **Publish latency numbers** (rejected). Strong technical story, but a licence risk.

## Consequences
- No QNX latency, jitter or QNX-vs-Linux numbers are published.
- Claims-lint and review enforce the wording.
- If approval arrives, a new ADR can relax this.

## Amendment (2026-11-16, D11)
D11 supersedes D5 under assumption A5 and narrows this decision:
- Nothing measured on QNX is published: no timing numbers, no QNX-vs-Linux comparison, and no numeric result tables from the target.
- Behaviour on the QNX target is described in words only, for example "shown on the QNX target".
- Numeric results (violation counts, reason-code counts, stop durations) come only from the host harness and are labelled "host (Linux) harness".
- This ADR is relaxed only if DECISIONS.md records BlackBerry's written approval.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CLAIMS](../architecture.md#ref-claims)
- [docs/architecture.md: REF-SCENARIOS](../architecture.md#ref-scenarios)
- [DECISIONS.md decision log (D5, D11)](../../DECISIONS.md#decision-log)
- Issue #2 (clarifications, assumptions A1-A8)
