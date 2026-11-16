#!/usr/bin/env python3
"""Evaluate SCOPE.yaml: DONE / OPEN / AT-RISK / DROPPED per item, by date and executable checks.

python tools/scope_gate.py            full table (Markdown)
python tools/scope_gate.py --summary  one line per stream (used by tools/check.py)
python tools/scope_gate.py --stream B only one stream
python tools/scope_gate.py --check-core-evidence
    exit 0 if a core-on-target bundle covers the core on main
Never fails the build; it is a report. Agents read it to self-descope
(DROPPED = stop working on it).
"""

import argparse
import datetime
import glob
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
SGT = datetime.timezone(datetime.timedelta(hours=8))


def run(command):
    # A cmd check may itself call tools/check.py, which calls this script again.
    # Inside a cmd check, nested scope_gate runs skip cmd checks, so it cannot recurse.
    if os.environ.get("GUARD_SCOPE_GATE_ACTIVE"):
        return False
    env = dict(os.environ, GUARD_SCOPE_GATE_ACTIVE="1")
    try:
        done = subprocess.run(
            command, cwd=ROOT, shell=True, capture_output=True, text=True, env=env, timeout=600
        )
    except subprocess.TimeoutExpired:
        return False
    return done.returncode == 0


def check(spec):
    if "file" in spec:
        return bool(glob.glob(str(ROOT / spec["file"])))
    if "grep" in spec:
        path = ROOT / spec["grep"]["file"]
        return (
            path.exists()
            and re.search(spec["grep"]["re"], path.read_text(encoding="utf-8")) is not None
        )
    if "cmd" in spec:
        return run(spec["cmd"])
    return False


def core_evidence():
    first = subprocess.run(
        ["git", "log", "--diff-filter=A", "--format=%H", "--", "core/src/safety/supervisor.cpp"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    ).stdout.split()
    if not first:
        return False
    landed = first[-1]
    for manifest in glob.glob(str(ROOT / "evidence/*-core-on-target/manifest.json")):
        sha = json.loads(Path(manifest).read_text(encoding="utf-8")).get("sha", "")
        if (
            subprocess.run(["git", "merge-base", "--is-ancestor", landed, sha], cwd=ROOT).returncode
            == 0
        ):
            return True
    return False


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--summary", action="store_true")
    parser.add_argument("--stream")
    parser.add_argument("--check-core-evidence", action="store_true")
    parser.add_argument("--now", help="override time, ISO, for tests")
    args = parser.parse_args()
    if args.check_core_evidence:
        return 0 if core_evidence() else 1
    scope = yaml.safe_load((ROOT / "SCOPE.yaml").read_text(encoding="utf-8"))
    now = (
        datetime.datetime.fromisoformat(args.now)
        if args.now
        else datetime.datetime.now(SGT).replace(tzinfo=None)
    )
    dates = {
        k: datetime.datetime.fromisoformat(v) if isinstance(v, str) else v
        for k, v in scope["dates"].items()
    }
    rows = []
    for item in scope["items"]:
        if args.stream and item["owner"] != args.stream:
            continue
        done = all(check(spec) for spec in item["checks"])
        due = dates[item["due"]]
        if done:
            state = "DONE"
        elif item["tier"] == "SHOULD" and now > dates["cut_1"]:
            state = "DROPPED"
        elif item["tier"] == "STRETCH" and now > dates["cut_2"]:
            state = "DROPPED"
        elif item["tier"] == "MUST" and now > due:
            state = "AT-RISK"
        else:
            state = "OPEN"
        rows.append((item, state, due))
    if args.summary:
        for stream in ("A", "B", "P"):
            mine = [r for r in rows if r[0]["owner"] == stream]
            counts = {
                s: sum(1 for r in mine if r[1] == s) for s in ("DONE", "OPEN", "AT-RISK", "DROPPED")
            }
            print(f"scope {stream}: " + ", ".join(f"{k} {v}" for k, v in counts.items()))
        return 0
    print(f"Scope gate {now:%a %d %b %H:%M} SGT\n")
    print("| ID | Tier | Owner | Due | State | Item |\n|---|---|---|---|---|---|")
    for item, state, due in rows:
        print(
            f"| {item['id']} | {item['tier']} | {item['owner']} | {due:%d %b} | "
            f"{state} | {item['title']} |"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
