# ADR-0016: MIT licence

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
The project needs an open-source licence that judges and the QNX codelab can reuse without friction.

## Decision
GUARD's own code is MIT-licensed. The QNX SDP is not included or redistributed.

## Alternatives considered
- **Apache-2.0** (rejected). Explicit patent grant, but longer and adds NOTICE handling for little benefit here.

## Consequences
- The codelab (P1) can reuse GUARD code directly.
- Each developer installs the SDP under their own licence.
- Third-party code added later must be MIT-compatible.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
