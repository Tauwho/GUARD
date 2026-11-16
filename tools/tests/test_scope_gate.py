"""scope_gate.py: argv done-checks (audit 1), honest MANUAL items (audit 3),
B4 early-cut rule for SHOULD items (audit 7)."""

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


def test_cmd_is_unknown_inside_check_py(monkeypatch):
    monkeypatch.setenv("GUARD_SCOPE_GATE_ACTIVE", "1")
    assert scope_gate.check({"cmd": "python -c 'raise SystemExit(1)'"}) is None


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


AFTER_CP2 = datetime.datetime(2026, 11, 30, 9, 0)


def fake_scope(b4_checks):
    return {
        "items": [
            {"id": "B4", "tier": "MUST", "owner": "B", "due": "cp2", "checks": b4_checks},
        ]
    }


def test_f0_early_cut_is_active_after_cp2_without_b4():
    assert scope_gate.b4_early_cut(fake_scope([{"file": "no-such-f0/*"}]), AFTER_CP2, DATES)


def test_f0_early_cut_is_inactive_when_b4_is_done():
    assert not scope_gate.b4_early_cut(fake_scope([{"file": "README.md"}]), AFTER_CP2, DATES)


def test_f0_early_cut_is_inactive_before_cp2():
    missing = fake_scope([{"file": "no-such-f0/*"}])
    assert not scope_gate.b4_early_cut(missing, NOW, DATES)


def test_should_item_drops_after_cp2_when_f0_is_missing():
    should = item("A6", tier="SHOULD", due="cut_1", checks=[{"file": "no-such-advisor/*"}])
    assert scope_gate.item_state(should, AFTER_CP2, DATES, early_cut=True)[0] == "DROPPED"


def test_should_item_survives_before_cp2_even_without_f0():
    should = item("A6", tier="SHOULD", due="cut_1", checks=[{"file": "no-such-advisor/*"}])
    assert scope_gate.item_state(should, NOW, DATES, early_cut=False)[0] == "OPEN"


def test_f0_early_cut_leaves_must_items_alone():
    must = item("P1", tier="MUST", due="package_done", checks=[{"file": "no-such-readme/*"}])
    assert scope_gate.item_state(must, AFTER_CP2, DATES, early_cut=True)[0] == "OPEN"


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
