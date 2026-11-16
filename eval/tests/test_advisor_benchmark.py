import asyncio
import json
import runpy
from pathlib import Path
from types import SimpleNamespace

import pytest


def test_spike_keeps_timeout_in_latency_population(tmp_path, monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[2] / "host"))
    module = runpy.run_path(str(Path(__file__).parents[2] / "host/benchmark_advisor.py"))

    class Advisor:
        def __init__(self, *_args, **_kwargs):
            self.calls = 0

        async def reply(self, _request):
            self.calls += 1
            if self.calls == 3:
                raise asyncio.TimeoutError("simulated timeout")
            return {"waypoints": [[5, 0]]}

    ticks = iter([0, 0.1, 0.1, 1.1, 1.1, 31.1])
    globals_ = module["benchmark"].__globals__
    monkeypatch.setitem(globals_, "OllamaAdvisor", Advisor)
    monkeypatch.setitem(globals_, "time", SimpleNamespace(perf_counter=lambda: next(ticks)))
    monkeypatch.setitem(
        globals_, "subprocess", SimpleNamespace(check_output=lambda *_args, **_kwargs: "test-sha\n")
    )
    request = {
        "v": 1,
        "type": "obs_request",
        "req_id": 1,
        "goal": [5, 0],
        "obs": {"obs_id": 1, "stamp_ns": 0, "x": 0, "y": 0, "theta": 0, "map_version": 1},
    }
    destination = tmp_path / "measurements"
    assert asyncio.run(module["benchmark"](request, destination, 2, "test-model")) == 1
    summary = json.loads((destination / "summary-internal.json").read_text())
    assert summary["warm_samples"] == 2
    assert summary["warm_successes"] == 1
    assert summary["median_s"] == pytest.approx(15.5)
    assert summary["p95_s"] == pytest.approx(28.55)
    assert summary["samples"][-1]["success"] is False
