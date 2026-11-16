# C9 advisory route prototype

D22 permits P1 advisor work after host F0/F1/F3 pass. Those runs pass on Linux
(issue #135). Motion permission still requires the normal supervisor/gate path.

The portable route checker uses a retained supervisor-owned source observation,
current sensor/map state, a maximum route age of 5 seconds and inflated active
restricted zones. A usable route does not authorize a motor command.

## Recorded response mode

```sh
python host/llm_advisor.py --trace out/advisor-recording.jsonl
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
python host/llm_advisor.py --ollama --model qwen2.5:3b --record out/advisor-recording.jsonl
```

This requires an already-running local Ollama service with the model installed.
Requests must include a goal. Response waiting is bounded to 30 seconds; inference
uses two CPU threads with GPU layers disabled for the host CPU spike.
Actual local CPU inference has been verified on Dat's host (S7, #29). The original
prompt missed the goal; the revised short prompt plus endpoint validation produced
goal-reaching routes. Actual recorded responses were passed through World/F6 and
the core checker: changed-map responses are rejected; slower responses reject as stale.
This is a stationary-pose host experiment, not target integration. The route
checker treats creation time as opaque metadata and derives age from its own source
observation. The recorded service never synthesizes a new creation time.

The named model [Qwen2.5-3B-Instruct uses the Qwen RESEARCH LICENSE](https://huggingface.co/Qwen/Qwen2.5-3B-Instruct/blob/main/LICENSE),
which limits use to non-commercial purposes and calls for a separate licence for
commercial use. It is not Apache-2.0. Model weights are not included or redistributed
in this repo. The current work is a simulation/research prototype.

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
