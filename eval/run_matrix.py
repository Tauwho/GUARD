"""Run every requested host case; keep mismatches and execution errors visible."""

import argparse
import hashlib
import json
import platform
import subprocess
import sys
import time
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import jsonschema
import yaml
from metrics import API, measure

ROOT = Path(__file__).resolve().parents[1]
MODES = ("B0", "B1", "B2", "B2-freshness", "B2-stopping", "B2-expiry", "B2-epoch")
VALIDATOR = API["C8Validator"](
    json.loads((ROOT / "contracts/telemetry.schema.json").read_text())
)
MEASURED = {}


def validated_measurement(lines):
    # Every run executes. Only validation/measurement of byte-identical bodies is reused.
    VALIDATOR.validate(json.loads(lines[0]))
    digest = hashlib.sha256("\n".join(lines[1:]).encode()).hexdigest()
    if digest not in MEASURED:
        events = [json.loads(line) for line in lines]
        for event in events:
            VALIDATOR.validate(event)
        MEASURED[digest] = measure(events)
    return MEASURED[digest], digest


def run_case(case):
    scenario, mode, seed, inputs, binary, directory = case
    identity = f"{scenario['id']}-{mode}-{seed}"
    trace = Path(directory) / (identity + ".jsonl")
    command = [
        binary,
        "--scenario",
        inputs["scenario"],
        "--map",
        inputs["map"],
        "--config",
        inputs["config"],
        "--seed",
        str(seed),
        "--out",
        str(trace),
    ]
    record = {
        "scenario": scenario["id"],
        "mode": mode,
        "seed": seed,
        "trace": str(trace),
        "reference": mode.split("-")[0],
    }
    try:
        execution = subprocess.run(command, capture_output=True, text=True, timeout=30)
        if execution.returncode:
            raise ValueError(execution.stderr.strip() or f"exit {execution.returncode}")
        lines = trace.read_text(encoding="utf-8").splitlines()
        summary, digest = validated_measurement(lines)
        record.update(
            summary=summary,
            mismatches=API["mismatches"](scenario["expected"][record["reference"]], summary),
            trajectory_sha256=digest,
        )
    except (
        ValueError,
        OSError,
        IndexError,
        subprocess.SubprocessError,
        jsonschema.ValidationError,
    ) as error:
        record.update(error=str(error), mismatches=["execution or telemetry validation failed"])
    return record


def failed_gate(records, b2_only=False):
    if not records or (b2_only and not any(r["mode"] == "B2" for r in records)):
        return True
    return any(
        "error" in r or (r["mismatches"] and (not b2_only or r["mode"] == "B2")) for r in records
    )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--gate-b2",
        action="store_true",
        help="fail errors/B2 regressions; retain control/ablation mismatches in report",
    )
    parser.add_argument("--seeds", type=int, default=50)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--modes", nargs="+", choices=MODES, default=list(MODES))
    parser.add_argument("--out", type=Path, default=ROOT / "out/matrix")
    parser.add_argument("--binary", type=Path)
    args = parser.parse_args(argv)
    if not 1 <= args.seeds <= 50 or not 1 <= args.jobs <= 16:
        parser.error("seeds must be 1..50 and jobs 1..16")
    binary = args.binary or ROOT / "build/host-debug/host/guard-replay-core"
    if sys.platform == "win32" and not binary.suffix:
        binary = binary.with_suffix(".exe")
    if not binary.is_file():
        parser.error(f"missing built executable {binary}")
    args.out = args.out.resolve()
    inputs = args.out / "inputs"
    traces = args.out / "traces"
    inputs.mkdir(parents=True, exist_ok=True)
    traces.mkdir(exist_ok=True)
    schema = json.loads((ROOT / "contracts/scenario.schema.json").read_text())
    cases = []
    for scenario_path in sorted((ROOT / "scenarios").glob("F*.yaml")):
        scenario = yaml.safe_load(scenario_path.read_text(encoding="utf-8"))
        jsonschema.validate(scenario, schema)
        map_path = ROOT / "scenarios/maps" / (scenario["map"] + ".yaml")
        world = yaml.safe_load(map_path.read_text(encoding="utf-8"))
        jsonschema.validate(world, schema)
        scenario_input = inputs / (scenario["id"] + ".json")
        map_input = inputs / (scenario["map"] + ".json")
        for path, data in ((scenario_input, scenario), (map_input, world)):
            path.write_text(json.dumps(data, allow_nan=False), encoding="utf-8")
        for mode in args.modes:
            config = {"version": 1, "mode": mode.split("-")[0]}
            if "-" in mode:
                config["ablate"] = {mode.split("-")[1]: True}
            config_input = inputs / (mode + ".json")
            config_input.write_text(json.dumps(config), encoding="utf-8")
            paths = {
                "scenario": str(scenario_input),
                "map": str(map_input),
                "config": str(config_input),
            }
            for index in range(args.seeds):
                cases.append(
                    (
                        scenario,
                        mode,
                        scenario["seed"] + index,
                        paths,
                        str(binary.resolve()),
                        str(traces),
                    )
                )
    started = time.monotonic()
    with ProcessPoolExecutor(max_workers=args.jobs) as executor:
        records = list(executor.map(run_case, cases))
    result = {
        "v": 1,
        "source": "host",
        "platform": platform.system(),
        "seed_variation": True,
        "limitations": [
            "Seeds vary staff event times within 100 ms and planner cadence by 0..5 ms; "
            "reference seed/flood cadence unchanged.",
            "Ablations compare with the B2 reference, not an ablation-specific hypothesis.",
            "No target numeric results or QNX timings are included.",
            "Recovery trials are not implemented.",
        ],
        "wall_seconds_internal": time.monotonic() - started,
        "runs": records,
    }
    result["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    destination = args.out / "results.json"
    destination.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    mismatched = sum(bool(record["mismatches"]) for record in records)
    print(
        json.dumps(
            {
                "runs": len(records),
                "mismatched": mismatched,
                "errors": sum("error" in record for record in records),
                "results": str(destination),
            }
        ),
        flush=True,
    )
    return 1 if failed_gate(records, args.gate_b2) else 0


if __name__ == "__main__":
    sys.exit(main())
