import asyncio
import copy
import json
import runpy
from pathlib import Path

import pytest

API = runpy.run_path(str(Path(__file__).parents[2] / "host/llm_advisor.py"))


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
