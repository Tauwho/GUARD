# Host replay

Build with `cmake --preset host-debug` and `cmake --build --preset host-debug`.
Install the pinned Python dependencies used by `.github/workflows/ci.yml`
(PyYAML 6.0.3 and jsonschema 4.26.0 for this CLI).

Run from the repository root:

```sh
python host/guard-replay --scenario scenarios/F0.yaml --seed 1000 --mode B2 --out out/F0.jsonl
```

The Python entry point validates YAML, normalizes buffers for the portable loaders,
runs `guard-replay-core`, validates every C8 telemetry record and checks the scenario's
expected outcomes. Exit codes: 0 expectations met; 1 actual outcome differs; 2 input,
model or execution error. Use `--binary` for another build directory.
On Windows with a portable compiler, put that compiler's runtime directory on PATH.

The engine runs the actual core classes with injected virtual time. F2 is an abstract
supervisor stall; F3 closes its gate session and stops supervisor decisions/rechecks.
F7 registers a replacement supervisor, then submits A's old lease after the configured
pause. These host models do not test QNX scheduling or process wiring.
B0 and B1 command the gate directly, without supervisor decisions. `--mode` also accepts
`B2-freshness`, `B2-stopping`, `B2-expiry`, `B2-epoch`; these translate into the shared
C12 configuration flags. Alternatively use `--config path.yaml`, with the same file
as the target binaries. Ablation outcomes are compared with the B2 reference hypotheses,
so a mismatch can be an expected consequence of removing that check. Full matrix: #23.

The seed is recorded for reproducibility; current scenario timelines have no random
perturbations, so different seeds currently produce the same physical trajectory.
Do not present those runs as independent randomized trials.

For target image packaging, normalize inputs on the host before copying them:

```sh
python host/prepare_inputs.py --out out/runtime-inputs
```

This preserves the scenario/map `.yaml` paths but writes JSON-subset YAML, which
the same portable buffer loaders consume without PyYAML on the target. It also
emits a default `config.yaml` (B2) or the file given by `--config`, plus SHA256 hashes
in `inputs.json`. Copy the generated files, not the original block-style YAML,
into the image's scenario directory. Stream B owns the packaging/wiring change.

For the nominal F0 run, the reported `false_reject_rate` is operationally defined as
rejected proposals divided by all proposals from the nominal rule planner. It is not
a separate correctness oracle. `stop_time_s` measures fault onset to the first STOPPING
transition, not time to physical standstill. Raw JSONL remains available for inspection.
