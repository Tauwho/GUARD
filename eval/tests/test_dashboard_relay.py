import asyncio
import json
import runpy
import shutil
import subprocess
import threading
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import urlopen

import jsonschema
import pytest

API = runpy.run_path(str(Path(__file__).parents[2] / "dashboard/relay.py"))


def test_replay_rejects_invalid_telemetry_and_sorts_virtual_time(tmp_path):
    path = tmp_path / "events.jsonl"
    events = [
        {"v": 1, "t_ns": stamp, "src": "harness", "seq": index, "kind": "status", "payload": {}}
        for index, stamp in enumerate((20, 10))
    ]
    path.write_text("\n".join(map(json.dumps, events)), encoding="utf-8")
    assert [event["t_ns"] for event in API["load_events"](path)] == [10, 20]
    events[0]["v"] = 99
    path.write_text(json.dumps(events[0]), encoding="utf-8")
    with pytest.raises(jsonschema.ValidationError):
        API["load_events"](path)


def test_http_serves_only_static_allowlist_and_metadata():
    server = ThreadingHTTPServer(("127.0.0.1", 0), API["http_handler"]({"source": "test"}))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    address = f"http://127.0.0.1:{server.server_port}"
    try:
        with urlopen(address + "/meta.json") as response:
            assert json.load(response) == {"source": "test"}
        with urlopen(address + "/") as response:
            assert b"Simulation-only prototype" in response.read()
            assert "default-src 'self'" in response.headers["Content-Security-Policy"]
        with pytest.raises(HTTPError) as error:
            urlopen(address + "/../AGENTS.md")
        assert error.value.code == 404
    finally:
        server.shutdown()
        server.server_close()


def test_live_bridge_is_receive_only_and_validates_records():
    async def exercise():
        received = []
        upstream = []
        event = {"v": 1, "t_ns": 123, "src": "gate", "seq": 1, "kind": "status", "payload": {}}

        class Connection:
            async def send(self, message):
                received.append(json.loads(message))

        async def bridge(reader, writer):
            writer.write(json.dumps(event).encode() + b"\n")
            await writer.drain()
            try:
                upstream.append(await asyncio.wait_for(reader.read(1), 0.1))
            except asyncio.TimeoutError:
                upstream.append(b"")
            writer.close()
            await writer.wait_closed()

        async with await asyncio.start_server(bridge, "127.0.0.1", 0) as server:
            await API["stream_bridge"](
                Connection(), "127.0.0.1", server.sockets[0].getsockname()[1]
            )
        assert received == [{"type": "events", "events": [event]}, {"type": "end"}]
        assert upstream == [b""]

    asyncio.run(exercise())


def test_live_bridge_drops_bad_lines_and_continues_valid_stream():
    async def exercise():
        received = []
        event = {"v": 1, "t_ns": 1, "src": "gate", "seq": 1, "kind": "status", "payload": {}}

        class Connection:
            async def send(self, message):
                received.append(json.loads(message))

        async def bridge(reader, writer):
            writer.write(b"bad json\n" + json.dumps({**event, "v": 99}).encode() + b"\n")
            writer.write(
                b'{"v":1,"t_ns":1,"src":"gate","seq":1,"kind":"status","payload":{"n":NaN}}\n'
            )
            writer.write(json.dumps(event).encode() + b"\n")
            await writer.drain()
            writer.close()
            await writer.wait_closed()

        async with await asyncio.start_server(bridge, "127.0.0.1", 0) as server:
            await API["stream_bridge"](
                Connection(), "127.0.0.1", server.sockets[0].getsockname()[1]
            )
        assert (
            sum(message.get("count", 0) for message in received if message["type"] == "drop") == 3
        )
        assert received[-2] == {"type": "events", "events": [event]}
        assert received[-1] == {"type": "end"}

    asyncio.run(exercise())


def test_headless_ui_seek_lease_source_arc_and_reason_feed():
    node = shutil.which("node")
    if not node:
        pytest.skip("Node is needed for the headless UI smoke")
    result = subprocess.run(
        [node, str(Path(__file__).parents[2] / "dashboard/tests/smoke.cjs")],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 0, result.stderr
    assert "headless smoke: PASS" in result.stdout


def test_chunked_oversized_line_discards_its_json_tail():
    async def exercise():
        event = {"v": 1, "t_ns": 1, "src": "gate", "seq": 1, "kind": "status", "payload": {}}
        reader = asyncio.StreamReader(limit=16)
        reader.feed_data(b"x" * 32)
        pending = asyncio.create_task(API["bridge_line"](reader))
        await asyncio.sleep(0)
        reader.feed_data(json.dumps(event).encode() + b"\n")
        line, invalid, ended = await pending
        assert invalid and not ended
        reader.feed_data(b"next\n")
        assert await API["bridge_line"](reader) == (b"next\n", False, False)

    asyncio.run(exercise())


def test_bridge_rejects_browser_infinite_integer():
    validator = jsonschema.Draft202012Validator(
        json.loads((Path(__file__).parents[2] / "contracts/telemetry.schema.json").read_text())
    )
    event = {"v": 1, "t_ns": 10**400, "src": "gate", "seq": 1, "kind": "status", "payload": {}}
    with pytest.raises(ValueError, match="browser number"):
        API["decode_event"](json.dumps(event), validator)
