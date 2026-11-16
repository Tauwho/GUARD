#!/usr/bin/env python3
"""Check docs/architecture.svg labels against docs/architecture.md (P5).

The diagram is hand-written, so its labels can drift from the reference:
- every contract id (`C1`..`C14`) in a label must be a row of REF-CONTRACTS;
- every `prio N` / `inherits N` value must appear in the REF-PRIORITIES table;
- every `checks A-B` range must stay within the checkers REF-CHECKERS numbers.

P's check only: it runs from `tools/check_package.py` (P4) and P's own runs, never as a
`make check` stage, so A or B editing architecture.md cannot fail their CI through it.
Exit 0 when clean, 1 with one line per problem. Never uses the network.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SVG = ROOT / "docs/architecture.svg"
DOC = ROOT / "docs/architecture.md"


def section(doc, name):
    match = re.search(rf"^## {name}\n(.*?)(?=^## |\Z)", doc, re.S | re.M)
    return match.group(1) if match else ""


def labels(svg):
    return [re.sub(r"<[^>]+>", "", t) for t in re.findall(r"<text[^>]*>(.*?)</text>", svg)]


def problems(svg, doc):
    out = []
    contracts = set(re.findall(r"^\| (C\d+) \|", section(doc, "REF-CONTRACTS"), re.M))
    priorities = section(doc, "REF-PRIORITIES")
    prio_values = set(re.findall(r"\|\s*(?:Inherits[^|]*\()?(\d+)\)?\s*\|", priorities))
    checkers = [int(n) for n in re.findall(r"^\| (\d+) \|", section(doc, "REF-CHECKERS"), re.M)]
    highest = max(checkers, default=0)
    if not contracts:
        out.append("architecture.md: REF-CONTRACTS table not found")
    for text in labels(svg):
        for first, last in re.findall(r"\bC(\d+)-C(\d+)\b", text):
            for n in range(int(first), int(last) + 1):
                if f"C{n}" not in contracts:
                    out.append(f"architecture.svg: '{text}': C{n} is not in REF-CONTRACTS")
        for cid in re.findall(r"(?<![-\w])(C\d+)\b(?!-C)", text):
            if cid not in contracts:
                out.append(f"architecture.svg: '{text}': {cid} is not in REF-CONTRACTS")
        for value in re.findall(r"\b(?:prio|inherits)\s+(\d+)", text):
            if value not in prio_values:
                out.append(f"architecture.svg: '{text}': priority {value} not in REF-PRIORITIES")
        for first, last in re.findall(r"\bchecks (\d+)-(\d+)", text):
            if int(first) < 1 or int(last) > highest:
                out.append(f"architecture.svg: '{text}': checks {first}-{last} outside 1-{highest}")
    return out


def main():
    found = problems(SVG.read_text(encoding="utf-8"), DOC.read_text(encoding="utf-8"))
    for problem in found:
        print(problem)
    if found:
        print(f"architecture-svg: FAIL ({len(found)} problem(s))")
        return 1
    print("architecture-svg: PASS (contract ids, priorities and checker range match)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
