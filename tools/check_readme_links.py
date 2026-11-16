#!/usr/bin/env python3
"""Fail if README.md has a broken local link or evidence reference (P1).

Checks:
- every relative Markdown link or image target resolves to a file or directory;
- every `evidence/<dir>` reference resolves and holds a manifest.json;
- every scenario id F0-F8 named in the README exists under scenarios/.

Exit 0 if clean, 1 with one line per problem. Never uses the network.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
LINK = re.compile(r"!?\[[^\]]*\]\(([^)\s]+)\)")
EVIDENCE = re.compile(r"evidence/([A-Za-z0-9._-]+)")
SCENARIO = re.compile(r"\bF([0-8])\b")


def problems(path):
    out = []
    text = path.read_text(encoding="utf-8")
    for number, line in enumerate(text.splitlines(), 1):
        for target in LINK.findall(line):
            if re.match(r"^[a-z]+:", target) or target.startswith("#"):
                continue
            file_part = target.split("#", 1)[0]
            if file_part and not (path.parent / file_part).exists():
                out.append(f"README.md:{number}: link target missing: {target}")
        for bundle in EVIDENCE.findall(line):
            manifest = path.parent / "evidence" / bundle / "manifest.json"
            if not manifest.exists():
                out.append(f"README.md:{number}: evidence bundle has no manifest: {bundle}")
    for scenario in sorted({m.group(1) for m in SCENARIO.finditer(text)}):
        if not list((ROOT / "scenarios").glob(f"F{scenario}.*")):
            out.append(f"README.md: scenario F{scenario} has no file under scenarios/")
    return out


def main():
    if not README.exists():
        print("README.md: missing")
        return 1
    found = problems(README)
    for line in found:
        print(line)
    print(f"readme-links: {'FAIL' if found else 'PASS'}")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
