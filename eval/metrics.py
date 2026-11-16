"""Functional host measurements; no target timing input."""

import runpy
from pathlib import Path

API = runpy.run_path(str(Path(__file__).parents[1] / "host/guard-replay"))


def measure(events):
    result = API["summarize"](events)
    durations = []
    started = None
    for event in events:
        if event["kind"] != "gate_transition":
            continue
        mode = event["payload"]["to"]
        if mode == "STOPPING" and started is None:
            started = event["t_ns"]
        elif mode == "STOPPED" and started is not None:
            durations.append((event["t_ns"] - started) / 1e9)
            started = None
    result["braking_durations_s"] = durations
    result["recovery_correctness"] = None  # Operator recovery trials are not yet implemented.
    return result
