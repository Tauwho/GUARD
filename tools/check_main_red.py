#!/usr/bin/env python3
"""Fail (and optionally open an issue) if the newest main commit has a red required check.

python tools/check_main_red.py [--sha SHA] [--open-issue]

Reads `.github/required-checks.txt`, lists the commit's check runs with `gh api`,
keeps the newest run per check name (highest id), and treats `failure`,
`cancelled`, `timed_out`, `action_required` and `startup_failure` as red. Pending or
missing checks are reported but are not red. With --open-issue, a red result opens
one issue titled "main is red: <sha7>" unless an open issue with that title exists.
Exit 0 = no red required check, 1 = red, 2 = cannot read the checks.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = "Tauwho/GUARD"
RED = {"failure", "cancelled", "timed_out", "action_required", "startup_failure"}


def required():
    lines = (ROOT / ".github" / "required-checks.txt").read_text(encoding="utf-8").splitlines()
    return [line.strip() for line in lines if line.strip() and not line.startswith("#")]


def verdict(check_runs, names):
    """Return (red, pending) lists of required check names, newest run per name wins."""
    newest = {}
    for run in check_runs:
        if run["name"] not in newest or run["id"] > newest[run["name"]]["id"]:
            newest[run["name"]] = run
    red, pending = [], []
    for name in names:
        run = newest.get(name)
        if run is None or run.get("status") != "completed":
            pending.append(name)
        elif run.get("conclusion") in RED:
            red.append(f"{name} ({run['conclusion']})")
    return red, pending


def gh(*args):
    return subprocess.run(["gh", *args], capture_output=True, text=True, check=True).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sha")
    parser.add_argument("--open-issue", action="store_true")
    args = parser.parse_args()
    try:
        sha = args.sha or json.loads(gh("api", f"repos/{REPO}/commits/main"))["sha"]
        runs = []
        page = 1
        while True:
            body = json.loads(
                gh("api", f"repos/{REPO}/commits/{sha}/check-runs?per_page=100&page={page}")
            )
            runs += body["check_runs"]
            if len(body["check_runs"]) < 100:
                break
            page += 1
    except (subprocess.CalledProcessError, KeyError, json.JSONDecodeError) as error:
        print(f"main-red: cannot read checks: {error}")
        return 2
    red, pending = verdict(runs, required())
    print(f"main-red: {sha[:7]} red={len(red)} pending={len(pending)}")
    for name in red:
        print(f"  RED {name}")
    for name in pending:
        print(f"  pending {name}")
    if not red:
        return 0
    if args.open_issue:
        title = f"main is red: {sha[:7]}"
        open_titles = json.loads(
            gh("issue", "list", "-R", REPO, "--state", "open", "--search", title, "--json", "title")
        )
        if not any(issue["title"] == title for issue in open_titles):
            body = (
                f"The nightly found red required checks on main `{sha}`:\n\n"
                + "\n".join(f"- {name}" for name in red)
                + "\n\nThe stream that owns the failing path fixes it on its own branch. "
                "Others base new work on the newest `green-*` tag until main is green (D26)."
            )
            gh("issue", "create", "-R", REPO, "--title", title, "--body", body)
            print(f"main-red: opened issue '{title}'")
    return 1


if __name__ == "__main__":
    sys.exit(main())
