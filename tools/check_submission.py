#!/usr/bin/env python3
"""Check the Devpost draft against the REF-DEVPOST requirements (#8, audit 7).

The upload itself is human PO only; this only proves the draft is complete enough
to paste. No TBD survives P6, the tagline fits 60 characters, every required
section exists, and the screenshots list names a diagram, the dashboard and a GIF.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = (
    "Inspiration",
    "What it does",
    "How it works",
    "QNX technologies",
    "How we built it",
    "Results",
    "Challenges",
    "Accomplishments",
    "What we learned",
    "What's next",
    "Limits",
    "AI-use disclosure",
)


def section_pattern(name):
    escaped = re.escape(name).replace("'", "['\u2019]")
    return rf"^#{{1,6}}\s+{escaped}\b"


def check_text(description, screenshots):
    errors = []
    first = description.lstrip().splitlines()[0] if description.strip() else ""
    if "GUARD" not in first:
        errors.append("title: the first line must name GUARD")
    for name in REQUIRED:
        if not re.search(section_pattern(name), description, re.MULTILINE | re.IGNORECASE):
            errors.append(f"missing section: {name}")
    tagline = re.search(
        r"^\s*(?:[-*]\s*)?\**Tagline:\**\s*(.+?)\s*$", description, re.MULTILINE | re.IGNORECASE
    )
    if not tagline:
        errors.append("missing Tagline: line")
    else:
        text = tagline.group(1).strip("* ")
        if len(text) > 60:
            errors.append(f"tagline is {len(text)} characters, limit is 60")
    for label, pattern in (
        ("Repo", r"^\s*(?:[-*]\s*)?\**Repo:\**\s*(\S+)"),
        ("Video", r"^\s*(?:[-*]\s*)?\**Video:\**\s*(.+)$"),
        ("Category", r"^\s*(?:[-*]\s*)?\**Category:\**\s*(.+)$"),
        ("Built with", r"^\s*(?:[-*]\s*)?\**Built with:\**\s*(.+)$"),
    ):
        match = re.search(pattern, description, re.MULTILINE | re.IGNORECASE)
        if not match:
            errors.append(f"missing {label}: line")
            continue
        value = match.group(1).strip()
        if label == "Repo" and "github.com/Tauwho/GUARD" not in value:
            errors.append("Repo: must point to github.com/Tauwho/GUARD")
        if label == "Video" and not ("http" in value.lower() or "planned" in value.lower()):
            errors.append("Video: must be a URL or 'planned'")
        if label == "Category" and "Robotics" not in value:
            errors.append("Category: must be Robotics & Medical")
    if re.search(r"\bTBD\b", description):
        errors.append("description still contains TBD")
    bullets = [line for line in screenshots.splitlines() if line.strip().startswith(("- ", "* "))]
    if len(bullets) < 3:
        errors.append("screenshots: need at least 3 bullet entries")
    for label in ("diagram", "dashboard", "gif"):
        if label not in screenshots.lower():
            errors.append(f"screenshots: missing {label}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=str(ROOT))
    args = parser.parse_args()
    root = Path(args.root)
    description = root / "docs/devpost/description.md"
    screenshots = root / "docs/devpost/screenshots.md"
    missing = [
        str(path.relative_to(root)) for path in (description, screenshots) if not path.exists()
    ]
    if missing:
        print("submission: FAIL missing " + ", ".join(missing))
        return 1
    errors = check_text(
        description.read_text(encoding="utf-8"), screenshots.read_text(encoding="utf-8")
    )
    if errors:
        print("submission: FAIL")
        for error in errors:
            print(f"  {error}")
        return 1
    print("submission: PASS (description, screenshots, video link, AI-use; upload stays human)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
