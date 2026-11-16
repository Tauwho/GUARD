"""Ownership rules, including the D27 tooling-fix path (audit fix 6). Simulates PRs."""

import importlib.util
from pathlib import Path

import yaml

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_ownership", TOOLS / "check_ownership.py")
own = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(own)
RULES = own.compile_rules(
    yaml.safe_load((TOOLS.parent / "OWNERSHIP.yaml").read_text(encoding="utf-8"))
)


def test_b_tooling_fix_to_check_policy_passes():
    ok, lines = own.evaluate(
        "B",
        ["tools/check_policy.py", "tools/tests/test_policy.py"],
        RULES,
        title="tooling-fix: evidence check rejects valid PR-head SHA",
    )
    assert ok, lines


def test_b_tooling_fix_mixed_with_own_files_fails():
    ok, lines = own.evaluate(
        "B",
        ["tools/check_policy.py", "qnx/gate/gate.cpp"],
        RULES,
        title="tooling-fix: evidence check",
    )
    assert not ok and any("qnx/gate/gate.cpp" in line for line in lines)


def test_tooling_fix_cannot_touch_frozen_or_other_streams_files():
    for path in (
        "SCOPE.yaml",
        "contracts/messages.md",
        "core/src/fsm/gate_fsm.cpp",
        "tools/env-check.py",
    ):
        ok, _ = own.evaluate("A", [path], RULES, title="tooling-fix: x")
        assert not ok, path


def test_tooling_fix_cannot_change_its_own_gate():
    for path in own.TOOLING_FIX_EXCLUDED:
        ok, _ = own.evaluate("B", [path], RULES, title="tooling-fix: loosen the gate")
        assert not ok, path


def test_tooling_fix_allows_ci_and_format_config():
    ok, lines = own.evaluate(
        "A",
        [".github/workflows/ci.yml", ".clang-format", "tools/check_format.py"],
        RULES,
        title="tooling-fix: format job",
    )
    assert ok, lines


def test_without_the_title_b_cannot_touch_p_tooling():
    ok, _ = own.evaluate("B", ["tools/check_policy.py"], RULES, title="fix: evidence check")
    assert not ok


def test_streams_keep_their_own_files():
    assert own.evaluate("B", ["qnx/gate/gate.cpp"], RULES)[0]
    assert own.evaluate("A", ["core/src/fsm/gate_fsm.cpp"], RULES)[0]
    assert not own.evaluate("A", ["qnx/gate/gate.cpp"], RULES)[0]
