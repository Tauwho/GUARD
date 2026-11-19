"""check.py: a missing tool or a hung stage is a FAIL line, never a traceback or a stall.
fresh_clone_demo.sh: an early failure exits nonzero without signalling the caller."""

import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check", TOOLS / "check.py")
check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check)


def run_with(monkeypatch, capsys, stage_error):
    monkeypatch.setattr(sys, "argv", ["check.py", "--fast"])
    monkeypatch.setattr(check, "stages", lambda fast: iter([("fixture", ["fixture-tool"])]))
    seen = []

    def fake_run(command, **kwargs):
        seen.append((command, kwargs.get("timeout")))
        if command == ["fixture-tool"]:
            raise stage_error
        return subprocess.CompletedProcess(command, 0, "", "")

    monkeypatch.setattr(check.subprocess, "run", fake_run)
    code = check.main()
    return code, capsys.readouterr().out, seen


def test_missing_tool_is_a_fail_line(monkeypatch, capsys):
    code, out, _ = run_with(monkeypatch, capsys, FileNotFoundError(2, "No such file", "ctest"))
    assert code == 1
    assert "FAIL fixture: cannot run fixture-tool" in out
    assert "RESULT: RED (fixture)" in out


def test_hung_stage_times_out_as_fail(monkeypatch, capsys):
    error = subprocess.TimeoutExpired(["fixture-tool"], check.STAGE_TIMEOUT)
    code, out, _ = run_with(monkeypatch, capsys, error)
    assert code == 1
    assert "FAIL fixture: timed out" in out
    assert "RESULT: RED (fixture)" in out


def test_every_subprocess_is_bounded(monkeypatch, capsys):
    _, _, seen = run_with(monkeypatch, capsys, OSError("boom"))
    assert seen and all(timeout for _, timeout in seen)


def test_offline_fetch_does_not_stop_the_run(monkeypatch, capsys):
    monkeypatch.setattr(sys, "argv", ["check.py", "--fast"])
    monkeypatch.setattr(check, "stages", lambda fast: iter([("fixture", ["fixture-tool"])]))

    def fake_run(command, **kwargs):
        if command[:2] == ["git", "fetch"]:
            raise subprocess.TimeoutExpired(command, check.FETCH_TIMEOUT)
        return subprocess.CompletedProcess(command, 0, "", "")

    monkeypatch.setattr(check.subprocess, "run", fake_run)
    assert check.main() == 0
    assert "RESULT: GREEN" in capsys.readouterr().out


@pytest.mark.skipif(
    os.name == "nt" or shutil.which("bash") is None, reason="needs POSIX bash process groups"
)
def test_fresh_clone_failure_does_not_signal_the_caller(tmp_path):
    wrapper = tmp_path / "wrapper.sh"
    wrapper.write_text(
        'bash "$SCRIPT" main; code=$?\nprintf "caller survived: %s\\n" "$code"\n',
        encoding="utf-8",
    )
    work = tmp_path / "tmp"
    work.mkdir()
    env = dict(
        os.environ,
        SCRIPT=str(TOOLS / "fresh_clone_demo.sh"),
        GUARD_REPO_URL=str(tmp_path / "no-such-repo"),
        TMPDIR=str(work),
    )
    result = subprocess.run(
        ["bash", str(wrapper)],
        env=env,
        capture_output=True,
        text=True,
        timeout=60,
        start_new_session=True,
    )
    assert "caller survived:" in result.stdout, result.stderr
    assert "caller survived: 0" not in result.stdout
    assert list(work.iterdir()) == []
