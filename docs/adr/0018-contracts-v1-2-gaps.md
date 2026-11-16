# ADR-0018: Contracts v1.2: run summary events, acceleration interval, zone precedence

- **Status:** Accepted, 2026-11-16 (PO, #125; frozen early under D26)
- **Date:** 2026-11-16
- **Deciders:** @dngvmnh (PO), with @phatdat220707 and @baofrovn

## Context
The PR reviews on 16 Nov found three places where the contracts left behaviour undefined, so the code had to guess:
1. **Telemetry (C8).** `run_start` and `run_end` are listed event kinds, but their payloads are a bare `object`. The host replay CLI (#111) already depends on `run_end` fields the contract never defines (`contacts`, `zone_breaches`, `proposals`, `rejected`, `final_state`). The stream rules (event order, `seq` gaps) are also unwritten.
2. **Checker 9 (C1).** REF-CHECKERS says "implied acceleration > `a_max`", but not over which interval. #98 uses one observation period, and #108 / #109 showed that a command exactly on the limit flips between pass and fail through floating-point rounding.
3. **Scenarios (C11).** When a map's zone activation windows and a scenario's `zones:` events disagree, nothing says which wins. #107 lets scenario events override the map for the rest of the run.

The `messages.md` header also still says "v1.0 draft". A fourth gap came as a `CONTRACT-Q:` from @phatdat220707 on #3: C9 `created_ns` had no clock domain.

## Decision
- **C8:** define `run_start` (`scenario`, `mode`, `seed`; optional `ablate`, `contracts`, `runner`) and `run_end` (`final_state`, `contacts`, `zone_breaches`, `proposals`, `authorized`, `rejected`, `unverifiable`, `latches`; optional `min_margin_m`, `sim_duration_ns`). Stream rules: per `run_id`, `run_start` is first and `run_end` is last; `t_ns` never decreases; `seq` per `src` increases by 1, and a gap means dropped events. All values are functional (D11).
- **C1 checker 9:** `v_0` is the supervisor's own latest `v_meas`; `dt = 1 / obs_rate_hz` (default 50 ms). Fail with `REJ_ACCEL_ENVELOPE` if `v_cmd - v_0 > a_max * dt + 1e-9`. Only a speed increase is bounded; braking is limited by `a_brake` in the gate and plant (D17). The active-lease re-check applies only checker 9's speed bounds, because the gate ramps toward an installed target (question from @phatdat220707 on PR #127, answered by the PO 16 Nov).
- **C11:** the map schedule applies until the scenario's first `zones:` event for that zone; after that, scenario events alone decide the zone's state for the rest of the run.
- **C9 (P1):** `created_ns` in a route or proposal response is `based_on.stamp_ns` plus the host's elapsed compute time, so it stays in the target clock domain and is never compared with a host clock (ADR 0007). Route age (`route_max_age_ms`, C12) is enforced from the supervisor-owned stamp of `based_on.obs_id`.
- **Header:** `messages.md` names the version in force.

No wire-format change: same binary messages, sizes, `devctl` numbers and reason codes. The telemetry change only narrows two payloads that were previously unconstrained.

## Alternatives considered
- **Leave the payloads open and document them in `host/README.md`** (rejected). The bridge (B-8, #42) and the dashboard (A-9, #24) both read these events, so the shape must be a contract.
- **Bound deceleration by `a_max` too (`abs(v_cmd - v_0)`)** (rejected). It would reject a request to slow down, and slowing down is always safe; `a_brake` (1.0 m/s^2) already limits braking.
- **Use the time since the last proposal as the acceleration interval** (rejected). It depends on planner timing, which is exactly what GUARD must not trust.
- **Let the map windows win over scenario events** (rejected). Scenarios are the acceptance tests and need full control over the fault timeline; this also matches the current #107 code.

## Consequences
- #111 and the eval matrix (#23) can rely on `run_end`; #42 and #24 must emit and read the same shape.
- #98, #108 and #109 share one acceleration definition, so F0 false rejects no longer depend on rounding.
- #107 already matches the precedence rule.
- A new tag `contracts-v1.2` is pushed once this ADR is accepted.

## Links
- [DECISIONS.md ADR index](../../DECISIONS.md#adr-index)
- [docs/architecture.md: REF-CHECKERS](../architecture.md#ref-checkers)
- [docs/architecture.md: REF-CONTRACTS](../architecture.md#ref-contracts)
