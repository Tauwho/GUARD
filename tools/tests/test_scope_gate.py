"""scope_gate.py runs done-checks as argv lists, identically on Windows and Linux (audit fix 1)."""

import importlib.util
import os
import sys
from pathlib import Path

import pytest
import yaml

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
SPEC = importlib.util.spec_from_file_location("scope_gate", TOOLS / "scope_gate.py")
scope_gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scope_gate)


def cmd_checks():
    scope = yaml.safe_load((ROOT / "SCOPE.yaml").read_text(encoding="utf-8"))
    for item in scope["items"]:
        for spec in item["checks"]:
            if "cmd" in spec:
                yield item["id"], spec["cmd"]


def test_quoted_argument_stays_one_argument():
    parts = scope_gate.argv("python -m pytest eval/tests -q -k 'not slow'")
    assert parts[0] == sys.executable
    assert parts[-2:] == ["-k", "not slow"]


def test_list_commands_pass_through():
    assert scope_gate.argv(["git", "rev-parse", "HEAD"]) == ["git", "rev-parse", "HEAD"]


@pytest.mark.skipif(
    bool(os.environ.get("GUARD_SCOPE_GATE_ACTIVE")), reason="already inside a scope-gate cmd check"
)
@pytest.mark.parametrize("item,command", list(cmd_checks()))
def test_every_cmd_check_is_runnable(item, command):
    """A cmd check may fail because the work is not done, never because of quoting.

    pytest rc 4 / "file or directory not found" means the argv was split wrongly.
    """
    done = scope_gate.execute(command, timeout=300)
    output = done.stdout + done.stderr
    assert done.returncode != 4, f"{item}: usage error\n{output[-400:]}"
    assert "file or directory not found" not in output, f"{item}: bad argv\n{output[-400:]}"
