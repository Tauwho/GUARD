<!-- managed-by: guard-bootstrap v1 -->
# qnx/

QNX 8.0 adapters only (AGENTS.md rule 2). This layer wires message passing, timers, resource managers and privileges to `core/`. It contains no business logic.

## Priority map

All threads use `SCHED_FIFO`, priority ≤ 63.

| Thread / event | Prio | Mechanism |
|---|---|---|
| sim tick | 40 | `timer_create` + `SIGEV_PULSE_INIT`, 10 ms |
| gate tick + lease expiry | 35 | Periodic + one-shot timer pulses |
| gate resmgr | Inherits the sender | Single-threaded dispatch |
| supervisor monitor + obs reader | 30 | 20 ms pulse; blocking `read` |
| supervisor proposal server | Inherits the planner (10) | `name_attach`; default inheritance; **not** `_NTO_CHF_FIXED_PRIORITY` |
| hog (F2) | 32 | Busy loop |
| planner | 10 | — |
| bridge | 8 | Ring reader |

**Privilege drop:** each process starts as root, then:
1. attaches its path (`PROCMGR_AID_PATHSPACE`) or public channel (`PROCMGR_AID_PUBLIC_CHANNEL`);
2. sets its priority;
3. calls `setuid` to its own uid;
4. locks its abilities with `procmgr_ability`.

**uids:** `guard_sim` 1101, `guard_gate` 1102, `guard_sup` 1103, `guard_plan` 1104, `guard_bridge` 1105, `guard_op` 1106.

**Rule:** starving the planner path can only let leases lapse, which stops the robot. It never extends motion.
