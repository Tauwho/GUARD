import asyncio
import json
import runpy
from pathlib import Path

import jsonschema
import pytest


def api(monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[2] / "host"))
    return runpy.run_path(str(Path(__file__).parents[2] / "host/ollama_route.py"))


def request():
    return {
        "v": 1,
        "type": "obs_request",
        "req_id": 7,
        "goal": [5, 0],
        "obs": {"obs_id": 2, "stamp_ns": 123, "x": 0, "y": 0, "theta": 0, "map_version": 1},
    }


def test_live_response_uses_source_stamp_plus_elapsed_and_records_unchanged_source(
    tmp_path, monkeypatch
):
    module = api(monkeypatch)
    ticks = iter([10, 600_000_010])
    recording = tmp_path / "actual-adapter-recording.jsonl"
    advisor = module["OllamaAdvisor"](
        generator=lambda _request: {"waypoints": [[5, 0]]},
        clock=lambda: next(ticks),
        record_path=recording,
    )
    response = asyncio.run(advisor.reply(request()))
    assert response["created_ns"] == 600_000_123
    assert response["based_on"] == {"obs_id": 2, "stamp_ns": 123, "map_version": 1}
    assert response["req_id"] == 7
    # This uses a fake generator; it checks the adapter, not a real model output.
    saved = json.loads(recording.read_text())
    assert saved["delay_ns"] == 600_000_000
    assert saved["response"] == response


def test_model_cannot_choose_provenance_or_unbounded_waypoints(monkeypatch):
    module = api(monkeypatch)
    for output in (
        {"waypoints": [[5, 0]], "based_on": {"stamp_ns": 999}},
        {"waypoints": [[101, 0]]},
    ):
        advisor = module["OllamaAdvisor"](generator=lambda _request: output, clock=lambda: 1)
        with pytest.raises(jsonschema.ValidationError):
            asyncio.run(advisor.reply(request()))


def test_ollama_http_request_is_local_bounded_and_schema_constrained(monkeypatch):
    module = api(monkeypatch)
    called = []

    class Response:
        def __enter__(self):
            return self

        def __exit__(self, *_args):
            pass

        def read(self, limit):
            assert limit == 1_048_577
            return json.dumps({"done": True, "response": '{"waypoints":[[5,0]]}'}).encode()

    def urlopen(message, timeout):
        called.append((message, timeout))
        return Response()

    monkeypatch.setitem(module["OllamaGenerator"].__call__.__globals__, "urlopen", urlopen)
    result = module["OllamaGenerator"]()(request())
    assert result == {"waypoints": [[5, 0]]}
    message, timeout = called[0]
    assert message.full_url == "http://127.0.0.1:11434/api/generate"
    body = json.loads(message.data)
    assert body["stream"] is False
    assert body["format"]["additionalProperties"] is False
    assert body["options"]["num_gpu"] == 0
    assert timeout == 30
