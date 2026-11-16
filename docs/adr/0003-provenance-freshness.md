# ADR-0003: Provenance freshness against a supervisor-owned observation history

- **Status:** Accepted
- **Date:** 2026-11-15
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
A proposal can be sent just now but computed from an old view of the world. Checking message age alone (as ROS 2 lifespan does) misses that case, which is the core failure GUARD targets (F1).

## Decision
Every proposal declares `based_on` (observation id, seq, stamp, map version). The supervisor keeps its own history of the last 64 observations and checks the claim against it: known id, matching stamp, age at most `max_obs_age` (200 ms), no more than K = 2 newer observations, current map version (checkers 4-7).

## Alternatives considered
- **Message age (ROS 2 lifespan)** (rejected). Only checks transport delay, not the age of the data behind the decision.

## Consequences
- Stale planners are rejected with `REJ_STALE_OBSERVATION` or `REJ_SUPERSEDED_OBSERVATION`.
- Provenance is declared, so a lying planner can pass checks 4-7; checks 8-11 still judge the command against the current world. Signed proposals are P2.
- The supervisor needs its own observation feed, separate from the planner's (C6).

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CHECKERS](../architecture.md#ref-checkers)
- [docs/architecture.md: REF-DATAMODEL](../architecture.md#ref-datamodel)
