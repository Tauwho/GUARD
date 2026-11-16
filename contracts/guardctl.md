# `guardctl` CLI (C14), contracts v1.0

`guardctl` is the operator tool on the QNX target. It talks to the gate (C3, C4) and, on the test image, to the sim (C7). It never bypasses gate rules.

**Runs as:** uid `guard_op` (1106) or root. `status` and `watch` work for any uid.

## Synopsis

```
guardctl status [--json]
guardctl watch [--interval-ms N]
guardctl recover
guardctl inject F1|F2|F3|F4|F5|F6|F7|F8 [--duration-ms N] [--param N]
guardctl scenario load <path> [--seed N] [--mode B0|B1|B2] [--ablate freshness,stopping,expiry,epoch]
guardctl scenario start
guardctl scenario stop
guardctl --help | --version
```

## Exit codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | Command ran, but the result was a refusal (for example `GATE_RECOVERY_UNMET`) |
| 2 | Usage error (bad command, argument or flag) |
| 3 | Permission denied (`EPERM`) |
| 4 | Timeout (`ETIMEDOUT`) or the device did not answer |
| 5 | Device missing (`ENOENT` on `/dev/guard/*` or `/dev/sim/*`) |
| 6 | Other I/O or protocol error (`EBADMSG`, `EMSGSIZE`, `ENOTSUP`, `EINVAL`, `EBUSY`) |

Errors go to stderr as one line: `guardctl: <command>: <reason>`. Stdout holds only the result.

## Commands

### `status [--json]`

Reads one snapshot from `/dev/guard/status` (C4).

- **Text output:** the C4 snapshot, unchanged.
- **JSON output:** one object, keys from C4. Numbers are JSON numbers; `recovery_unmet_mask` is an integer; counters are under `"counts"`.

```json
{"version":1,"mode":"LATCHED","epoch":2,"lease_id":0,"lease_seq":311,"lease_remaining_ns":0,
 "latch_cause":"SUPERVISOR_LOST","recovery_unmet_mask":4,"v_cmd":0.0,"w_cmd":0.0,
 "ticks":4500,"late_ticks":0,"counts":{"GATE_LEASE_ACCEPTED":310,"GATE_SUPERVISOR_LOST":1}}
```

Exit 0 on success, 4 or 5 if the gate is not reachable.

### `watch [--interval-ms N]`

Re-reads the status every N ms (default 200, range 50-5000) and redraws one screen until Ctrl-C. Shows mode, epoch, lease remaining, latch cause, unmet conditions by name, and the last 10 reason codes with counts. Exit 0 on Ctrl-C.

### `recover`

Sends `DCMD_GUARD_RECOVER` (C3) with a fresh `request_id`.

| Result | Stdout | Exit |
|---|---|---|
| `GATE_RECOVERED` | `recovered: mode=READY epoch=<n>` | 0 |
| `GATE_RECOVERY_UNMET` | `unmet: <names, comma-separated>` (from the mask, section 1.3 of `messages.md`) | 1 |
| `GATE_NOT_READY` | `not latched: mode=<mode>` | 1 |

Unmet names: `standstill`, `outside_zones`, `supervisor_registered`, `supervisor_fresh`, `latch_cleared`.

### `inject F1..F8 [--duration-ms N] [--param N]`

Test image only. Model faults go to the sim via `DCMD_SIM_WORLD_INJECT` (C7). Process faults act on real QNX processes.

| Fault | How `guardctl` injects it |
|---|---|
| F1 Stale planner | C7 inject; sim delays the planner feed by `--param` ms (default 600) |
| F2 Delayed authorization | Spawns `guard-hog --prio 32 --ms <duration>` (default 400 ms) |
| F3 Supervisor killed | `kill -9` on the `guard-supervisor` pid |
| F4 Sensor dropout | C7 inject; sim pauses the supervisor feed for `--duration-ms` (default 1000) |
| F5 Insufficient stopping distance | C7 inject; switches the planner to aggressive at `--param` mm/s (default 1200) |
| F6 Restricted zone | C7 inject; activates the MRI zone and increments `map_version` |
| F7 Zombie supervisor | `SIGSTOP` supervisor A, start supervisor B (new epoch), then `SIGCONT` A after `--duration-ms` (default 2000) |
| F8 Planner flood | C7 inject; planner sends at `--param` Hz (default 1000) for `--duration-ms` (default 2000) |

Stdout: `injected F<n> at t_ns=<now>`. Exit 0, or 3 / 4 / 5 / 6 on error.

### `scenario load|start|stop`

| Command | Action | Stdout | Exit |
|---|---|---|---|
| `scenario load <path>` | C7 `WorldLoad` with the path, seed (default 0), mode (default B2) and ablation mask | `loaded <id> mode=<m> seed=<s>` | 0; 6 if the file is invalid |
| `scenario start` | C7 `WorldPause` with `pause=0` | `started` | 0 |
| `scenario stop` | C7 `WorldPause` with `pause=1` | `stopped` | 0 |

`--json` is accepted on every command and wraps the stdout line as `{"ok":true,"result":"<line>"}` or `{"ok":false,"error":"<reason>","exit":<code>}`.
