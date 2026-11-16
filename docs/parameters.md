# GUARD parameters

Default values for the simulated hospital robot and for GUARD's checks. They match REF-PARAMS in [architecture.md](architecture.md#ref-params) and the defaults in `contracts/config.schema.json` (C12). Every value can be changed per run in the config file.

**Tags:**
- **Sourced:** backed by the cited source.
- **Assumption:** a team choice at hospital-AMR scale with no direct source. Change it if a better source turns up.
- **Design:** a GUARD design choice; the rationale is the justification.

These are simulation parameters. They do not describe any real robot's tested behaviour.

## Motion envelope

| Parameter | Value | Tag | Rationale and source |
|---|---|---|---|
| `v_max` | 1.2 m/s | Sourced + assumption | Hospital delivery robots run near walking pace. The Aethon TUG spec sheet lists an operating speed of 1 m/s ([Swiss Post TUG spec sheet](https://media.post.ch/files/4vfch9m7/production-2026-11-03/2137859ba2dca82de7a62dd68ff347eb0d6c00e8.pdf)). 1.2 m/s matches a common design walking speed for people ([FHWA pedestrian study](https://www.fhwa.dot.gov/publications/research/safety/pedbike/98107/section2.cfm)). Going slightly above the TUG figure gives F5 a fast case to reject |
| `w_max` | 1.0 rad/s | Assumption | About 57°/s; a gentle turn rate for a cart-sized base in corridors |
| `a_max` | 0.5 m/s² | Assumption | Smooth starts with loaded carts; keeps the implied-acceleration check (checker 9) meaningful |
| `a_brake` | 1.0 m/s² | Assumption | A moderate controlled stop for a loaded cart, well below emergency braking. Used in `d_req` and by the gate's stop ramp |
| `r_robot` | 0.35 m | Sourced + assumption | Footprint radius for a 0.6-0.7 m wide base. The TUG brochure lists a 76 cm (30 in) dimension ([TUG brochure](https://www.robotize.co.il/wp-content/uploads/2021/02/specs-brochure-TUG-Eng.pdf)); 0.35 m covers a slightly smaller base plus its load |
| `margin` | 0.20 m | Assumption | Clearance added to obstacles and zone edges for sensing and localisation error |

## Timing

| Parameter | Value | Tag | Rationale and source |
|---|---|---|---|
| `T_lease` | 250 ms (50-1000) | Design | Long enough to survive a few missed renewals at 20 Hz, short enough that `d_req` at 1.2 m/s stays near 1.6 m. Lease length is a safety parameter: shorter leases allow more speed near hazards (ADR-0002) |
| `t_tick` | 10 ms | Design | Gate and sim tick at 100 Hz; the gate sends a `MotorCmd` every tick |
| `t_act` | 20 ms | Assumption | Allowance for the simulated drive to respond to a new command |
| Observation rate | 20 Hz | Assumption | Typical rate for fused localisation and obstacle tracks on indoor AMRs |
| `max_obs_age` | 200 ms | Design | Four observation periods. Older data is stale (checker 5, F1) |
| K (supersession) | 2 | Design | A proposal may lag at most two newer observations (checker 6) |
| Sensor stale threshold | 150 ms | Design | Three missed observations on the supervisor feed make it UNVERIFIABLE (checker 8, F4) |
| Standstill | v_meas < 0.02 m/s for 5 ticks | Design | Measured speed, not commanded speed, confirms the stop (50 ms window) |
| Actuator watchdog | 50 ms | Design | Five missed gate ticks. Last stop layer if the gate dies (stand-in for a drive-level stop; GUARD does not implement STO) |
| B1 heartbeat timeout | 500 ms | Design | Baseline B1 only; a typical heartbeat setting, longer than the F2 hog window on purpose |

## Load bounds

| Parameter | Value | Tag | Rationale |
|---|---|---|---|
| Rate limit | 50 proposals/s per planner | Design | 2.5 × the 20 Hz planning rate. F8 floods at 1000/s |
| Observation history | 64 | Design | 3.2 s at 20 Hz; enough for checkers 4-6 |
| Zones | ≤ 32 | Design | Bounds checker 10 cost |
| Obstacles | ≤ 16 | Design | Bounds checker 11 cost and the fixed `Observation` size (464 bytes) |
| Route max age (P1) | 5 s | Assumption | Remote route advice older than this is ignored |

## Scenario values

| Value | Tag | Rationale and source |
|---|---|---|
| Staff walking speed 1.0-1.2 m/s in crossings | Sourced | 1.2 m/s is the usual design walking speed for crossings ([FHWA](https://www.fhwa.dot.gov/publications/research/safety/pedbike/98107/section2.cfm)); many older adults walk slower ([PMC5485874](https://pmc.ncbi.nlm.nih.gov/articles/PMC5485874/)), so 1.0 m/s is also used |
| Cruise speed 0.8 m/s for the rule planner | Assumption | Below the TUG's 1 m/s, typical when sharing corridors |

## Stopping distance

```
t_r   = T_lease + t_tick + t_act
v_pk  = min(v_cmd, v_0 + a_max * t_r)
d_req = v_pk * t_r + v_pk^2 / (2 * a_brake) + margin + r_robot
```

| `v_cmd` | `t_r` | `d_req` |
|---|---|---|
| 0.4 m/s | 0.28 s | 0.112 + 0.080 + 0.20 + 0.35 = **0.74 m** |
| 0.8 m/s | 0.28 s | 0.224 + 0.320 + 0.20 + 0.35 = **1.09 m** |
| 1.2 m/s | 0.28 s | 0.336 + 0.720 + 0.20 + 0.35 = **1.61 m** |

(Assumes `v_0` is already at `v_cmd`.) With `T_lease` = 100 ms, `t_r` = 0.13 s and `d_req` at 1.2 m/s falls to 1.43 m.

## Standards context

GUARD claims no compliance with any standard (see [limits.md](limits.md)). Stop states are named by analogy only: STOPPING is like stop category 1 / SS1 in IEC 60204-1, and STOPPED is like SOS. The parameters above were not derived from ISO 3691-4 or any other standard; any match is a coincidence.
