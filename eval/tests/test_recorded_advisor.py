import asyncio
import copy
import json
import runpy
import subprocess
import sys
from pathlib import Path

import pytest

API = runpy.run_path(str(Path(__file__).parents[2] / "host/llm_advisor.py"))


@pytest.fixture(autouse=True)
def host_imports(monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[2] / "host"))


def record():
    return {
        "request": {
            "v": 1,
            "type": "obs_request",
            "req_id": 1,
            "obs": {"obs_id": 1, "stamp_ns": 123, "x": 0, "y": 0, "theta": 0, "map_version": 1},
        },
        "response": {
            "v": 1,
            "type": "route",
            "req_id": 1,
            "route_id": 1,
            "based_on": {"obs_id": 1, "stamp_ns": 123, "map_version": 1},
            "waypoints": [[5, 0]],
            "created_ns": 999,
        },
        "delay_ns": 600_000_000,
        "source": "synthetic-test",
    }


def test_recorded_reply_preserves_original_fields_and_delay(tmp_path):
    item = record()
    path = tmp_path / "recording.jsonl"
    path.write_text(json.dumps(item), encoding="utf-8")
    loaded = API["load_recording"](path)
    delays = []

    async def sleep(seconds):
        delays.append(seconds)

    advisor = API["RecordedAdvisor"](loaded, sleep)
    assert asyncio.run(advisor.reply(item["request"])) == item["response"]
    assert delays == [0.6]
    assert loaded[0]["response"]["based_on"]["stamp_ns"] == 123
    with pytest.raises(ValueError, match="exhausted"):
        asyncio.run(advisor.reply(item["request"]))


def test_recording_cannot_be_relabelled_with_a_new_observation(tmp_path):
    item = record()
    path = tmp_path / "bad.jsonl"
    item["response"]["based_on"]["map_version"] = 2
    path.write_text(json.dumps(item), encoding="utf-8")
    with pytest.raises(ValueError, match="source provenance"):
        API["load_recording"](path)
    item = record()
    changed = copy.deepcopy(item["request"])
    changed["obs"]["obs_id"] = 2
    with pytest.raises(ValueError, match="differs from recording"):
        asyncio.run(API["RecordedAdvisor"]([item]).reply(changed))


def test_recorded_tcp_service_returns_the_exact_c9_route():
    async def exercise():
        item = record()
        item["delay_ns"] = 0
        async with await asyncio.start_server(
            lambda reader, writer: API["serve_client"](reader, writer, [item]), "127.0.0.1", 0
        ) as server:
            reader, writer = await asyncio.open_connection(
                "127.0.0.1", server.sockets[0].getsockname()[1]
            )
            writer.write((json.dumps(item["request"]) + "\n").encode())
            await writer.drain()
            reply = await asyncio.wait_for(reader.readline(), 1)
            assert json.loads(reply) == item["response"]
            writer.close()
            await writer.wait_closed()

    asyncio.run(exercise())


@pytest.mark.parametrize("text", ['{"v":1,"v":2}', '{"x":NaN}', '{"x":1e999}'])
def test_non_json_and_duplicate_fields_are_rejected(text):
    with pytest.raises(ValueError):
        API["strict_json"](text)


def test_recorded_is_default_and_trace_alias_stays_compatible(monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[2] / "host"))
    defaults = API["parse_args"]([])
    assert not defaults.ollama
    assert defaults.trace == API["DEFAULT_RECORDING"]
    assert API["load_recording"](defaults.trace)[0]["source"] == "synthetic-test"
    assert API["parse_args"](["--trace", "custom.jsonl"]).trace == Path("custom.jsonl")
    assert API["parse_args"](["--ollama"]).trace is None


def test_timeout_fallback_preserves_exact_response_and_refuses_new_provenance():
    from ollama_route import ModelUnavailableError

    class Live:
        async def reply(self, request):
            raise ModelUnavailableError("transport timeout")

    item = record()
    item["delay_ns"] = 0
    advisor = API["FallbackAdvisor"](Live(), [item])
    assert asyncio.run(advisor.reply(item["request"])) == item["response"]
    changed = copy.deepcopy(item["request"])
    changed["obs"]["stamp_ns"] += 1
    with pytest.raises(ValueError, match="no exact"):
        asyncio.run(advisor.reply(changed))


def test_invalid_live_output_is_not_relabelled_as_a_recorded_success():
    class Live:
        async def reply(self, request):
            raise ValueError("invalid route")

    with pytest.raises(ValueError, match="invalid route"):
        asyncio.run(API["FallbackAdvisor"](Live(), [record()]).reply(record()["request"]))


def test_bundled_recorded_tcp_and_core_changed_map_demo(tmp_path):
    root = Path(__file__).parents[2]
    binary = (
        root
        / "build/host-debug/host"
        / ("guard-route-demo.exe" if sys.platform == "win32" else "guard-route-demo")
    )
    if not binary.exists():
        pytest.skip("build host-debug for real core admission demo")
    items = API["load_recording"](API["DEFAULT_RECORDING"])

    async def exchange():
        async with await asyncio.start_server(
            lambda r, w: API["serve_client"](r, w, items), "127.0.0.1", 0
        ) as server:
            reader, writer = await asyncio.open_connection(
                "127.0.0.1", server.sockets[0].getsockname()[1]
            )
            writer.write((json.dumps(items[0]["request"]) + "\n").encode())
            await writer.drain()
            response = json.loads(await asyncio.wait_for(reader.readline(), 2))
            writer.close()
            await writer.wait_closed()
            return response

    assert asyncio.run(exchange()) == items[0]["response"]
    prepare = runpy.run_path(str(root / "host/prepare_inputs.py"))["prepare"]
    inputs = tmp_path / "inputs"
    prepare(root / "scenarios", inputs)
    result = subprocess.run(
        [
            str(binary),
            str(inputs / "maps/mri_suite.yaml"),
            str(inputs / "F6.yaml"),
            str(API["DEFAULT_RECORDING"]),
        ],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 1, result.stderr
    assert json.loads(result.stdout)["reason"] == "REJ_MAP_VERSION"


def test_recording_write_failure_never_triggers_a_stale_fallback(tmp_path):
    from ollama_route import OllamaAdvisor

    item = record()
    item["request"]["goal"] = [5, 0]
    live = OllamaAdvisor(
        lambda request: {"waypoints": [[5, 0]]}, clock=lambda: 100, record_path=tmp_path
    )
    with pytest.raises(OSError):
        asyncio.run(API["FallbackAdvisor"](live, [item]).reply(item["request"]))
