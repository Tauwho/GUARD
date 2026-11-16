# ADR-0017: LeaseAck delivery on a resmgr channel

- **Status:** Proposed
- **Date:** 2026-11-16
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn
- **Issue:** #89

## Context
Contract C2 v1.0 says that when the supervisor's 5 ms `TimerTimeout` fires during a `DCMD_GUARD_LEASE`, the supervisor gets `ETIMEDOUT` and the gate's `MsgReply` fails with `ESRCH`, so the gate discards the lease (messages.md 3.2 step 4).

Spike S3 (#33) tested this on QSTI. A resmgr channel is created with `_NTO_CHF_UNBLOCK`:
- a timeout while the client is **send**-blocked returns `ETIMEDOUT` at once, and the server never sees the message;
- a timeout while the client is **reply**-blocked sends the server an unblock pulse, and the client keeps waiting. With `iofunc_unblock_default` it got the late reply (`EOK`). With an unblock handler it got the handler's `MsgError`.
- the server's `MsgReply` did not fail in either case, not even for a client killed with `kill -9`.

So the `ESRCH` path in v1.0 does not happen, and the 5 ms bound is not what the supervisor sees once the gate has the message.

## Decision
- The gate answers every `DCMD_GUARD_LEASE` within the same handler call. It never defers a `LeaseAck`.
- Order stays: record `lease_seq`, `MsgReply` the ack, install only if the reply succeeded. Any reply failure discards the lease and emits `GATE_ACK_UNDELIVERED`.
- Unblock pulses for answered `rcvid`s are ignored.
- For the supervisor, `ETIMEDOUT` means "not received, not installed". A late `LeaseAck` is binding.
- The 5 ms bounds send-blocked time. Reply-blocked time is bounded by the gate handler (ADR 0015).

## Alternatives considered
- **Gate defers the reply and answers the unblock pulse with `MsgError(ETIMEDOUT)`** (rejected for P0). Keeps a 5 ms bound on the supervisor side, but adds deferred-reply state to the gate.
- **Keep v1.0 text** (rejected). Its timeout path cannot happen on QNX, so tests and reviewers would rely on behaviour that does not exist.

## Consequences
- The supervisor and gate still agree on every lease: the kernel delivers either `ETIMEDOUT` (gate never saw it) or the reply.
- `GATE_ACK_UNDELIVERED` stays as a defensive code; it is not expected in normal runs.
- `qnx/common` (#37) and the gate (#39) must not use `_RESMGR_NOREPLY` for later replies on C2.
- The host harness models a late ack as "accepted", not as a timeout.

## Links
- [contracts/messages.md section 3.2](../../contracts/messages.md)
- [DECISIONS.md spike log, S3](../../DECISIONS.md#spike-log)
- [ADR 0015: single-threaded gate](0015-single-threaded-gate.md)
