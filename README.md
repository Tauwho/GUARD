# GUARD — Gated Unexpired Authorization for Robotic Drives

**Stale decisions can't move the robot.**

GUARD is an independent runtime supervisor for a *simulated* autonomous mobile
robot on QNX 8.0. An untrusted planner (rule-based, optionally steered by a local
LLM advisor) only *proposes* motion. GUARD checks each proposal for provenance
freshness, stopping distance and restricted zones, then grants a short,
epoch-fenced lease. A separate gate process uses its own timer to stop the robot
when the lease expires, even if the supervisor has died.

The checks, lease rules and gate state machine are a portable C++17 core. The
same core runs under a deterministic host harness and inside QNX 8.0 processes
(gate, supervisor, planner, simulator, bridge, guardctl) that wire it to message
passing, timers and resource managers. The target runs in QEMU, not on hardware;
target runs are reported as behaviour only (Evidence Map below).

Why QNX: the gate runs as an independent resource-manager process with its own
timer and message path, so the component that stops the robot does not share a
fate with the component that authorises it. The supervisor, gate, planner and
simulator are separate processes; only the planner is treated as untrusted, and
a secpol policy stops it from connecting to the gate at all.

> **Status:** in development for the QNX Global Hackathon 2026 (Robotics &
> Medical). Internal code freeze 10 Dec 2026, submission 15 Dec 2026.

## Demo

