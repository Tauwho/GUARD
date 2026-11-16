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

## Live model and clock status

Live Ollama calls remain pending the C9 `created_ns` clock-domain answer in #3 and
actual local model execution. No live response or target demo is claimed. The route
checker treats creation time as opaque metadata and derives age from its own source
observation. The recorded service never synthesizes a new creation time.

The named model [Qwen2.5-3B-Instruct uses the Qwen RESEARCH LICENSE](https://huggingface.co/Qwen/Qwen2.5-3B-Instruct/blob/main/LICENSE),
which limits use to non-commercial purposes and calls for a separate licence for
commercial use. It is not Apache-2.0. Model weights are not included or redistributed
in this repo. The current work is a simulation/research prototype.
