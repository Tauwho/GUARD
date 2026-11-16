"""Brief milestone tables stay in sync with SCOPE.yaml (audit fix 4)."""

import datetime
import importlib.util
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("brief_sync", TOOLS / "brief_sync.py")
brief_sync = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(brief_sync)

MINI = """<!-- gen:milestones-A -->
| ID | Due (SGT) | Tier | Work | Acceptance check |
|---|---|---|---|---|
| A1 | Sun 22 Nov | MUST | work | check |
<!-- gen:milestones-A-end -->
"""
DATES = {"cp1": datetime.datetime(2026, 11, 22, 23, 59)}
ITEMS = {"A1": {"id": "A1", "owner": "A", "tier": "MUST", "due": "cp1"}}


def test_real_briefs_match_scope():
    assert brief_sync.check_all() == []


def test_due_text_uses_one_source_of_truth():
    assert brief_sync.due_text("2026-11-22T23:59") == "Sun 22 Nov"
    assert brief_sync.due_text("2026-12-10T18:00") == "Thu 10 Dec 18:00"
    assert brief_sync.due_text("2026-12-07T23:59") == "Mon 7 Dec"


def test_stale_due_and_tier_are_reported():
    stale = MINI.replace("Sun 22 Nov | MUST", "Mon 23 Nov | SHOULD")
    errors = brief_sync.problems("A", DATES, ITEMS, stale)
    assert any("A1: due" in error for error in errors)
    assert any("A1: tier" in error for error in errors)


def test_rewrite_fixes_due_and_tier_and_keeps_prose():
    stale = MINI.replace("Sun 22 Nov | MUST", "Mon 23 Nov | SHOULD")
    fixed = brief_sync.rewrite("A", DATES, ITEMS, stale)
    assert "Sun 22 Nov | MUST | work | check" in fixed
    assert brief_sync.problems("A", DATES, ITEMS, fixed) == []


def test_missing_markers_fail_loudly():
    with pytest.raises(brief_sync.BriefError):
        brief_sync.table("| A1 | Sun 22 Nov | MUST | work | check |\n", "A")
