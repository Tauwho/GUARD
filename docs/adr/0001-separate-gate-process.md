# ADR-0001: A separate gate process owns the only actuator path

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
The robot must stop when the component that authorizes motion fails. If the supervisor wrote motor commands itself, a crash or hang of the supervisor would leave the last command in force, and its failure would be the actuator's failure.

## Decision
`guard-gate` is a separate QNX process and the only client the sim accepts on `/dev/sim/motor` (uid `guard_gate`). The supervisor can only hand the gate leases and revokes. The gate sends a `MotorCmd` every 10 ms in every state and stops on its own timer when a lease ends.

## Alternatives considered
- **Supervisor writes the motor directly** (rejected). One process fewer, but a supervisor failure leaves the robot moving until the sim watchdog acts.

## Consequences
- Supervisor death is a tested fault (F3), not an outage: the gate stops the robot and latches `SUPERVISOR_LOST`.
- Process isolation between deciding and actuating can be shown on the target (`pidin`, uid checks).
- One more IPC hop (C2) with a 5 ms bound; the gate must stay small and single-threaded (ADR-0015).

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-FSM](../architecture.md#ref-fsm)
- [docs/architecture.md: REF-CONTRACTS](../architecture.md#ref-contracts)
- [docs/architecture.md: REF-SECURITY](../architecture.md#ref-security)
