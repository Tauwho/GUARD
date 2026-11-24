# GUARD 3-minute demo

The demo as numbered, copy-pasteable commands. The video (`docs/video/`) is recorded
from these steps. Retained footage is from `v1.0-rc1`; the #222 remote scene
is a labelled replay of B take2 recorded target telemetry from main `09491a1`.

`python tools/check_demo_commands.py` checks this file: every command in a `ready`
step must exist on `main` and answer `--help` with the flags used here (shell scripts:
exist and parse with `bash -n`; target programs: sources under `qnx/<name>/`). A step
marked `planned (<SCOPE id>)` is listed but not run; the checker fails once that SCOPE
item is DONE and the step is still marked planned, so the script cannot fall behind
`main`.

Rules for every step (D11, ADR 0011):

- Numbers on screen come only from the host (Linux) harness and are labelled so.
- On-target steps show behaviour only: modes, latch causes, reason codes, logs. The
  target commands below filter their output to those fields, and the target scene
  crops the dashboard's speed and lease-time readouts.
- GUARD is a simulation-only research prototype. No QNX logo.

## Format

Each step is a `###` heading `N. Title [m:ss-m:ss]` (the slot in the video), a
`Status:` line (`ready` or `planned (<SCOPE id>)`), one `sh` block and the expected
result. Setup blocks run off camera and are checked like steps.

### Host setup (off camera)
Status: ready

```sh
python -m pip install PyYAML==6.0.3 jsonschema==4.26.0 websockets==17.3
cmake --preset host-debug
cmake --build --preset host-debug --parallel 4
mkdir -p out
```

Expected: the host build finishes; `out/` exists. Linux or WSL2 (see `host/README.md`).

### Target setup (off camera)
Status: ready

```sh
source ~/qnx800/qnxsdp-env.sh
image/run-demo.sh --test --dashboard
```

Expected: the image boots in QEMU with user-mode networking (ssh on 127.0.0.1:2222,
bridge C8 port forwarded to 127.0.0.1:7700, `docs/setup.md` Option C), `run-demo.sh`
shows the GUARD start log, starts `dashboard/relay.py --bridge 127.0.0.1:7700` and runs
F0 with `guardctl` (`image/README.md`). `--test` builds the test image, which
step 4 needs: on the default image `guardctl inject` answers "process faults need the
test image". Open `http://127.0.0.1:8765`. Stop everything
afterwards with `image/run-demo.sh --stop`.

### 1. Supervisor killed, host replay [0:00-0:36]
Status: ready

```sh
python host/guard-replay --scenario scenarios/F0.yaml --seed 1000 --mode B2 --out out/F0-B2.jsonl
python dashboard/relay.py --trace out/F0-B2.jsonl
python host/guard-replay --scenario scenarios/F3.yaml --seed 1003 --mode B2 --out out/F3-B2.jsonl
python host/guard-replay --scenario scenarios/F3.yaml --seed 1003 --mode B1 --out out/F3-B1.jsonl
```

Expected: the opening shot is the dashboard replaying the normal F0 run (exit 0, final
state `GOAL`); stop the relay with Ctrl-C. Both F3 runs exit 0 (actual outcome matches the scenario's hypothesis). Under
B2 the gate ends `LATCHED` with `GATE_SUPERVISOR_LOST` and `GATE_STANDSTILL`; under the
heartbeat baseline B1 the robot is still `MOVING`, because the planner's heartbeat is
alive (`scenarios/F3.yaml`, `expected`).

### 2. Dashboard replay of the same run [0:36-1:01]
Status: ready

```sh
python dashboard/relay.py --trace out/F3-B2.jsonl
```

Expected: open `http://127.0.0.1:8765`. The lease bar drains after the supervisor
dies, the mode goes STOPPING then LATCHED, and the reason journal shows
`GATE_SUPERVISOR_LOST`. The relay is read-only and binds to loopback only. Stop it
with Ctrl-C before step 3.

### 3. A refusal with its reason code [1:01-1:26]
Status: ready

```sh
python host/guard-replay --scenario scenarios/F6.yaml --seed 1006 --mode B2 --out out/F6-B2.jsonl
grep -m1 REJ_MAP_VERSION out/F6-B2.jsonl
python host/guard-replay --scenario scenarios/F6.yaml --seed 1006 --mode B0 --out out/F6-B0.jsonl
python dashboard/relay.py --trace out/F6-B2.jsonl --map scenarios/maps/mri_suite.yaml
python dashboard/relay.py --trace out/F6-B0.jsonl --map scenarios/maps/mri_suite.yaml
```

Expected: the MRI zone switches on while the planner still cites its cached map.
Under B2 the supervisor rejects the proposal with `REJ_MAP_VERSION` and the robot
waits outside the zone (final state READY, 0 violations). Under B0 (no supervisor)
the robot drives through the active zone (at least one violation). Both runs exit 0
because each matches its own hypothesis. Run the two relays one after the other
(Ctrl-C between them); `--map` is needed so the dashboard draws the MRI zone.

### 4. The same fault on the QNX target [1:26-1:56]
Status: ready

```sh
ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1 'guardctl status | grep -E "^(mode|latch_cause)="'
ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1 'guardctl inject F3 | cut -d" " -f1-2'
ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1 'guardctl status | grep -E "^(mode|latch_cause)="'
ssh -i ~/.ssh/guard_target_ed25519 -p 2222 root@127.0.0.1 guardctl recover
```

Expected (behaviour only): before the fault, `mode=MOVING`; `inject F3` prints
`injected F3`; then `mode=LATCHED` and `latch_cause=SUPERVISOR_LOST`. `recover`
either prints `recovered: mode=READY ...` or is refused with `unmet: <names>` and
exit 1 (`GATE_RECOVERY_UNMET`, `contracts/guardctl.md`). The dashboard follows the
target live through the relay `run-demo.sh --dashboard` started. Target evidence for
the same fault: `evidence/20261117-604281c-scenario-F3/`.

### 5. Remote model route checks [1:56-2:26]
Status: ready

Follow host/ADVISOR.md On the target. Warm the local model before booting QEMU;
use the develop image, not secure secpol. Capture several real takes if needed.

```sh
python host/llm_advisor.py --ollama
image/run-demo.sh
bash demo/remote-advisor.sh
python eval/target_runner.py --target ssh:guard-target --bridge 127.0.0.1:7700 --scenario scenarios/F6.yaml --out out/target-F6.jsonl
```

Expected: planner C8 route events show REJECTED / REJ_MAP_VERSION after the map
changes, and later USABLE / ROUTE_USABLE. Live inference is nondeterministic;
never fabricate or retime output. Remote mode ends GOAL rather than static F6's
READY. The runner still expects C1 REJ_MAP_VERSION, which is absent because
rejection occurs in the planner. Both documented mismatches are expected in
remote mode.
The local #222 cut uses B take2 recorded telemetry at original pacing, labelled
Recorded target trace replay (the supplied dashboard footage froze). Hide every
target numeric readout. Motion still needs supervisor and gate authorisation.

### 6. Results and limits [2:26-2:46]
Status: ready

```sh
python tools/build_readme.py --check
```

Expected: `build_readme: PASS`, so the README on screen matches `main`. Then open
`https://github.com/Tauwho/GUARD#results` and scroll from Results (host Linux
harness, labelled) to Limits. No QNX numbers.
