"""Ownership rules, including the D27 tooling-fix path (audit fix 6). Simulates PRs."""

import importlib.util
import os
import subprocess
from pathlib import Path

import pytest
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


def pr_repo(tmp_path, change):
    """A repo with main plus one PR commit made by change(root); returns its root."""
    env = dict(
        os.environ,
        GIT_AUTHOR_NAME="t",
        GIT_AUTHOR_EMAIL="t@example.invalid",
        GIT_COMMITTER_NAME="t",
        GIT_COMMITTER_EMAIL="t@example.invalid",
    )

    def git(*args):
        subprocess.run(["git", *args], cwd=tmp_path, env=env, check=True, capture_output=True)

    (tmp_path / "OWNERSHIP.yaml").write_bytes((TOOLS.parent / "OWNERSHIP.yaml").read_bytes())
    for name in ("qnx/gate.cpp", "qnx/space name.cpp", "qnx/caf\u00e9.cpp", "core/a.cpp"):
        (tmp_path / name).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / name).write_text("int x;\n")
    git("init", "-q", "-b", "main")
    git("add", ".")
    git("commit", "-qm", "base")
    git("switch", "-q", "-c", "pr")
    change(tmp_path)
    git("add", "-A")
    git("commit", "-qm", "pr")
    return tmp_path


def run_gate(root, head):
    return own.main(["--root", str(root), "--base", "main", "--head-ref", head, "--labels", ""])


def test_move_out_of_another_streams_area_fails(tmp_path):
    root = pr_repo(tmp_path, lambda r: (r / "qnx/gate.cpp").rename(r / "core/gate.cpp"))
    assert run_gate(root, "a/1-move") == 1


@pytest.mark.parametrize("name", ["qnx/space name.cpp", "qnx/caf\u00e9.cpp"])
def test_unusual_file_names_keep_their_owner(tmp_path, name):
    root = pr_repo(tmp_path, lambda r: (r / name).write_text("changed\n"))
    assert run_gate(root, "po/1-edit") == 1
    assert run_gate(root, "b/1-edit") == 0


def test_pr_from_a_branch_named_main_is_checked(tmp_path):
    root = pr_repo(tmp_path, lambda r: (r / "qnx/gate.cpp").write_text("changed\n"))
    assert run_gate(root, "main") == 1
