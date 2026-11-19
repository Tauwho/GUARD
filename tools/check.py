#!/usr/bin/env python3
"""`make check`: the one definition of "green". CI runs the same stages.

Usage: python tools/check.py [--fast] [--skip-build]
  --fast        skip the C++ build and tests (Python and policy only)
  --skip-build  same as --fast, kept for scripts
Exit 0 only if every stage passes. Prints each command and its result, so the
output can be pasted into progress/<stream>.md as evidence.
"""

import argparse
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PY = sys.executable
# In CI the required `ownership` job runs the base-branch gate with the PR title and
# labels (D27). A `--local` run cannot see either on a feature-branch push or a PR
# checkout, so it would false-red a phase0-override branch. Skip it in CI; the
# required job on the PR (and the merge itself) is authoritative.
CI_OWNERSHIP = "ci-ownership-job"
CI_EVENTS = ("pull_request", "merge_group", "push")
IN_CI = os.environ.get("GITHUB_ACTIONS") == "true" and os.environ.get(
    "GITHUB_EVENT_NAME"
) in CI_EVENTS


def stages(fast):
    yield "format", [PY, "tools/check_format.py"]
    yield "policy", [PY, "tools/check_policy.py", "all"]
    yield "contract-compat", [PY, "tools/check_contract_compat.py", "--base", "contracts-v1.2"]
    if IN_CI:
        yield "ownership", CI_OWNERSHIP
    else:
        yield "ownership", [PY, "tools/check_ownership.py", "--base", "origin/main", "--local"]
    yield "doc-links", [PY, "tools/check_doc_links.py"]
    yield "brief-sync", [PY, "tools/check_brief_sync.py"]
    if not fast:
        if not shutil.which("cmake"):
            yield "host-build", None
        else:
            yield "host-configure", ["cmake", "--preset", "host-debug"]
            yield "host-build", ["cmake", "--build", "--preset", "host-debug", "--parallel", "4"]
            yield "host-test", ["ctest", "--preset", "host-debug"]
    yield "ruff", [PY, "-m", "ruff", "check", "."]
    yield "pytest", [PY, "-m", "pytest", "-q"]


FORMAT_TOOL_MISSING = 3  # tools/check_format.py exit code; it already fails in CI
STAGE_TIMEOUT = 1200  # seconds; a hung stage fails instead of stalling the run
FETCH_TIMEOUT = 60


def stage_status(name, returncode):
    """PASS, FAIL, or SKIPPED (only the format stage, only when clang-format is absent)."""
    if returncode == 0:
        return "PASS"
    if name == "format" and returncode == FORMAT_TOOL_MISSING:
        return "SKIPPED"
    return "FAIL"


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--fast", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    failed = []
    # The evidence policy (D21) accepts bundles whose SHA is in a PR head, as CI does.
    # Best effort: offline runs still check against local history only.
    try:
        subprocess.run(
            ["git", "fetch", "-q", "--no-tags", "origin", "+refs/pull/*/head:refs/remotes/pr/*"],
            cwd=ROOT,
            capture_output=True,
            timeout=FETCH_TIMEOUT,
        )
    except (OSError, subprocess.TimeoutExpired):
        print("note: could not fetch PR heads; evidence is checked against local history")
    for name, command in stages(args.fast or args.skip_build):
        if command == CI_OWNERSHIP:
            print(
                f"SKIPPED {name}: the required `ownership` job checks the PR; "
                "CI cannot see labels here"
            )
            continue
        if command is None:
            print(f"FAIL {name}: cmake not found (install cmake >= 3.24 or use --fast)")
            failed.append(name)
            continue
        started = time.monotonic()
        try:
            done = subprocess.run(
                command, cwd=ROOT, capture_output=True, text=True, timeout=STAGE_TIMEOUT
            )
        except OSError as error:
            print(f"FAIL {name}: cannot run {command[0]}: {error}")
            failed.append(name)
            continue
        except subprocess.TimeoutExpired:
            print(f"FAIL {name}: timed out after {STAGE_TIMEOUT}s: {' '.join(command)}")
            failed.append(name)
            continue
        status = stage_status(name, done.returncode)
        print(f"{status} {name} ({time.monotonic() - started:.0f}s): {' '.join(command)}")
        if status == "SKIPPED":
            print("    " + (done.stdout + done.stderr).strip())
        elif done.returncode:
            failed.append(name)
            tail = (done.stdout + done.stderr).strip().splitlines()[-25:]
            print("\n".join("    " + line for line in tail))
    # Report only: never fails the build. cmd checks are not run here (they show
    # MANUAL); `python tools/scope_gate.py --report` runs the full table.
    env = dict(os.environ, GUARD_SCOPE_GATE_ACTIVE="1")
    try:
        subprocess.run(
            [PY, "tools/scope_gate.py", "--summary"], cwd=ROOT, env=env, timeout=FETCH_TIMEOUT
        )
    except subprocess.TimeoutExpired:
        print("scope: summary timed out (report only)")
    print("RESULT:", "GREEN" if not failed else "RED (" + ", ".join(failed) + ")")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
