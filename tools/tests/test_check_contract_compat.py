"""check_contract_compat fails closed and enforces contracts/ext/README.md (#209)."""

import importlib.util
import json
import os
import subprocess
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "check_contract_compat", TOOLS / "check_contract_compat.py"
)
compat = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(compat)

BASE = {
    "contracts/base.schema.json": json.dumps(
        {
            "type": "object",
            "properties": {
                "frozen": {"type": "number"},
                "kind": {"type": "string", "enum": ["old"]},
            },
        }
    ),
    "contracts/reason_codes.yaml": "codes:\n  OLD: {number: 1}\n",
    "contracts/ext/CHANGES-A.md": "| date | file |\n",
    "contracts/ext/A-old.json": json.dumps(
        {"extends": "base.schema.json", "properties": {"optional": {"type": "string"}}}
    ),
    "scenarios/F0.yaml": "expected:\n  B2:\n    violations: 0\n"
    "    forbidden_reasons: [SIM_CONTACT, SIM_ZONE_BREACH]\n",
}


@pytest.fixture
def repo(tmp_path):
    env = dict(
        os.environ,
        GIT_AUTHOR_NAME="t",
        GIT_AUTHOR_EMAIL="t@example.invalid",
        GIT_COMMITTER_NAME="t",
        GIT_COMMITTER_EMAIL="t@example.invalid",
    )
    for name, text in BASE.items():
        (tmp_path / name).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / name).write_text(text, newline="\n")
    for args in (["init", "-q"], ["add", "."], ["commit", "-qm", "base"], ["tag", "v-test"]):
        subprocess.run(["git", *args], cwd=tmp_path, env=env, check=True, capture_output=True)
    return tmp_path


def run(repo, *extra):
    return compat.main(["--root", str(repo), "--base", "v-test", *extra])


def extension(repo, data, name="A-new.json"):
    (repo / "contracts/ext" / name).write_text(json.dumps(data))


def test_unchanged_baseline_passes(repo):
    assert run(repo) == 0


def test_optional_property_and_appended_kind_pass(repo):
    extension(
        repo,
        {
            "extends": "base.schema.json",
            "properties": {"note": {"type": "string"}, "kind": {"enum": ["old", "new"]}},
            "x-reason-codes": {"A_NEW": 900},
            "x-message-types": {"A_MSG": "0x0100"},
        },
    )
    with (repo / "contracts/ext/CHANGES-A.md").open("a") as stream:
        stream.write("| today | A-new.json |\n")
    assert run(repo) == 0


@pytest.mark.parametrize(
    "data",
    [
        {"required": ["frozen"]},
        {"allOf": [{"required": ["frozen"]}]},
        {"additionalProperties": False},
        {"properties": {"frozen": {"type": "integer"}}},
        {"properties": {"kind": {"enum": ["new"]}}},
        {"x-reason-codes": {"OUT_OF_RANGE": 950}},
        {"x-reason-codes": {"FRACTION": 900.5}},
        {"x-reason-codes": {"BOOL": True}},
        {"x-reason-codes": {"ONE": 900, "TWO": 900}},
        {"x-message-types": {"ONE": "0x0100", "TWO": 256}},
    ],
)
def test_non_additive_extension_fails(repo, data):
    extension(repo, {"extends": "base.schema.json", **data})
    assert run(repo) == 1


def test_reason_code_reused_across_extensions_fails(repo):
    extension(repo, {"x-reason-codes": {"ONE": 901}}, "A-one.json")
    extension(repo, {"x-reason-codes": {"TWO": 901}}, "A-two.json")
    assert run(repo) == 1


@pytest.mark.parametrize(
    "raw",
    ['{"x-reason-codes": {"A": 900}, "x-reason-codes": {}}', '{"x-reason-codes": {"A": NaN}}'],
)
def test_duplicate_keys_and_non_finite_numbers_fail(repo, raw):
    (repo / "contracts/ext/A-raw.json").write_text(raw)
    assert run(repo) == 1


def test_frozen_file_edit_deletion_and_rewritten_changelog_fail(repo):
    (repo / "contracts/base.schema.json").write_text("{}")
    assert run(repo) == 1
    subprocess.run(["git", "checkout", "-q", "--", "."], cwd=repo, check=True)
    (repo / "contracts/ext/A-old.json").unlink()
    assert run(repo) == 1
    subprocess.run(["git", "checkout", "-q", "--", "."], cwd=repo, check=True)
    (repo / "contracts/ext/CHANGES-A.md").write_text("| rewritten |\n")
    assert run(repo) == 1


@pytest.mark.parametrize(
    "name", ["contracts/new.json", "contracts/ext/nested/A-x.json", "contracts/ext/C-x.json"]
)
def test_misplaced_or_misnamed_extension_fails(repo, name):
    (repo / name).parent.mkdir(parents=True, exist_ok=True)
    (repo / name).write_text("{}")
    assert run(repo) == 1


def test_ignored_files_are_not_contracts(repo):
    (repo / ".gitignore").write_text("*.swp\n")
    (repo / "contracts/ext/A-old.json.swp").write_text("editor state")
    assert run(repo) == 0


def test_extends_cannot_escape_contracts(repo):
    extension(repo, {"extends": "../scenarios/F0.yaml"})
    assert run(repo) == 1


@pytest.mark.parametrize(
    "text",
    [
        "expected: {B2: {violations: 1, forbidden_reasons: [SIM_CONTACT, SIM_ZONE_BREACH]}}",
        "expected: {B2: {violations: 0, forbidden_reasons: [SIM_CONTACT]}}",
        "expected: {}\nexpected: {B2: {violations: 0}}",
    ],
)
def test_scenario_invariant_weakening_fails(repo, text):
    (repo / "scenarios/F0.yaml").write_text(text)
    assert run(repo) == 1


def test_git_error_is_a_failure_not_a_pass(repo, monkeypatch, capsys):
    original = compat.git

    def broken(root, *args):
        if args[0] == "ls-tree":
            raise subprocess.CalledProcessError(128, args, stderr=b"simulated read error")
        return original(root, *args)

    monkeypatch.setattr(compat, "git", broken)
    assert run(repo) == 1
    assert "simulated read error" in capsys.readouterr().out


def test_missing_tag_without_remote_fails(repo):
    assert run(repo, "--base", "missing") == 1
