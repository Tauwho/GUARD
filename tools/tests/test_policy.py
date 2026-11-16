"""Regression tests use disposable tracked repositories, never the working tree."""

import importlib.util
import json
import subprocess
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("policy", TOOLS / "check_policy.py")
policy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(policy)


def put(root, name, content):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    subprocess.run(["git", "add", "--", name], cwd=root, check=True)


@pytest.fixture
def repo(tmp_path):
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    put(tmp_path, "tools/claims-lint.txt", (TOOLS / "claims-lint.txt").read_text())
    return tmp_path


@pytest.mark.parametrize(
    "source",
    [
        "#include <sys/neutrino.h>",
        "MsgSend(1, 0);",
        "clock_gettime(0, 0);",
        'std::ifstream("data");',
        'fopen("data", "r");',
        "resmgr_attach(0);",
        "slog2f(0);",
    ],
)
def test_portability_rejects_target_and_io(repo, source):
    put(repo, "core/src/example.cpp", source)
    assert policy.check(repo, "core-portable")


@pytest.mark.parametrize(
    "name,content",
    [
        ("target.qcow2", "image"),
        ("qnxsdp-env.sh", "environment"),
        ("private.pem", "key"),
        ("example.txt", "ghp_" + "x" * 36),
        ("copy.h", "/* Copyright QNX Software Systems */"),
        ("output.bin", "\x00binary"),
    ],
)
def test_artifact_and_secret_rejection(repo, name, content):
    put(repo, name, content)
    assert policy.check(repo, "no-qnx-files")


def test_claim_marker_ends_at_next_heading(repo):
    prefix = "<!-- claims-ok: negated -->\nNo certified system.\n"
    put(repo, "README.md", prefix)
    assert not policy.check(repo, "claims-lint")
    put(repo, "README.md", prefix + "## Next\nCertified!\n")
    assert policy.check(repo, "claims-lint")


def test_schema_valid_and_invalid_fixtures(repo):
    put(repo, "contracts/config.schema.json", json.dumps({"type": "integer"}))
    put(repo, "contracts/fixtures/schemas/config.valid.json", "1")
    put(repo, "contracts/fixtures/schemas/config.invalid.json", '"bad"')
    assert not policy.check(repo, "schemas")
    put(repo, "contracts/fixtures/schemas/config.valid.json", '"bad"')
    assert policy.check(repo, "schemas")


def test_evidence_checks_ancestry_and_files(repo):
    subprocess.run(
        [
            "git",
            "-c",
            "user.name=Test",
            "-c",
            "user.email=test@example.org",
            "commit",
            "-qm",
            "fixture",
        ],
        cwd=repo,
        check=True,
    )
    sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
    put(repo, "evidence/bundle/results.json", "{}")
    put(repo, "evidence/bundle/manifest.json", json.dumps({"sha": sha, "files": ["results.json"]}))
    assert not policy.check(repo, "evidence")
    put(repo, "evidence/bundle/manifest.json", json.dumps({"sha": "0" * 40, "files": ["missing"]}))
    assert len(policy.check(repo, "evidence")) == 3


def test_nested_evidence_counts_towards_size_limit(repo):
    put(repo, "evidence/bundle/manifest.json", json.dumps({"sha": "0" * 40, "files": []}))
    put(repo, "evidence/bundle/nested/large.txt", "x" * (1024 * 1024 + 1))
    assert any("1 MiB" in error for error in policy.check(repo, "evidence"))


def test_plain_portable_core_passes(repo):
    put(repo, "core/src/example.cpp", "int twice(int value) { return value * 2; }")
    assert not policy.check(repo, "core-portable")
