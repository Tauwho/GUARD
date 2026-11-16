"""Check tracked C/C++ sources, excluding downloaded dependencies."""

import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode().split("\0")
sources = [p for p in files if Path(p).suffix in {".h", ".hpp", ".c", ".cpp", ".cc", ".cxx"}]
if sources:
    subprocess.run(["clang-format", "--dry-run", "--Werror", *sources], cwd=root, check=True)
