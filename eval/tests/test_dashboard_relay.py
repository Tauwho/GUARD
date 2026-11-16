import json
import runpy
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
