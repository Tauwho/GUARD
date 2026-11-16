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


def stages(fast):
    yield "format", [PY, "tools/check_format.py"]
    yield "policy", [PY, "tools/check_policy.py", "all"]
    yield "contract-compat", [PY, "tools/check_contract_compat.py", "--base", "contracts-v1.2"]
    yield "ownership", [PY, "tools/check_ownership.py", "--base", "origin/main", "--local"]
    if not fast:
        if not shutil.which("cmake"):
            yield "host-build", None
        else:
            yield "host-configure", ["cmake", "--preset", "host-debug"]
            yield "host-build", ["cmake", "--build", "--preset", "host-debug", "--parallel", "4"]
            yield "host-test", ["ctest", "--preset", "host-debug"]
    yield "ruff", [PY, "-m", "ruff", "check", "."]
    yield "pytest", [PY, "-m", "pytest", "-q"]


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--fast", action="store_true")
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    failed = []
    for name, command in stages(args.fast or args.skip_build):
        if command is None:
            print(f"FAIL {name}: cmake not found (install cmake >= 3.24 or use --fast)")
            failed.append(name)
            continue
        started = time.monotonic()
        done = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        status = "PASS" if done.returncode == 0 else "FAIL"
        print(f"{status} {name} ({time.monotonic() - started:.0f}s): {' '.join(command)}")
        if done.returncode:
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
