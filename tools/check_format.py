"""Check tracked C/C++ sources with clang-format, excluding downloaded dependencies.

Exit codes: 0 formatted, 1 not formatted, 3 clang-format is not installed.
CI (env CI or GITHUB_ACTIONS set) treats a missing clang-format as a failure;
tools/check.py reports exit 3 as SKIPPED on a local machine only.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

MISSING = 3


def in_ci(env=None):
    env = os.environ if env is None else env
    return bool(env.get("CI") or env.get("GITHUB_ACTIONS"))


def main():
    root = Path(__file__).resolve().parents[1]
    if shutil.which("clang-format") is None:
        if in_ci():
            print("format: FAIL clang-format not found (required in CI)")
            return 1
        print("format: SKIPPED clang-format not found on this machine (CI still runs it)")
        return MISSING
    files = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode().split("\0")
    sources = [p for p in files if Path(p).suffix in {".h", ".hpp", ".c", ".cpp", ".cc", ".cxx"}]
    if not sources:
        return 0
    return subprocess.run(["clang-format", "--dry-run", "--Werror", *sources], cwd=root).returncode


if __name__ == "__main__":
    sys.exit(main())
