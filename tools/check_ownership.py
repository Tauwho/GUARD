#!/usr/bin/env python3
"""Fail if a PR changes files its stream does not own (OWNERSHIP.yaml, D26).

CI:    python tools/check_ownership.py --base origin/main
           --head-ref "$GITHUB_HEAD_REF" --labels "$PR_LABELS"
Local: python tools/check_ownership.py --base origin/main --local
       (uses the current branch name)
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]


def glob_regex(pattern):
    out = ""
    i = 0
    while i < len(pattern):
        if pattern.startswith("**", i):
            out += ".*"
            i += 2
        elif pattern[i] == "*":
            out += "[^/]*"
            i += 1
        else:
            out += re.escape(pattern[i])
            i += 1
    return re.compile("^" + out + "$")


def owner_of(path, rules):
    for rule in rules:
        if rule["regex"].match(path):
            return rule["owner"], bool(rule.get("frozen"))
    return "P", False


def git(*args):
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--head-ref", default="")
    parser.add_argument("--labels", default="")
    parser.add_argument("--local", action="store_true")
    args = parser.parse_args()
    config = yaml.safe_load((ROOT / "OWNERSHIP.yaml").read_text(encoding="utf-8"))
    rules = [dict(rule, regex=glob_regex(rule["path"])) for rule in config["rules"]]
    head = args.head_ref or (git("rev-parse", "--abbrev-ref", "HEAD").strip() if args.local else "")
    if head in ("", "HEAD", "main"):
        print("ownership: not on a stream branch (main or detached); nothing to check")
        return 0
    stream = next(
        (name for name, s in config["streams"].items() if head.startswith(s["branch_prefix"])), None
    )
    if stream is None:
        prefixes = ", ".join(s["branch_prefix"] for s in config["streams"].values())
        print(f"ownership: FAIL branch '{head}' has no stream prefix ({prefixes})")
        return 1
    try:
        changed = git("diff", "--name-only", f"{args.base}...HEAD").split()
    except subprocess.CalledProcessError as error:
        print(f"ownership: FAIL cannot diff against {args.base}: {error.stderr.strip()}")
        return 1
    override = "phase0-override" in args.labels.split(",") and stream == "P"
    if override:
        print(f"ownership: PASS (phase0-override on a P branch), {len(changed)} files")
        return 0
    bad = []
    for path in changed:
        owner, frozen = owner_of(path, rules)
        if owner != stream:
            bad.append(f"{path} (owned by {owner})")
        elif frozen and not override:
            bad.append(f"{path} (frozen after Phase 0; see contracts/ext/README.md)")
    if bad:
        print(f"ownership: FAIL stream {stream} changed files it may not change:")
        print("\n".join("  " + line for line in bad))
        print("Fix: move the change to a file you own, use an adapter,")
        print("or add an additive file under contracts/ext/.")
        return 1
    print(f"ownership: PASS stream {stream}, {len(changed)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
