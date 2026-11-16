"""scope_gate.py: argv done-checks (audit 1) and honest MANUAL items (audit 3)."""

import datetime
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


SCOPE = yaml.safe_load((ROOT / "SCOPE.yaml").read_text(encoding="utf-8"))
DATES = {key: datetime.datetime.fromisoformat(value) for key, value in SCOPE["dates"].items()}
NOW = datetime.datetime(2026, 11, 20, 12, 0)


def cmd_checks():
    for item in SCOPE["items"]:
        for spec in item.get("checks", []):
            if "cmd" in spec:
                yield item["id"], spec["cmd"]


def item(item_id="X", tier="MUST", due="cp1", checks=None, evidence=None):
    spec = {"id": item_id, "tier": tier, "owner": "A", "due": due, "checks": checks or []}
    if evidence:
        spec["evidence"] = evidence
    return spec


def test_no_progress_file_is_a_done_check():
    for scope_item in SCOPE["items"]:
        for spec in scope_item.get("checks", []):
            if "grep" in spec:
                assert not spec["grep"]["file"].startswith("progress/"), scope_item["id"]


def test_manual_item_is_manual_not_done():
    state, _ = scope_gate.item_state(item(evidence="manual"), NOW, DATES)
    assert state == "MANUAL"


def test_manual_item_stays_manual_even_when_its_file_check_passes():
    state, _ = scope_gate.item_state(
        item(checks=[{"file": "README.md"}], evidence="manual"), NOW, DATES
    )
    assert state == "MANUAL"


def test_manual_item_with_a_failing_check_still_shows_open():
    state, _ = scope_gate.item_state(
        item(checks=[{"file": "does-not-exist/*"}], evidence="manual"), NOW, DATES
    )
    assert state == "OPEN"


def test_passing_file_check_is_done():
    state, _ = scope_gate.item_state(item(checks=[{"file": "README.md"}]), NOW, DATES)
    assert state == "DONE"


def test_summary_counts_manual_separately(capsys):
    rows = [
        (item("A1", checks=[{"file": "README.md"}]), "DONE", DATES["cp1"]),
        (item("A2", evidence="manual"), "MANUAL", DATES["cp1"]),
        (item("A3"), "OPEN", DATES["cp1"]),
    ]
    scope_gate.summarize(rows)
    out = capsys.readouterr().out
    assert "DONE 1" in out and "MANUAL 1" in out and "OPEN 1" in out


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
