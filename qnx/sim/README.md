# qnx/sim: `guard-sim` (B3, #38)

QNX adapter around `guard::Plant` and `guard::World` from `core/`. One single-threaded resource manager (ADR 0015); a 10 ms pulse timer at priority 40 steps the world and the plant.

| Path | Contract | Who may call |
|---|---|---|
| `/dev/sim/world` | C7 `devctl` load / inject / pause; `read()` gives a `key: value` status snapshot | `devctl`: uid 1106 or root. `read`: any uid |
| `/dev/sim/motor` | C5 `DCMD_SIM_MOTOR` | uid 1102 only |
| `/dev/sim/obs/supervisor` | C6, blocking `read`, delayed reply | uid 1103 only |
| `/dev/sim/obs/planner` | C6, same; F1 delays it | uid 1104 only |

Inputs are the normalized files from `host/prepare_inputs.py` (#141), packed by `image/build-image.sh` under `/system/guard/scenarios/`. `WorldLoad.scenario_path` names the scenario; the map is `maps/<map>.yaml` and the config `config.yaml` in the same folder.

Telemetry goes to the shared-memory ring `/guard_tlm_sim` (`qnx/common` `tlm.hpp`), read by `guard-bridge`.

## Check on the target

```sh
bash image/target-check.sh --project qnx/sim --copy guard-sim --copy guard_sim_check \
  --run "slay -f guard-gate guard-sim; /tmp/guard_sim_check /tmp/guard-sim /system/guard/scenarios/F0.yaml" process-sim
```

The image must contain the inputs (`image/run-demo.sh --rebuild` after `scenarios/` changes).