![Dashboard replay of host scenario F3: the supervisor dies, the lease expires and the gate stops the robot](https://github.com/Tauwho/GUARD/releases/download/v1.0-rc1/dashboard-f3-replay.gif)


The 3-minute demo (host replay F3, dashboard, target `run-demo.sh` with
`guardctl`, and a refusal with its reason code) is specified in [demo/DEMO.md](demo/DEMO.md)
and recorded from `v1.0-rc1`. Video and GIF: planned (P4) - under 3:00, no QNX
logo, [storyboard and captions plan](docs/video/storyboard.md) in `docs/video/`.

## The problem

A planner decides from an observation, and by the time its command arrives the
world may have moved on: a door closed, a pedestrian stepped out, an MRI suite
became a restricted zone. A slow planner such as an LLM makes this worse. Its
answer can be right for the world it saw and wrong for the world the robot is in
now. In a hospital the robot shares corridors with people, so the part that lets
it move should not trust the planner's timing or its claims, and should stop the
robot by itself when its permission runs out.

## How it works

![GUARD design: planner, supervisor, gate, simulator, bridge and guardctl as separate QNX processes; dashboard and guard-replay on the host](docs/architecture.svg)

The diagram shows the design in `docs/architecture.md`; each target box is a QNX
process on the target image, with its evidence bundle in the Evidence Map.

1. The planner proposes a trajectory with a map version and a timestamp.
2. The supervisor rejects the proposal if the map is stale, the stopping
   distance exceeds the measured clearance, or the path enters a restricted
   zone. A rejection changes nothing and carries a reason code.
3. An accepted proposal gets a lease: an id, an epoch and a deadline.
4. The gate admits motion only under the current epoch and stops when the lease
   expires. A stale lease from a previous supervisor cannot be reused.
5. Every decision is a C8 telemetry record; the dashboard replays them
   read-only, and evidence bundles keep the transcripts.

Scenarios F0-F8 inject faults (stale map, planner stall, supervisor death,
replaced supervisor, restricted-zone entry) and compare outcomes against
hypotheses. See `docs/architecture.md` and the ADRs in `docs/adr/`.

<!-- gen:evidence-map -->
## Evidence Map

| Evidence | Where it ran | Commit | Contracts | Bundle |
|---|---|---|---|---|
| llm-route | QNX 8.0 target (behaviour only) | `36f0bf2` | contracts-v1.2 | [20261119-36f0bf2-llm-route](evidence/20261119-36f0bf2-llm-route/manifest.json) |
| remote-planner | QNX 8.0 target (behaviour only) | `36f0bf2` | contracts-v1.2 | [20261119-36f0bf2-remote-planner](evidence/20261119-36f0bf2-remote-planner/manifest.json) |
| core-on-target | QNX 8.0 target (behaviour only) | `bdb25b9` | contracts-v1.2 | [20261119-bdb25b9-core-on-target](evidence/20261119-bdb25b9-core-on-target/manifest.json) |
| scenario-f0 | QNX 8.0 target (behaviour only) | `db1dcc7` | contracts-v1.2 | [20261117-db1dcc7-scenario-F0](evidence/20261117-db1dcc7-scenario-F0/manifest.json) |
| slm | QNX 8.0 target (behaviour only) | `db1dcc7` | contracts-v1.2 | [20261117-db1dcc7-slm](evidence/20261117-db1dcc7-slm/manifest.json) |
| tracelogger | QNX 8.0 target (behaviour only) | `55753de` | contracts-v1.2 | [20261117-55753de-tracelogger](evidence/20261117-55753de-tracelogger/manifest.json) |
| qnx-common-priv | QNX 8.0 target (behaviour only) | `739744a` | contracts-v1.2 | [20261117-739744a-qnx-common-priv](evidence/20261117-739744a-qnx-common-priv/manifest.json) |
| secpol | QNX 8.0 target (behaviour only) | `c6a83fd` | contracts-v1.2 | [20261117-c6a83fd-secpol](evidence/20261117-c6a83fd-secpol/manifest.json) |
| image-demo | QNX 8.0 target (behaviour only) | `b91ea6b` | contracts-v1.2 | [20261117-b91ea6b-image-demo](evidence/20261117-b91ea6b-image-demo/manifest.json) |
| image-inputs | QNX 8.0 target (behaviour only) | `179b8d3` | contracts-v1.2 | [20261117-179b8d3-image-inputs](evidence/20261117-179b8d3-image-inputs/manifest.json) |
| scenario-f7 | QNX 8.0 target (behaviour only) | `604281c` | contracts-v1.2 | [20261117-604281c-scenario-F7](evidence/20261117-604281c-scenario-F7/manifest.json) |
| scenario-f3 | QNX 8.0 target (behaviour only) | `604281c` | contracts-v1.2 | [20261117-604281c-scenario-F3](evidence/20261117-604281c-scenario-F3/manifest.json) |
| scenario-f2 | QNX 8.0 target (behaviour only) | `604281c` | contracts-v1.2 | [20261117-604281c-scenario-F2](evidence/20261117-604281c-scenario-F2/manifest.json) |
| qnx-common-tlm | QNX 8.0 target (behaviour only) | `604281c` | contracts-v1.2 | [20261117-604281c-qnx-common-tlm](evidence/20261117-604281c-qnx-common-tlm/manifest.json) |
| scenario-f8 | QNX 8.0 target (behaviour only) | `77a4b96` | contracts-v1.2 | [20261117-77a4b96-scenario-F8](evidence/20261117-77a4b96-scenario-F8/manifest.json) |
| scenario-f6 | QNX 8.0 target (behaviour only) | `77a4b96` | contracts-v1.2 | [20261117-77a4b96-scenario-F6](evidence/20261117-77a4b96-scenario-F6/manifest.json) |
| scenario-f5 | QNX 8.0 target (behaviour only) | `77a4b96` | contracts-v1.2 | [20261117-77a4b96-scenario-F5](evidence/20261117-77a4b96-scenario-F5/manifest.json) |
| scenario-f4 | QNX 8.0 target (behaviour only) | `77a4b96` | contracts-v1.2 | [20261117-77a4b96-scenario-F4](evidence/20261117-77a4b96-scenario-F4/manifest.json) |
| scenario-f1 | QNX 8.0 target (behaviour only) | `77a4b96` | contracts-v1.2 | [20261117-77a4b96-scenario-F1](evidence/20261117-77a4b96-scenario-F1/manifest.json) |
| scenario-f0 | QNX 8.0 target (behaviour only) | `8c1e748` | contracts-v1.2 | [20261117-8c1e748-scenario-F0](evidence/20261117-8c1e748-scenario-F0/manifest.json) |
| qnx-ctl-bridge | QNX 8.0 target (behaviour only) | `dbcd97c` | contracts-v1.2 | [20261117-dbcd97c-qnx-ctl-bridge](evidence/20261117-dbcd97c-qnx-ctl-bridge/manifest.json) |
| process-planner | QNX 8.0 target (behaviour only) | `41aa9a7` | contracts-v1.1 | [20261117-41aa9a7-process-planner](evidence/20261117-41aa9a7-process-planner/manifest.json) |
| process-supervisor | QNX 8.0 target (behaviour only) | `edfa19a` | contracts-v1.1 | [20261117-edfa19a-process-supervisor](evidence/20261117-edfa19a-process-supervisor/manifest.json) |
| process-sim | QNX 8.0 target (behaviour only) | `f948929` | contracts-v1.1 | [20261117-f948929-process-sim](evidence/20261117-f948929-process-sim/manifest.json) |
| qnx-common-tlm | QNX 8.0 target (behaviour only) | `f948929` | contracts-v1.1 | [20261117-f948929-qnx-common-tlm](evidence/20261117-f948929-qnx-common-tlm/manifest.json) |
| process-gate | QNX 8.0 target (behaviour only) | `2395caa` | contracts-v1.1 | [20261117-2395caa-process-gate](evidence/20261117-2395caa-process-gate/manifest.json) |
| core-on-target | QNX 8.0 target (behaviour only) | `5f957d9` | contracts-v1.1 | [20261117-5f957d9-core-on-target](evidence/20261117-5f957d9-core-on-target/manifest.json) |
| image-layout | QNX 8.0 target (behaviour only) | `6913b85` | contracts-v1.1 | [20261116-6913b85-image-layout](evidence/20261116-6913b85-image-layout/manifest.json) |
| qnx-hog | QNX 8.0 target (behaviour only) | `3ed822e` | contracts-v1.1 | [20261116-3ed822e-qnx-hog](evidence/20261116-3ed822e-qnx-hog/manifest.json) |
| qnx-common-priv | QNX 8.0 target (behaviour only) | `f6dc640` | contracts-v1.1 | [20261116-f6dc640-qnx-common-priv](evidence/20261116-f6dc640-qnx-common-priv/manifest.json) |
| qnx-common-resmgr | QNX 8.0 target (behaviour only) | `0da47f6` | contracts-v1.1 | [20261116-0da47f6-qnx-common-resmgr](evidence/20261116-0da47f6-qnx-common-resmgr/manifest.json) |
| qnx-common-timer | QNX 8.0 target (behaviour only) | `667ec80` | contracts-v1.1 | [20261116-667ec80-qnx-common-timer](evidence/20261116-667ec80-qnx-common-timer/manifest.json) |
| qnx-common | QNX 8.0 target (behaviour only) | `7bdd68b` | contracts-v1.1 | [20261116-7bdd68b-qnx-common](evidence/20261116-7bdd68b-qnx-common/manifest.json) |
| setup-b | QNX 8.0 target (behaviour only) | `60aaa66` | contracts-v1.1 | [20261116-60aaa66-setup-b](evidence/20261116-60aaa66-setup-b/manifest.json) |
| setup-po | QNX 8.0 target (behaviour only) | `161b1e8` | - | [20261116-161b1e8-setup-po](evidence/20261116-161b1e8-setup-po/manifest.json) |
| core-on-target | QNX 8.0 target (behaviour only) | `161b1e8` | contracts-v1.0 | [20261116-161b1e8-core-on-target](evidence/20261116-161b1e8-core-on-target/manifest.json) |

Target bundles are transcripts, reason codes and test counts: behaviour, not
measurements. Numbers appear only in the host (Linux) Results section.
<!-- gen:evidence-map-end -->

<!-- gen:quickstart -->
## Quick start
### Host replay (Linux or WSL2)

```sh
python host/guard-replay --scenario scenarios/F0.yaml --seed 1000 --mode B2 --out out/F0.jsonl
```

### Dashboard (Linux or WSL2)

```sh
python dashboard/relay.py --trace out/runs/F3-B2-1003.jsonl
```

### On target (QNX SDP required, B's and P's machines only)

```sh
source ~/qnx800/qnxsdp-env.sh
image/run-demo.sh                 # user-mode networking, ssh on 127.0.0.1:2222, no sudo
image/run-demo.sh --net bridge    # br0, 10.10.10.2 (docs/setup.md Option B)
image/run-demo.sh --test          # adds guard-hog (fault F2)
image/run-demo.sh --dashboard     # boot, live dashboard on http://127.0.0.1:8765, run F0
image/run-demo.sh --stop
```
<!-- gen:quickstart-end -->

`bash tools/fresh_clone_demo.sh <ref>` replays the host part of this quick start
in a fresh clone and is what the nightly job runs.

<!-- gen:results -->
## Results

Source: host (Linux) harness, commit `6220b91`, 1 repeat(s) per scenario/config, [run 43907070270](https://github.com/Tauwho/GUARD/actions/runs/43907070270). Simulation only; no QNX numbers.

| Scenario | Runs | B2 gate mismatches | Baseline (B0/B1) mismatches | Ablation differences |
|---|---:|---:|---:|---:|
| F0 | 7 | 0 | 0 | 0 |
| F1 | 7 | 0 | 2 | 1 |
| F2 | 7 | 0 | 0 | 1 |
| F3 | 7 | 0 | 0 | 0 |
| F4 | 7 | 0 | 0 | 0 |
| F5 | 7 | 0 | 0 | 1 |
| F6 | 7 | 0 | 0 | 1 |
| F7 | 7 | 0 | 0 | 1 |
| F8 | 7 | 0 | 0 | 0 |

B2 mismatches are failures (CI gates them). Baseline mismatches are expected: B0/B1 remove the checks a scenario is meant to catch. Ablation differences are the hypothesis under test, reported even when the hypothesis is rejected.
<!-- gen:results-end -->

## Limits
<!-- gen:limits -->
<!-- claims-ok: negated -->
- No certified functional safety. GUARD claims no compliance with ISO 13849, IEC 61508, ISO 3691-4 or IEC 62304, and no SIL or PL rating.
- No real actuators, braking or vehicles. This is a simulation-only prototype.
- No hard real-time guarantee on hardware. QEMU timing is not hardware timing.
- No formal verification. Assurance comes from unit, scenario and property tests only.
- No prediction of obstacle motion. The stopping-distance check treats each observation as a snapshot, so a pedestrian who walks into the braking path after a lease is granted can still reach the braking robot (#215).
- No defence against a compromised kernel, supervisor or gate. Only the planner is treated as untrusted.
- No QNX performance claims and no QNX-vs-Linux comparisons without BlackBerry's written approval.
<!-- gen:limits-end -->

## Repository map
| Path | Purpose |
|---|---|
| `contracts/` | Frozen interface contract (`contracts-v1.2`) and schemas |
| `core/` | Portable C++17 logic. No QNX calls |
| `qnx/` | QNX adapters. No business logic |
| `host/` | Deterministic host replay harness |
| `scenarios/` | Acceptance scenarios F0-F8 |
| `eval/` | Host evaluation matrix and report |
| `dashboard/` | Read-only telemetry viewer |
| `image/` | `mkqnximage` scripts. No images are committed |
| `evidence/` | Target and host evidence bundles |
| `docs/` | Architecture, decisions, limits, setup |
| `demo/`, `docs/video/` | Demo script, storyboard and captions |

## How we work
- [AGENTS.md](AGENTS.md): rules for humans and AI agents
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [DECISIONS.md](DECISIONS.md): the decision log
- [docs/architecture.md](docs/architecture.md)
- [docs/limits.md](docs/limits.md)

## AI-assistance disclosure
Three people build this project with AI coding assistants. Assistants wrote
code, tests, checks and docs, and opened and merged pull requests. After the
initial bootstrap commit, every change reached `main` through a pull request
merged by its author once the required CI checks passed. Pull requests have no
required human review; a fresh-context agent self-review takes its place (D12,
D26). The humans set the rules and own the design decisions in `DECISIONS.md`. No
result in this README was generated by a model unprompted: the numbers come from
the Linux host harness, and target bundles are transcripts of runs on our own
machines.

## Team

| Stream | Person | Scope |
|---|---|---|
| A | @phatdat220707 | Portable core, host harness, evaluation, dashboard, advisor, scenarios |
| B | @baofrovn | QNX processes, image, target evidence, setup |
| P | @dngvmnh | README, docs, demo, video, release, governance (product owner) |

## Licence
MIT. The QNX SDP is not included or redistributed. Each developer installs it
under their own non-commercial licence; CI never touches QNX.

The optional LLM advisor (`host/ADVISOR.md`) uses
[Qwen2.5-1.5B-Instruct](https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct/blob/main/LICENSE)
(Apache-2.0) through a local Ollama service. The model weights are never committed
or redistributed.
