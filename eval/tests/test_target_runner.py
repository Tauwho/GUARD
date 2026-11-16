import io
import json
import runpy
import sys
import threading
from pathlib import Path

import pytest

ROOT = Path(__file__).parents[2]
API = runpy.run_path(str(ROOT / "eval/target_runner.py"))
BINARY = (
    ROOT
    / "build/host-debug/host"
    / ("guard-replay-core.exe" if sys.platform == "win32" else "guard-replay-core")
)


@pytest.mark.parametrize("scenario,final", [("F0", "GOAL"), ("F3", "LATCHED")])
def test_local_mock_drives_c14_and_validates_actual_core(tmp_path, scenario, final):
    if not BINARY.exists():
        pytest.skip("build host-debug first for real core integration")
    output = tmp_path / "trace.jsonl"
    result = API["run_case"]("local", ROOT / f"scenarios/{scenario}.yaml", output)
    assert result["mismatches"] == []
    assert result["summary"]["final_state"] == final
    assert result["source"] == "local virtual-time mock"
    assert result["commands"][0][1:3] == ["scenario", "load"]
    assert result["commands"][1] == ["guardctl", "scenario", "start"]
    assert result["commands"][-1] == ["guardctl", "scenario", "stop"]
    if scenario == "F3":
        assert ["guardctl", "inject", "F3"] in result["commands"]


def test_ssh_rejects_options_remote_hosts_and_keeps_shell_arguments_separate(tmp_path, monkeypatch):
    with pytest.raises(ValueError):
        API["SshTarget"]("-oProxyCommand=bad", "localhost:7700", tmp_path / "trace")
    with pytest.raises(ValueError):
        API["SshTarget"]("guard-target", "example.com:7700", tmp_path / "trace")
    calls = []

    def run(command, **kwargs):
        calls.append(command)
        return type("Result", (), {"returncode": 0})()

    monkeypatch.setattr(API["subprocess"], "run", run)
    target = API["SshTarget"]("guard-target", "localhost:7700", tmp_path / "trace")
    target.control("scenario", "load", "/path/a b;echo.yaml")
    assert calls[0][-1] == "guardctl scenario load '/path/a b;echo.yaml'"
    assert "BatchMode=yes" in calls[0]


def test_local_collection_failure_still_stops_scenario(tmp_path, monkeypatch):
    adapter = API["LocalTarget"](None, 0, "B2", tmp_path / "trace")

    def fail():
        raise ValueError("capture failed")

    monkeypatch.setattr(adapter, "collect", fail)
    monkeypatch.setitem(API["run_case"].__globals__, "LocalTarget", lambda *args: adapter)
    with pytest.raises(ValueError, match="capture failed"):
        API["run_case"]("local", ROOT / "scenarios/F0.yaml", tmp_path / "trace")
    assert adapter.commands[-1] == ["guardctl", "scenario", "stop"]


def test_trace_rejects_incomplete_run(tmp_path):
    path = tmp_path / "trace"
    path.write_text(
        json.dumps({"v": 1, "t_ns": 0, "src": "gate", "seq": 1, "kind": "status", "payload": {}})
    )
    with pytest.raises(ValueError, match="complete"):
        API["validate_trace"](path, {"id": "F0"}, "B2")


def test_bridge_capture_skips_background_and_reports_corrupt_stream(tmp_path):
    background = {"kind": "status"}
    start, end = {"kind": "run_start"}, {"kind": "run_end"}

    class Connection:
        def makefile(self, mode):
            return io.BytesIO(b"\n".join(json.dumps(e).encode() for e in (background, start, end)))

    path = tmp_path / "trace"
    adapter = API["SshTarget"]("guard-target", "localhost:7700", path)
    errors, finished = [], threading.Event()
    adapter.collect(Connection(), errors, finished)
    assert finished.is_set() and not errors
    assert [json.loads(line) for line in path.read_text().splitlines()] == [start, end]

    class BadConnection:
        def makefile(self, mode):
            return io.BytesIO(b"bad json\n")

    adapter.collect(BadConnection(), errors, finished)
    assert errors


def test_failed_load_does_not_stop_someone_elses_run(tmp_path, monkeypatch):
    adapter = API["LocalTarget"](None, 0, "B2", tmp_path / "trace")

    def refuse(*args):
        adapter.commands.append(list(args))
        raise ValueError("EBUSY")

    monkeypatch.setattr(adapter, "control", refuse)
    monkeypatch.setitem(API["run_case"].__globals__, "LocalTarget", lambda *args: adapter)
    with pytest.raises(ValueError, match="EBUSY"):
        API["run_case"]("local", ROOT / "scenarios/F0.yaml", tmp_path / "trace")
    assert len(adapter.commands) == 1
    assert adapter.commands[0][:2] == ["scenario", "load"]


def test_fault_duration_comes_from_c11_top_level():
    assert API["injection"]({"fault": "F7", "duration_ms": 1234, "params": {}}) == [
        "inject",
        "F7",
        "--duration-ms",
        "1234",
    ]
