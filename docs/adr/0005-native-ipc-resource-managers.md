# ADR-0005: Native message passing, pulses and resource managers on the target; TCP only to the host

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
The target processes need bounded, priority-aware IPC between planner, supervisor, gate and sim. QNX 8.0 no longer ships Qnet, so cross-node native IPC is not an option.

## Decision
On the target: `MsgSend`/`MsgReceive`/`MsgReply` for proposals (C1), `devctl` on resource managers for gate and sim (C2, C3, C5, C7), delayed-reply `read` for observations (C6), and timer pulses for ticks and lease expiry. TCP is used only for host links: telemetry (C8, port 7700) and the P1 remote planner (C9, port 7701).

## Alternatives considered
- **POSIX message queues or sockets everywhere** (rejected). Portable, but loses priority inheritance and the synchronous bounded send-receive-reply model.

## Consequences
- Priority inheritance and `TimerTimeout` bound every blocking call.
- The QNX features used are visible and demonstrable (Evidence Map).
- Host and target code differ at the IPC layer only; `core/` is unaffected.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CONTRACTS](../architecture.md#ref-contracts)
- [docs/architecture.md: REF-PRIORITIES](../architecture.md#ref-priorities)
