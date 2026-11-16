"""check_main_red.verdict: newest run per check wins; pending is not red (audit fix 2)."""

import importlib.util
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_main_red", TOOLS / "check_main_red.py")
main_red = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(main_red)


def run(id_, name, conclusion, status="completed"):
    return {"id": id_, "name": name, "status": status, "conclusion": conclusion}


def test_required_list_matches_branch_protection_size():
    assert len(main_red.required()) == 13


def test_red_required_check_is_reported():
    red, pending = main_red.verdict([run(1, "eval", "failure")], ["eval"])
    assert red == ["eval (failure)"] and pending == []


def test_newer_success_supersedes_older_failure():
    runs = [run(1, "eval", "failure"), run(2, "eval", "success")]
    assert main_red.verdict(runs, ["eval"]) == ([], [])


def test_newer_cancel_is_red():
    runs = [run(1, "eval", "success"), run(2, "eval", "cancelled")]
    assert main_red.verdict(runs, ["eval"])[0] == ["eval (cancelled)"]


def test_pending_and_missing_are_not_red():
    runs = [run(1, "eval", None, status="in_progress")]
    assert main_red.verdict(runs, ["eval", "format"]) == ([], ["eval", "format"])


def test_unrequired_red_check_is_ignored():
    assert main_red.verdict([run(1, "make-check", "failure")], ["eval"]) == ([], ["eval"])
