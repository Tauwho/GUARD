#!/usr/bin/env python3
"""Contracts are frozen at a tag; only additive files under contracts/ext/ may change (D26).

python tools/check_contract_compat.py --base contracts-v1.2
Checks:
  1. no file under contracts/ outside contracts/ext/ differs from the base tag;
  2. every contracts/ext/*.json parses; ext schemas declare no "required" list that names a
     property of the frozen schema they extend ("extends": "<file>");
  3. reason codes and message type ids added in ext files stay in the stream's reserved range;
  4. scenario safety invariants: for every scenario at the base tag, B2 keeps violations 0 and
     forbidden_reasons only grows.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
RANGES = {
    "A": {"code": (900, 949), "msg": (0x0100, 0x01FF)},
    "B": {"code": (950, 999), "msg": (0x0200, 0x02FF)},
}


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--base", default="contracts-v1.2")
    args = parser.parse_args()
    if git("rev-parse", "-q", "--verify", f"refs/tags/{args.base}").returncode:
        git("fetch", "-q", "--tags", "origin")
        if git("rev-parse", "-q", "--verify", f"refs/tags/{args.base}").returncode:
            print(f"contract-compat: FAIL tag {args.base} not found")
            return 1
    problems = []
    changed = git(
        "diff", "--name-only", args.base, "--", "contracts", ":(exclude)contracts/ext"
    ).stdout.split()
    problems += [f"{path}: frozen at {args.base}; use contracts/ext/ instead" for path in changed]
    for path in sorted((ROOT / "contracts/ext").glob("*.json")):
        stream = path.name.split("-", 1)[0]
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            problems.append(f"{path.name}: invalid JSON ({error})")
            continue
        base_name = data.get("extends")
        if base_name:
            base = json.loads((ROOT / "contracts" / base_name).read_text(encoding="utf-8"))
            frozen_props = set(base.get("properties", {}))
            for name in data.get("required", []):
                if name in frozen_props:
                    problems.append(f"{path.name}: makes frozen property '{name}' required")
        for code in data.get("x-reason-codes", {}).values():
            low, high = RANGES.get(stream, {"code": (0, -1)})["code"]
            if not low <= int(code) <= high:
                problems.append(
                    f"{path.name}: reason code {code} outside stream {stream} range {low}-{high}"
                )
        for msg in data.get("x-message-types", {}).values():
            low, high = RANGES.get(stream, {"msg": (0, -1)})["msg"]
            if not low <= int(str(msg), 0) <= high:
                problems.append(f"{path.name}: message type {msg} outside stream {stream} range")
    for scenario in sorted(git("ls-tree", "--name-only", args.base, "scenarios/").stdout.split()):
        if not re.match(r"scenarios/F\d+\.yaml$", scenario):
            continue
        before = yaml.safe_load(git("show", f"{args.base}:{scenario}").stdout) or {}
        now_path = ROOT / scenario
        if not now_path.exists():
            problems.append(f"{scenario}: removed")
            continue
        now = yaml.safe_load(now_path.read_text(encoding="utf-8")) or {}
        old_b2 = (before.get("expected") or {}).get("B2") or {}
        new_b2 = (now.get("expected") or {}).get("B2") or {}
        if old_b2.get("violations") == 0 and new_b2.get("violations") != 0:
            problems.append(f"{scenario}: B2 violations must stay 0")
        missing = set(old_b2.get("forbidden_reasons", [])) - set(
            new_b2.get("forbidden_reasons", [])
        )
        if missing:
            problems.append(f"{scenario}: B2 forbidden_reasons removed {sorted(missing)}")
    if problems:
        print("contract-compat: FAIL")
        print("\n".join("  " + p for p in problems))
        return 1
    print(f"contract-compat: PASS against {args.base}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
