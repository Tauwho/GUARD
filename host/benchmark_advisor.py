"""S7 host CPU measurements. Keep numeric outputs internal; no QNX runtime is used."""

import argparse
import asyncio
import copy
import json
import platform
import statistics
import subprocess
import time
from pathlib import Path

import jsonschema
from llm_advisor import VALIDATOR, strict_json
from ollama_route import OllamaAdvisor, OllamaGenerator


async def benchmark(request, output, samples, model):
    VALIDATOR.validate(request)
    output.mkdir(parents=True, exist_ok=False)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    advisor = OllamaAdvisor(OllamaGenerator(model), record_path=output / "recording.jsonl")
    measurements = []
    for index in range(samples + 1):
        current = copy.deepcopy(request)
        current["req_id"] = request["req_id"] + index
        started = time.perf_counter()
        try:
            response = await advisor.reply(current)
            result = {
                "sample": index,
                "warmup": index == 0,
                "success": True,
                "waypoints": len(response["waypoints"]),
            }
        except (ValueError, OSError, asyncio.TimeoutError, jsonschema.ValidationError) as error:
            result = {
                "sample": index,
                "warmup": index == 0,
                "success": False,
                "error": type(error).__name__ + ": " + str(error),
            }
        result["elapsed_s"] = time.perf_counter() - started
        measurements.append(result)
        (output / "samples.json").write_text(json.dumps(measurements, indent=2), encoding="utf-8")
        print(f"Sample {index}: {'valid route' if result['success'] else 'failed'}", flush=True)
        if sum(not item["success"] for item in measurements) >= 2:
            break
    durations = [item["elapsed_s"] for item in measurements if not item["warmup"]]
    summary = {
        "internal_only": True,
        "source": "host CPU",
        "platform": platform.system(),
        "commit": commit,
        "model": model,
        "threads": 2,
        "gpu_layers": 0,
        "request": request,
        "samples": measurements,
        "warm_samples": len(durations),
        "requested_warm_samples": samples,
        "warm_successes": sum(item["success"] and not item["warmup"] for item in measurements),
        "latency_population": "all completed warm attempts, including failures/timeouts",
        "median_s": statistics.median(durations) if durations else None,
        "p95_s": statistics.quantiles(durations, n=20, method="inclusive")[-1]
        if len(durations) > 1
        else None,
    }
    (output / "summary-internal.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    return 0 if all(item["success"] for item in measurements) else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=20)
    parser.add_argument("--model", default="qwen2.5:1.5b-instruct")
    args = parser.parse_args()
    if not 2 <= args.samples <= 100:
        parser.error("samples must be 2..100 (plus one excluded warmup)")
    request = strict_json(args.request.read_text(encoding="utf-8"))
    raise SystemExit(asyncio.run(benchmark(request, args.out, args.samples, args.model)))


if __name__ == "__main__":
    main()
