"""Host tests for tools/capture-evidence.py with fake target and host runners."""

import datetime
import importlib.util
import json
from pathlib import Path

import jsonschema
import pytest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("capture", TOOLS / "capture-evidence.py")
capture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(capture)

SHA = "a" * 40
NOW = datetime.datetime(2026, 11, 16, 12, 0, tzinfo=datetime.timezone.utc)
DOCTEST_OUT = (
    "[doctest] test cases: 4 | 4 passed | 0 failed | 0 skipped\n"
    "[doctest] assertions: 20 | 20 passed | 0 failed |\n"
    "[doctest] Status: SUCCESS!\n"
)


def fake_target(command):
    if command == "uname -a":
        return 0, "QNX noname 8.0.0 2026/10/30-09:10:05EDT x86pc x86_64\n"
    if command == "slog2info":
        return 0, "\n".join(f"line {i}" for i in range(500))
    if command == "pidin":
        return 0, "pid tid name\n1 1 procnto\n"
    key = f"Accepted publickey ED25519 SHA256:{'Q' * 43}\n"
    return 0, "ran in /home/alice/work\n" + key + DOCTEST_OUT


def fake_host(args):
    if args[0] == "qcc":
        return 0, "Toolchain:  12.2.0\tVersion:  gcc_ntox86_64\n"
    return 0, "described\n"


def args_for(tmp_path, **extra):
    argv = ["--topic", "qnx-hog", "--sha", SHA, "--out", str(tmp_path), "--run", "/tmp/t"]
    for key, value in extra.items():
        argv += [f"--{key}", value]
    return capture.parser().parse_args(argv)


def test_bundle_has_every_file_and_a_schema_valid_manifest(tmp_path):
    bundle = capture.capture(args_for(tmp_path), fake_target, fake_host, NOW, "/home/alice")
    assert bundle.name == "20261116-aaaaaaa-qnx-hog"
    assert sorted(p.name for p in bundle.iterdir()) == sorted(capture.FILES + ["manifest.json"])
    manifest = json.loads((bundle / "manifest.json").read_text())
    schema = json.loads((TOOLS.parent / "contracts" / "evidence-manifest.schema.json").read_text())
    jsonschema.validate(manifest, schema)
    assert manifest["files"] == capture.FILES
    assert manifest["qcc_version"].startswith("Toolchain:")


def test_transcript_is_redacted_and_results_parse_doctest(tmp_path):
    bundle = capture.capture(args_for(tmp_path), fake_target, fake_host, NOW, "/home/alice")
    transcript = (bundle / "transcript.txt").read_text()
    assert "/home/alice" not in transcript and "~/work" in transcript
    assert "SHA256:<redacted>" in transcript
    results = json.loads((bundle / "results.json").read_text())
    assert results["exit_codes"] == [0]
    assert results["doctest"]["test_cases"] == {"total": 4, "passed": 4, "failed": 0}
    assert results["doctest"]["status"] == "SUCCESS"


def test_slog_is_trimmed_and_binaries_described(tmp_path):
    binary = tmp_path / "guard-hog"
    binary.write_bytes(b"\x7fELF fake")
    args = args_for(tmp_path, binary=str(binary))
    bundle = capture.capture(args, fake_target, fake_host, NOW, "")
    assert len((bundle / "slog2.txt").read_text().splitlines()) == capture.SLOG_LINES
    assert "sha256" in (bundle / "binaries.txt").read_text()


@pytest.mark.parametrize("sha,topic", [("abc", "qnx-hog"), ("a" * 40, "Bad Topic")])
def test_rejects_bad_sha_or_topic(tmp_path, sha, topic):
    args = capture.parser().parse_args(["--topic", topic, "--sha", sha, "--out", str(tmp_path)])
    with pytest.raises(ValueError):
        capture.capture(args, fake_target, fake_host, NOW, "")


def test_size_limit(tmp_path, monkeypatch):
    monkeypatch.setattr(capture, "LIMIT", 100)
    with pytest.raises(ValueError, match="1 MiB"):
        capture.capture(args_for(tmp_path), fake_target, fake_host, NOW, "")
