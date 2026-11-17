#!/usr/bin/env python3
"""Capture a REF-EVIDENCE bundle from a QNX target (B-10, #44). Stdlib only.

Run on the QNX host with the SDP env sourced, from the repo root:

    python3 tools/capture-evidence.py --topic qnx-hog --target root@10.10.10.2 \\
        --binary build/qnx-hog/guard-hog --run '/tmp/guard_hog_tests'

It runs the --run commands on the target over ssh, then writes
evidence/<yyyymmdd>-<sha7>-<topic>/ with transcript.txt, uname.txt, pidin.txt,
binaries.txt, slog2.txt, results.json and manifest.json (C13). The --binary files are
described, never copied into the bundle. Home paths and ssh key fingerprints are
redacted. The bundle is checked against the 1 MiB limit and, if jsonschema is
installed, against contracts/evidence-manifest.schema.json.

Scenario runs (B4): --scenario-result <out>.results.json --trace <out>.jsonl from
eval/target_runner.py put the summary, mismatches and the C8 trace into results.json.
D11 (nothing measured on QNX is published): every t_ns becomes the event index, *_ns
payload fields and the summary's time fields are removed. The trace is checked against
contracts/telemetry.schema.json when jsonschema is installed.
"""

import argparse
import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = ["transcript.txt", "uname.txt", "pidin.txt", "binaries.txt", "slog2.txt", "results.json"]
LIMIT = 1024 * 1024
SLOG_LINES = 300
FINGERPRINT = re.compile(r"SHA256:[A-Za-z0-9+/]{20,}")
DOCTEST = re.compile(
    r"\[doctest\] (test cases|assertions):\s*(\d+) \|\s*(\d+) passed \|\s*(\d+) failed"
)


def run_local(args):
    """Runs a host command and returns (exit code, combined output)."""
    try:
        done = subprocess.run(args, capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired) as error:
        return 127, str(error)
    return done.returncode, done.stdout + done.stderr


def ssh_runner(target):
    def run(command):
        return run_local(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=5", target, command])

    return run


def redact(text, home=None):
    home = home if home is not None else os.path.expanduser("~")
    if home and home not in ("/", "~"):
        text = text.replace(home, "~")
    return FINGERPRINT.sub("SHA256:<redacted>", text.replace("\r", ""))


def describe_binary(path, run=run_local):
    data = Path(path).read_bytes()
    lines = [f"{path}: sha256 {hashlib.sha256(data).hexdigest()} size {len(data)}"]
    for tool in (["file", str(path)], ["readelf", "-h", str(path)]):
        if shutil.which(tool[0]):
            lines.append(run(tool)[1].rstrip())
    return "\n".join(lines) + "\n"


def doctest_summary(text):
    out = {}
    for kind, total, passed, failed in DOCTEST.findall(text):
        counts = {"total": int(total), "passed": int(passed), "failed": int(failed)}
        out[kind.replace(" ", "_")] = counts
    if "Status: SUCCESS!" in text:
        out["status"] = "SUCCESS"
    elif "Status: FAILURE!" in text:
        out["status"] = "FAILURE"
    return out


def timed(key):
    """Summary keys that measure time (D11): *_s, *_ns, durations."""
    return key.endswith(("_s", "_ns")) or "duration" in key or "time" in key


def sanitize_trace(lines):
    """D11: keep the order and every functional field, drop what measures time on QNX."""
    events = []
    for index, line in enumerate(x for x in lines if x.strip()):
        event = json.loads(line)
        event["t_ns"] = index
        event["payload"] = {k: v for k, v in event["payload"].items() if not k.endswith("_ns")}
        events.append(event)
    return events


def scenario_results(result_path, trace_path):
    result = json.loads(Path(result_path).read_text(encoding="utf-8"))
    trace = sanitize_trace(Path(trace_path).read_text(encoding="utf-8").splitlines())
    try:
        import jsonschema
    except ImportError:
        jsonschema = None
    if jsonschema is not None:
        schema = json.loads((ROOT / "contracts" / "telemetry.schema.json").read_text())
        validator = jsonschema.Draft202012Validator(schema)
        for event in trace:
            validator.validate(event)
    summary = {k: v for k, v in result.get("summary", {}).items() if not timed(k)}
    # Mismatch texts can quote measured values; keep only which metric differed.
    mismatches = [str(m).split(":")[0].split()[0] for m in result.get("mismatches") or []]
    return {
        "source": result.get("source"),
        "commands": result.get("commands"),
        "summary": summary,
        "mismatches": mismatches,
        "trace_schema": "contracts/telemetry.schema.json (t_ns = event index, D11)",
        "trace": trace,
    }


def first_line(run, args, fallback):
    code, out = run(args)
    line = out.strip().splitlines()[0] if code == 0 and out.strip() else ""
    return line or fallback


def capture(args, run_target, run_host=run_local, now=None, home=None):
    """Writes the bundle and returns its path. Raises ValueError on a limit or schema failure."""
    sha = args.sha or first_line(run_host, ["git", "-C", str(ROOT), "rev-parse", "HEAD"], "")
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError(f"not a full commit sha: {sha!r}")
    # The folder keeps the brief's spelling (scenario-F0, SCOPE.yaml globs); C13 wants the
    # manifest topic in lower case.
    topic = args.topic.lower()
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,47}", topic):
        raise ValueError(f"bad topic: {args.topic!r}")
    now = now or datetime.datetime.now(datetime.timezone.utc)
    bundle = Path(args.out) / f"{now:%Y%m%d}-{sha[:7]}-{args.topic}"
    bundle.mkdir(parents=True, exist_ok=True)

    transcript, exit_codes = [], []
    for command in args.run:
        code, out = run_target(command)
        exit_codes.append(code)
        transcript.append(f"$ {command}\n{out.rstrip()}\n[exit {code}]\n")
    write = lambda name, text: (bundle / name).write_text(redact(text, home), encoding="utf-8")  # noqa: E731
    write("transcript.txt", "\n".join(transcript))
    write("uname.txt", run_target("uname -a")[1])
    write("pidin.txt", run_target("pidin")[1])
    slog = run_target("slog2info")[1].splitlines()[-SLOG_LINES:]
    write("slog2.txt", "\n".join(slog) + "\n")
    write("binaries.txt", "".join(describe_binary(b, run_host) for b in args.binary) or "none\n")
    text = "\n".join(transcript)
    results = {"commands": args.run, "exit_codes": exit_codes, "doctest": doctest_summary(text)}
    if args.scenario_result:
        results["scenario_run"] = scenario_results(args.scenario_result, args.trace)
        lines = [json.dumps(e, separators=(",", ":")) for e in results["scenario_run"].pop("trace")]
        body = json.dumps(results, indent=2)
        # One event per line keeps the trace readable and the bundle small.
        results_text = body[:-2] + ',\n  "trace": [\n    ' + ",\n    ".join(lines) + "\n  ]\n}\n"
        json.loads(results_text)
        write("results.json", results_text)
    else:
        write("results.json", json.dumps(results, indent=2) + "\n")

    uname = (bundle / "uname.txt").read_text().split()
    qcc = first_line(run_host, ["qcc", "-Vgcc_ntox86_64", "--version"], "unknown")
    manifest = {
        "version": 1,
        "sha": sha,
        "date": now.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "topic": topic,
        "runner_role": args.role,
        "host_os": args.host_os[:120],
        "sdp_version": args.sdp,
        "qcc_version": qcc[:200],
        "image_id": (args.image + "; " + " ".join(uname[2:5]))[:120],
        "contracts": args.contracts,
        "files": FILES,
    }
    if args.scenario:
        manifest["scenario"], manifest["mode"] = args.scenario, args.mode
        if args.seed is not None:
            manifest["seeds"] = [args.seed]
    if args.notes:
        manifest["notes"] = args.notes[:1000]
    write("manifest.json", json.dumps(manifest, indent=2) + "\n")
    check(bundle)
    return bundle


