# C9 advisory route prototype

D22 permits P1 advisor work after host F0/F1/F3 pass. Those runs pass on Linux
(issue #135). Motion permission still requires the normal supervisor/gate path.

The portable route checker uses a retained supervisor-owned source observation,
current sensor/map state, a maximum route age of 5 seconds and inflated active
restricted zones. A usable route does not authorize a motor command.

D24: `Supervisor::admit_route` uses a separate provenance ring whose active capacity
is `ceil(route_max_age_ms * obs_rate_hz / 1000)` (100 by default); C1 history stays
64. Age uses the current observation stamp minus the retained source stamp.
The bounded backing array supports all valid configurations (up to 6000 metadata
entries); the supervisor object reserves roughly 192 KiB more storage even at the
default capacity. Target adapters should provision this object outside small thread
stacks. No heap allocation occurs in observation or route admission.
D26 cuts binary RouteProposal transport: the planner checks advisory routes with
`guard::check_route`, then emits ordinary C1 motion proposals.

## Recorded response mode

```sh
python host/llm_advisor.py
python host/llm_advisor.py --recorded out/advisor-recording.jsonl
```

Each JSONL record has `request` and `response` objects exactly per C9, `delay_ns`
(integer, 0..60 seconds) and `source` (`ollama-recording` or `synthetic-test`).
The service listens only on localhost:7701, preserves response fields and recorded
delays, and requires an exact request match. Every connection starts at record zero.
It never updates an old observation stamp to make a replay look fresh. Synthetic
tests are not evidence of an actual LLM run. QNX wiring: see "On the target" below.

For a retained single-record JSON object from an actual call, run the same core checker:

```sh
build/host-debug/host/guard-route-demo out/runtime-inputs/maps/mri_suite.yaml out/runtime-inputs/F6.yaml out/one-advisor-record.json
```

The demo keeps the robot at the retained pose while the advisor computes and advances
the real scenario World from the request stamp to the computed response stamp. It
reports the actual map/age rejection. Exit 0 means the advisory route is usable,
1 means rejected, 2 means an input error. Motion is never authorized by this tool.

## Live model and clock status

The C9 clock question was answered in #3: creation time is the source observation
stamp plus host elapsed computation time, never an absolute host clock reading.
The adapter implements that rule and can record actual calls for later replay:

```sh
python host/llm_advisor.py --ollama --model qwen2.5:1.5b-instruct --record out/advisor-recording.jsonl
```

This requires an already-running local Ollama service with the model installed.
Requests must include a goal. Response waiting is bounded to 30 seconds; inference
uses two CPU threads with GPU layers disabled for the host CPU spike.
The D23-selected 1.5B model was measured on Dat's CPU (two threads, no GPU):
one warmup plus 20 goal-reaching valid samples. Actual retained responses were
passed through the D24 supervisor API with World/F6: changed-map responses reject
as `REJ_MAP_VERSION`, while the cold/slow response rejects as stale. Live C9 TCP
preserves the goal/provenance; recorded TCP replay preserves the exact response
and delay across fresh connections. Numeric timings and raw recordings remain in
ignored local `out/s7-1p5b/`; inference on other laptops is optional.
The earlier 3B experiments are historical only. The measurements above are a
stationary-pose host experiment; target integration is described below.
The route
checker treats creation time as opaque metadata and derives age from its own source
observation. The recorded service never synthesizes a new creation time.

D23 selects [Qwen2.5-1.5B-Instruct (Apache-2.0)](https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct/blob/main/LICENSE).
The shared default is `DEFAULT_MODEL` in `host/ollama_route.py`; both CLIs use it
and accept `--model` overrides. Pull `qwen2.5:1.5b-instruct` locally. Weights are never committed or redistributed.
The earlier 3B experiment is historical only; 3B is excluded by D23.

## S7 reproduction

Run with a retained C9 request (including a goal) and a fresh output directory:

```sh
python host/benchmark_advisor.py --request out/advisor-request.json --out out/s7-new --samples 20
```

One warmup is excluded from median/p95. Failed requests remain in the samples; the
script stops after two failures and exits non-zero. Numeric timing files stay local
and internal. Keep recordings for deterministic replay; do not rely on the live
response always arriving within the route-age limit. Other members' laptop measurements
are optional; QNX acceptance belongs to stream B.


The default is the bundled `host/fixtures/advisor-recorded.jsonl`, explicitly
`synthetic-test`: a deterministic wiring/demo fixture, not an actual model run or
latency measurement. `--trace` remains an alias for `--recorded`. Real model data
is retained locally as described above. Send the exact fixture request to TCP 7701.

For explicit live mode with a recording fallback:
`python host/llm_advisor.py --ollama --fallback-recorded out/advisor-recording.jsonl`.
Only timeout/unavailable service errors fall back, and only for an exact recorded
request. Invalid model output is not hidden. The original response and delay remain
unchanged; a response whose source expired during the live timeout is still rejected
by the core. Recorded default mode works without Ollama or a network connection.

## On the target (#49)

`guard-planner --remote IP:PORT` adds an advisor thread to the QNX planner. It sends
a C9 `obs_request` (the planner's latest observation, pose rounded to millimetres,
and the goal) with one request in flight. Socket waits are bounded and happen only
on that thread; the C1 loop never waits for the host. Each reply goes to a one-route
mailbox. The main loop then:

1. drops waypoints the robot reached or passed while the model was computing
   (`guard::trim_route`), so the checked polyline is the one that will be followed;
2. runs `guard::check_route` against the planner's own retained source observation
   (a 128-entry ring, enough for the 5 s route age at 20 Hz), its latest observation,
   and the map version and zone state read from `/dev/sim/world`;
3. on a usable route, replaces only the waypoints that `Planner::propose` steers by.

Every motion command is still a C1 proposal that needs a supervisor decision and a
gate lease. A map change sends the planner back to the static route. `check_route`
also rejects a segment through an inflated wall (`REJ_STOPPING_DISTANCE`), so the
planner does not follow a model route into a wall where the supervisor would only
stop it. Each checked route is a C8 `proposal` event with `route` (`USABLE` or
`REJECTED`) and `route_reason`.

```sh
python host/llm_advisor.py --ollama            # host side, 127.0.0.1:7701
image/run-demo.sh                              # develop image; user networking reaches the host as 10.0.2.2
ssh guard-target 'slay -f guard-planner; on -d /system/bin/guard-planner --remote 10.0.2.2:7701 > /tmp/planner.log 2>&1'
python eval/target_runner.py --target ssh:guard-target --bridge 127.0.0.1:7700 --scenario scenarios/F6.yaml --out out/target-F6.jsonl
```

Evidence: [remote-planner](../evidence/20261119-36f0bf2-remote-planner/manifest.json)
and [llm-route](../evidence/20261119-36f0bf2-llm-route/manifest.json) (one F6 B2 run,
behaviour only). On the target, the route based on map 1 that arrived after the MRI
zone activated was rejected `REJ_MAP_VERSION`. Other routes were rejected for the
restricted zone or age, or were followed. The run had 0 contacts, 0 zone breaches
and 0 latches.

Limits of this mode:

- Remote mode ignores the scenario's planner mode (`shortcut`, `aggressive`). Under
  `--remote`, F6 ends at GOAL where the scenario file expects READY. The supervisor
  may also never see the C1 `REJ_MAP_VERSION` that F6 requires, because the planner
  rejects the stale route itself. The runner reports these two mismatches; both are
  expected in this mode.
- Live inference shares the host CPU with QEMU. A cold model can delay the first
  route past the 5 s route age, and in our runs it once starved the priority-10
  planner long enough for the gate to stop and latch. Warm the model before booting.
  Which routes are rejected, and for which reason, depends on inference time, so a
  live run is not deterministic; the recorded mode above is.
- Remote mode on the `--secpol secure` image is not verified. Root cannot slay the
  boot planner there (by design), and one try with a second `on -T planner_t`
  planner made no advisor connection.


## Verification

[A9 acceptance decision](https://github.com/Tauwho/GUARD/issues/29#issuecomment-6921570197)
replaces the physical second-laptop requirement with these two evidence sources:

- Actual inference on Dat's laptop: Qwen2.5-1.5B, one warmup plus 20
  schema-valid, goal-reaching samples, two CPU threads and no GPU. Live C9 TCP
  and recorded replay preserve the response and provenance. Retained real
  responses reject through World/F6 when the map changes or the source expires.
  Internal recordings and metadata remain in ignored `out/s7-1p5b/`.
- Cross-environment offline/recorded verification: the successful Linux
  [main `eval` job](https://github.com/Tauwho/GUARD/actions/runs/43895106219/job/133396405455)
  at `b3c10273c1e95b0c8371ed28116bd77dba63b072` tests the recorded advisor,
  timeout fallback and provenance preservation. This is offline/recorded
  verification, not inference on a second machine.

Numeric timings remain internal (D11). A9 remains MANUAL in the frozen scope
checker; the linked PO decision defines its acceptance. Another physical laptop
is optional and does not block A9.

## Optional inference on another laptop

No private request file or teammate handoff is needed. On another laptop,
use its own local Ollama service and pull the D23 model, then run:

```sh
ollama pull qwen2.5:1.5b-instruct
python host/benchmark_advisor.py --out out/s7-second-laptop --samples 20
```

The benchmark defaults to the bundled demo request; `--request <C9-JSON>` remains
available. The synthetic input task is not an inference result: new outputs are
recorded as actual Ollama calls. The output directory must be new. Keep timing
files/internal raw recordings local; retain failed calls, commit and platform
metadata. Use `--ollama --fallback-recorded <recording.jsonl>` to verify exact
recorded fallback separately, without freshening provenance. A label or a repeated
run on the same laptop is not second-device evidence. Add actual additional-device
results to Verification only if that optional experiment is performed.
