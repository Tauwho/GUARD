#!/usr/bin/env python3
"""Evaluate SCOPE.yaml: DONE / OPEN / AT-RISK / DROPPED per item, by date and executable checks.

python tools/scope_gate.py [--report] full table (Markdown)
python tools/scope_gate.py --summary  one line per stream (used by tools/check.py)
python tools/scope_gate.py --stream B only one stream
python tools/scope_gate.py --check-core-evidence
    exit 0 if a core-on-target bundle covers the core on main
Never fails the build; it is a report. Agents read it to self-descope
(DROPPED = stop working on it).

A check is file/grep/cmd (pass/fail) or `evidence: manual` (returns None).
An item with manual evidence is MANUAL and never counts as DONE; the summary
counts it separately. A progress file alone is never a done-check (audit fix 3).

Early cut (audit fix 7): after cp2, if B4 (F0 on target evidence) is not DONE,
every SHOULD item is DROPPED. B's gate is the demo's backbone; without it the
host-only SHOULD work has nothing to point at. MUST items are unaffected.
"""

import argparse
import datetime
import glob
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
SGT = datetime.timezone(datetime.timedelta(hours=8))


def argv(command):
    """A cmd check as an argv list, the same on Windows and Linux (no shell quoting).

    A string is split with POSIX rules, so `-k 'not slow'` stays one argument even
    under cmd.exe. A leading `python`/`python3` becomes this interpreter.
    """
    parts = list(command) if isinstance(command, list) else shlex.split(command, posix=True)
    if parts and parts[0] in ("python", "python3"):
        parts[0] = sys.executable
    return parts


def execute(command, timeout=600):
    """Run one cmd check without a shell. Returns the CompletedProcess (rc 124 on timeout)."""
    env = dict(os.environ, GUARD_SCOPE_GATE_ACTIVE="1")
    try:
        return subprocess.run(
            argv(command), cwd=ROOT, capture_output=True, text=True, env=env, timeout=timeout
        )
    except subprocess.TimeoutExpired as error:
        return subprocess.CompletedProcess(error.cmd, 124, "", "timeout")
    except FileNotFoundError as error:
        return subprocess.CompletedProcess(argv(command), 127, "", str(error))


def run(command):
    # A cmd check may itself call tools/check.py, which calls this script again.
    # Inside a cmd check, nested scope_gate runs cannot evaluate cmd checks: report
    # them as unknown (None -> MANUAL), never as a failure. `--report` outside
    # check.py runs them for real.
    if os.environ.get("GUARD_SCOPE_GATE_ACTIVE"):
        return None
    return execute(command).returncode == 0


def check(spec):
    """True pass, False fail, None manual (cannot be machine-verified here)."""
    if spec.get("evidence") == "manual":
        return None
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


def item_state(item, now, dates, early_cut=False):
    """DONE / MANUAL / OPEN / AT-RISK / DROPPED for one item (pure).

    early_cut: the B4-gate rule is in force (after cp2, F0 is not on target);
    SHOULD items are then DROPPED regardless of cut_1.
    """
    results = [check(spec) for spec in item.get("checks", [])]
    failed = any(result is False for result in results)
    manual = item.get("evidence") == "manual" or any(result is None for result in results)
    done = not failed and not manual and all(result is True for result in results)
    due = dates[item["due"]]
    if done:
        return "DONE", due
    if manual and not failed:
        return "MANUAL", due
    if item["tier"] == "SHOULD" and (early_cut or now > dates["cut_1"]):
        return "DROPPED", due
    if item["tier"] == "STRETCH" and now > dates["cut_2"]:
        return "DROPPED", due
    if item["tier"] == "MUST" and now > due:
        return "AT-RISK", due
    return "OPEN", due


def b4_early_cut(scope, now, dates):
    """True after cp2 when the F0-on-target bundle (B4) is not DONE (audit fix 7)."""
    b4 = next((i for i in scope["items"] if i["id"] == "B4"), None)
    return (
        b4 is not None and now > dates["cp2"] and item_state(b4, now, dates)[0] != "DONE"
    )


def summarize(rows):
    """Rows of (item, state, due) -> one line per stream, MANUAL counted separately."""
    for stream in ("A", "B", "P"):
        mine = [row for row in rows if row[0]["owner"] == stream]
        counts = {
            state: sum(1 for row in mine if row[1] == state)
            for state in ("DONE", "OPEN", "AT-RISK", "MANUAL", "DROPPED")
        }
        print(f"scope {stream}: " + ", ".join(f"{k} {v}" for k, v in counts.items()))


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
    parser.add_argument("--report", action="store_true", help="full table (the default)")
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
    early_cut = b4_early_cut(scope, now, dates)
    rows = []
    for item in scope["items"]:
        if args.stream and item["owner"] != args.stream:
            continue
        state, due = item_state(item, now, dates, early_cut)
        rows.append((item, state, due))
    if args.summary:
        summarize(rows)
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
