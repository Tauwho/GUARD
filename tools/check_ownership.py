#!/usr/bin/env python3
"""Fail if a PR changes files its stream does not own (OWNERSHIP.yaml, D26, D27).

CI:    python <base copy of this file> --root . --base origin/main --policy-from-base
           --head-ref "$HEAD_REF" --labels "$PR_LABELS" --title "$PR_TITLE"
Local: PR_LABELS=phase0-override python tools/check_ownership.py --base origin/main --local
       (uses the current branch name; PR_LABELS/PR_TITLE are read from the environment,
       so a phase0-override branch passes locally exactly as it does in CI)

D27 tooling-fix: any stream may fix a defect in P's tooling while P is away. The PR
title starts with `tooling-fix:`, every changed path is a non-frozen P file matching
TOOLING_FIX_PATHS, and the gate files themselves are never in the change. CI runs the
base branch's copy of this script with the base branch's OWNERSHIP.yaml
(--policy-from-base), so a PR cannot loosen its own gate.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
TOOLING_FIX_PREFIX = "tooling-fix:"
TOOLING_FIX_PATHS = (
    "tools/**",
    ".github/workflows/**",
    ".github/required-checks.txt",
    ".clang-format",
)
# Gate files: a tooling-fix PR may not change the checks that judge it.
TOOLING_FIX_EXCLUDED = (
    "tools/check_ownership.py",
    "tools/check_contract_compat.py",
    "tools/tests/test_check_ownership.py",
    ".github/workflows/governance.yml",
)


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


TOOLING_FIX_REGEX = [glob_regex(p) for p in TOOLING_FIX_PATHS]


def owner_of(path, rules):
    for rule in rules:
        if rule["regex"].match(path):
            return rule["owner"], bool(rule.get("frozen"))
    return "P", False


def compile_rules(config):
    return [dict(rule, regex=glob_regex(rule["path"])) for rule in config["rules"]]


def is_tooling_fix_path(path, rules):
    owner, frozen = owner_of(path, rules)
    return (
        owner == "P"
        and not frozen
        and path not in TOOLING_FIX_EXCLUDED
        and any(regex.match(path) for regex in TOOLING_FIX_REGEX)
    )


def evaluate(stream, changed, rules, labels=(), title=""):
    """Return (ok, lines). Pure, so tests can simulate any PR."""
    if "phase0-override" in labels and stream == "P":
        return True, [f"ownership: PASS (phase0-override on a P branch), {len(changed)} files"]
    if title.strip().lower().startswith(TOOLING_FIX_PREFIX):
        bad = [p for p in changed if not is_tooling_fix_path(p, rules)]
        if not changed:
            return False, ["ownership: FAIL tooling-fix PR changes no files"]
        if bad:
            lines = ["ownership: FAIL tooling-fix (D27) may change only P tooling files, not:"]
            lines += [f"  {p}" for p in bad]
            lines.append(
                "Split the change: tooling paths in a tooling-fix PR, the rest in your own PR."
            )
            return False, lines
        return True, [f"ownership: PASS tooling-fix (D27) by stream {stream}, {len(changed)} files"]
    bad = []
    for path in changed:
        owner, frozen = owner_of(path, rules)
        if owner != stream:
            bad.append(f"{path} (owned by {owner})")
        elif frozen:
            bad.append(f"{path} (frozen after Phase 0; see contracts/ext/README.md)")
    if bad:
        lines = [f"ownership: FAIL stream {stream} changed files it may not change:"]
        lines += ["  " + line for line in bad]
        lines.append("Fix: move the change to a file you own, use an adapter,")
        lines.append(
            "add an additive file under contracts/ext/, or (P tooling only) a tooling-fix PR."
        )
        return False, lines
    return True, [f"ownership: PASS stream {stream}, {len(changed)} files"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default="origin/main")
    parser.add_argument("--head-ref", default="")
    parser.add_argument("--labels", default=os.environ.get("PR_LABELS", ""))
    parser.add_argument("--title", default=os.environ.get("PR_TITLE", ""))
    parser.add_argument("--local", action="store_true")
    parser.add_argument("--root", default=str(ROOT), help="repository to check")
    parser.add_argument(
        "--policy-from-base", action="store_true", help="read OWNERSHIP.yaml from --base"
    )
    args = parser.parse_args()
    root = Path(args.root).resolve()

    def git(*git_args):
        return subprocess.run(
            ["git", *git_args], cwd=root, capture_output=True, text=True, check=True
        ).stdout

    if args.policy_from_base:
        config = yaml.safe_load(git("show", f"{args.base}:OWNERSHIP.yaml"))
    else:
        config = yaml.safe_load((root / "OWNERSHIP.yaml").read_text(encoding="utf-8"))
    rules = compile_rules(config)
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
    ok, lines = evaluate(stream, changed, rules, args.labels.split(","), args.title)
    print("\n".join(lines))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
