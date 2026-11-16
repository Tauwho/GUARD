"""Missing clang-format is SKIPPED locally and FAIL in CI (audit fix 8)."""

import importlib.util
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]


def load(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


check = load("check")
check_format = load("check_format")


def run_without_clang_format(env_extra):
    env = {"PATH": str(Path(sys.executable).parent), "SYSTEMROOT": "C:\\Windows", **env_extra}
    probe = (
        "import shutil, runpy, sys; shutil.which = lambda name, *a, **k: None; "
        f"sys.argv = ['check_format.py']; runpy.run_path(r'{TOOLS / 'check_format.py'}', "
        "run_name='__main__')"
    )
    return subprocess.run([sys.executable, "-c", probe], env=env, capture_output=True, text=True)


def test_missing_clang_format_is_skipped_locally():
    done = run_without_clang_format({})
    assert done.returncode == check_format.MISSING
    assert "SKIPPED" in done.stdout
    assert check.stage_status("format", done.returncode) == "SKIPPED"


def test_missing_clang_format_fails_in_ci():
    done = run_without_clang_format({"CI": "true"})
    assert done.returncode == 1
    assert check.stage_status("format", done.returncode) == "FAIL"


def test_only_the_format_stage_can_be_skipped():
    assert check.stage_status("pytest", check_format.MISSING) == "FAIL"
    assert check.stage_status("format", 1) == "FAIL"
    assert check.stage_status("format", 0) == "PASS"
