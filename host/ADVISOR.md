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
tests are not evidence of an actual LLM run. QNX connection wiring belongs to #49.

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
ignored local `out/s7-1p5b/`; other laptops are pending.
The earlier 3B experiments are historical only. This remains a stationary-pose
host experiment; target integration is pending.
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
and QNX acceptance remain pending.


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
