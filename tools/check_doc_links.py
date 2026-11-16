#!/usr/bin/env python3
"""Fail if docs/streams/*.md has a broken reference (audit fix 5).

Checks:
- every relative Markdown link `[text](path)` or `[text](path#anchor)` resolves to a file;
- every section reference `§N` resolves to a heading `## N.` in the same file, or in the
  backticked `*.md` file named just before it (for example `docs/streams/README.md` §2).
Exit 0 if clean, 1 with one line per broken reference.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")
SECTION = re.compile(r"§(\d+)")
NAMED = re.compile(r"`([^`]+\.md)`\s*$")


def headings(path):
    text = path.read_text(encoding="utf-8")
    return {m.group(1) for m in re.finditer(r"^#{1,6}\s+(\d+)[.\s]", text, re.MULTILINE)}


def problems(path):
    out = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        for target in LINK.findall(line):
            if re.match(r"^[a-z]+:", target) or target.startswith("#"):
                continue
            file_part = target.split("#", 1)[0]
            if file_part and not (path.parent / file_part).exists():
                out.append(f"{path.relative_to(ROOT)}:{number}: link target missing: {target}")
        for match in SECTION.finditer(line):
            named = NAMED.search(line[: match.start()])
            ref = path
            if named:
                candidate = ROOT / named.group(1)
                ref = candidate if candidate.exists() else path.parent / named.group(1)
                if not ref.exists():
                    out.append(f"{path.relative_to(ROOT)}:{number}: file missing: {named.group(1)}")
                    continue
            if match.group(1) not in headings(ref):
                out.append(
                    f"{path.relative_to(ROOT)}:{number}: §{match.group(1)} has no heading "
                    f"'## {match.group(1)}.' in {ref.relative_to(ROOT)}"
                )
    return out


def main(paths=None):
    files = paths or sorted((ROOT / "docs" / "streams").glob("*.md"))
    found = [p for f in files for p in problems(Path(f))]
    for line in found:
        print(line)
    print(f"doc-links: {'FAIL' if found else 'PASS'} ({len(files)} files)")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
