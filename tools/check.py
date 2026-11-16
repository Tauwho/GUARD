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
# labels (D27). A local `--local` run on a PR checkout cannot see either, so it would
# false-red a phase0-override PR. Skip it there; the required job is authoritative.
CI_OWNERSHIP = "ci-ownership-job"
IN_CI_PR = os.environ.get("GITHUB_ACTIONS") == "true" and os.environ.get(
    "GITHUB_EVENT_NAME"
) in ("pull_request", "merge_group")


def stages(fast):
    yield "format", [PY, "tools/check_format.py"]
    yield "policy", [PY, "tools/check_policy.py", "all"]
    yield "contract-compat", [PY, "tools/check_contract_compat.py", "--base", "contracts-v1.2"]
    if IN_CI_PR:
        yield "ownership", CI_OWNERSHIP
    else:
        yield "ownership", [PY, "tools/check_ownership.py", "--base", "origin/main", "--local"]
    yield "doc-links", [PY, "tools/check_doc_links.py"]
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
    subprocess.run(
        ["git", "fetch", "-q", "--no-tags", "origin", "+refs/pull/*/head:refs/remotes/pr/*"],
        cwd=ROOT,
        capture_output=True,
    )
    for name, command in stages(args.fast or args.skip_build):
        if command == CI_OWNERSHIP:
            print(f"SKIPPED {name}: the required `ownership` job runs the base-branch gate")
            continue
        if command is None:
            print(f"FAIL {name}: cmake not found (install cmake >= 3.24 or use --fast)")
            failed.append(name)
            continue
        started = time.monotonic()
        done = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        status = stage_status(name, done.returncode)
        print(f"{status} {name} ({time.monotonic() - started:.0f}s): {' '.join(command)}")
        if status == "SKIPPED":
            print("    " + (done.stdout + done.stderr).strip())
        elif done.returncode:
            failed.append(name)
            tail = (done.stdout + done.stderr).strip().splitlines()[-25:]
            print("\n".join("    " + line for line in tail))
    # Report only: never fails the build. File and grep checks only (cmd checks are slow).
    env = dict(os.environ, GUARD_SCOPE_GATE_ACTIVE="1")
    subprocess.run([PY, "tools/scope_gate.py", "--summary"], cwd=ROOT, env=env)
    print("RESULT:", "GREEN" if not failed else "RED (" + ", ".join(failed) + ")")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