def check(bundle):
    size = sum(p.stat().st_size for p in bundle.rglob("*") if p.is_file())
    if size > LIMIT:
        raise ValueError(f"{bundle.name}: {size} bytes exceeds 1 MiB")
    try:
        import jsonschema
    except ImportError:
        return
    schema = json.loads((ROOT / "contracts" / "evidence-manifest.schema.json").read_text())
    jsonschema.validate(json.loads((bundle / "manifest.json").read_text()), schema)


def parser():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--topic", required=True)
    p.add_argument("--target", default="root@10.10.10.2")
    p.add_argument("--run", action="append", default=[], help="command to run on the target")
    p.add_argument("--binary", action="append", default=[], help="host file to describe")
    p.add_argument("--sha", help="commit the binaries were built from (default: HEAD)")
    p.add_argument("--role", choices=["po", "a", "b"], default="b")
    host_os = " ".join(os.uname()[0:3:2]) if hasattr(os, "uname") else "unknown"
    p.add_argument("--host-os", default=host_os)
    p.add_argument("--sdp", default="8.0.0")
    p.add_argument("--image", default="mkqnximage --type=qemu --arch=x86_64 (QSTI)")
    p.add_argument("--contracts", default="contracts-v1.2")
    p.add_argument("--notes", default="")
    p.add_argument("--out", default=str(ROOT / "evidence"))
    p.add_argument("--scenario-result", help="eval/target_runner.py <out>.results.json")
    p.add_argument("--trace", help="the C8 trace (JSONL) of that run")
    p.add_argument("--scenario", help="F0..F8, recorded in the manifest")
    p.add_argument("--mode", default="B2", choices=["B0", "B1", "B2"])
    p.add_argument("--seed", type=int)
    return p


def main(argv=None):
    args = parser().parse_args(argv)
    if bool(args.scenario_result) != bool(args.trace):
        print("capture-evidence: --scenario-result and --trace go together", file=sys.stderr)
        return 2
    try:
        bundle = capture(args, ssh_runner(args.target))
    except (ValueError, OSError) as error:
        print(f"capture-evidence: {error}", file=sys.stderr)
        return 1
    print(f"capture-evidence: wrote {bundle}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
