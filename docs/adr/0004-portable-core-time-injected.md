# ADR-0004: Portable core with time injected; the QNX layer only wires IPC and timers

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
CI cannot run QNX (licence, ADR-0009). Logic inside QNX handlers could only be tested on a target by hand, and results would depend on timing.

## Decision
All business logic (types, codecs, checkers, gate FSM, plant model, planner, parsers) lives in `core/` as portable C++17 with no QNX headers, clock reads or I/O. Time is always a parameter. `qnx/` only connects IPC, timers, resource managers and privileges to `core/`.

## Alternatives considered
- **Logic inside QNX handlers** (rejected). Fewer layers, but no host CI and no deterministic replay.

## Consequences
- The same code runs on host CI (`guard-replay`, virtual time) and on the target.
- CI enforces the rule (`core-portable` check, AGENTS.md hard rule 2).
- Adapters in `qnx/` stay thin; reviewers reject logic there.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-LAYOUT](../architecture.md#ref-layout)
- [docs/architecture.md: REF-CI](../architecture.md#ref-ci)
