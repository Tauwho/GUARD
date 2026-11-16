# ADR-0013: uid-based authorization in P0; secpol in P1 (P0 if spike S4 passes)

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
Only the supervisor may send leases and only the gate may drive the motor. QNX offers both client-credential checks and security policies (secpol), but secpol on the QSTI image is untested by the team.

## Decision
P0: each resource manager checks the caller's uid with `ConnectClientInfo` (gate accepts `guard_sup` and `guard_op`; sim motor accepts `guard_gate`), and every process locks its abilities after init. P1: add a secpol policy with default-deny connect edges, promoted to P0 if spike S4 passes.

## Alternatives considered
- **secpol only** (rejected). Stronger, but the spike might fail on QSTI and block P0.

## Consequences
- P0 does not depend on an unproven spike.
- Unauthorized callers get `EPERM` and are counted as `GATE_UNAUTHORIZED_CLIENT`.
- secpol is a separate P1 item (B-12a).

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-SECURITY](../architecture.md#ref-security)
- [docs/architecture.md: REF-PRIORITIES](../architecture.md#ref-priorities)
