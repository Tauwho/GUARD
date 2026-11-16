#!/usr/bin/env python3
"""Final package gate (P4): `make package-check` runs this after `make check`.

One line per item, PASS or FAIL, then `package-check: PASS` or `FAIL`. Items:

- readme-current   `tools/build_readme.py --check` (generated blocks match main)
- readme-links     `tools/check_readme_links.py`
- readme-no-tbd    no `TBD` in README.md
- readme-results   README Results cite the Linux host-evaluation run
- readme-video     README links the demo video (an http(s) URL on a line naming the video)
- captions         docs/video/captions.srt exists and its last cue ends before 3:00
- demo-commands    `tools/check_demo_commands.py`
- architecture-svg `tools/check_architecture_svg.py`
- submission       `tools/check_submission.py`
- no-qnx-logo      no image file in the repository is named like a QNX logo (D11)

Exit 0 only if every item passes. Never uses the network.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
CAPTIONS = ROOT / "docs/video/captions.srt"
VIDEO_LIMIT_S = 180
CUE = re.compile(r"(\d\d):(\d\d):(\d\d)[,.](\d{3})\s*-->\s*(\d\d):(\d\d):(\d\d)[,.](\d{3})")
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp"}


def readme_no_tbd(text):
    hits = [n for n, line in enumerate(text.splitlines(), 1) if re.search(r"\bTBD\b", line)]
    return (not hits), ("no TBD" if not hits else f"TBD on line(s) {hits}")


def readme_results(text):
    ok = re.search(r"host \(Linux\) harness", text) and re.search(
        r"github\.com/Tauwho/GUARD/actions/runs/\d+", text)
    return bool(ok), "Results cite a host (Linux) run" if ok else "Results lack the Linux run link"


def readme_video(text):
    for line in text.splitlines():
        if re.search(r"(?i)video", line) and re.search(r"https?://\S+", line):
            return True, "video linked"
    return False, "no video URL in README (planned (P4))"


def captions(path=CAPTIONS):
    if not path.exists():
        return False, f"{path.relative_to(ROOT).as_posix()} missing"
    ends = [int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000
            for *_, h, m, s, ms in CUE.findall(path.read_text(encoding="utf-8"))]
    if not ends:
        return False, "captions.srt has no cues"
    if max(ends) >= VIDEO_LIMIT_S:
        return False, f"last cue ends at {max(ends):.1f} s, limit is under 180 s"
    return True, f"{len(ends)} cues, last ends at {max(ends):.1f} s"


def no_qnx_logo(root=ROOT):
    hits = [p.relative_to(root).as_posix() for p in root.rglob("*")
            if p.suffix.lower() in IMAGE_SUFFIXES and ".git" not in p.parts
            and re.search(r"(?i)qnx.*logo|logo.*qnx", p.name)]
    return (not hits), ("none" if not hits else ", ".join(hits))


def items():
    text = README.read_text(encoding="utf-8")
    return [
        ("readme-current", lambda: tool_args("build_readme.py", "--check")),
        ("readme-links", lambda: tool_args("check_readme_links.py")),
        ("readme-no-tbd", lambda: readme_no_tbd(text)),
        ("readme-results", lambda: readme_results(text)),
        ("readme-video", lambda: readme_video(text)),
        ("captions", captions),
        ("demo-commands", lambda: tool_args("check_demo_commands.py")),
        ("architecture-svg", lambda: tool_args("check_architecture_svg.py")),
        ("submission", lambda: tool_args("check_submission.py")),
        ("no-qnx-logo", no_qnx_logo),
    ]


def tool_args(script, *args):
    result = subprocess.run([sys.executable, f"tools/{script}", *args], cwd=ROOT,
                            capture_output=True, encoding="utf-8", errors="replace")
    lines = (result.stdout + result.stderr).strip().splitlines()
    return result.returncode == 0, lines[-1] if lines else "no output"


def main():
    failed = []
    for name, check in items():
        ok, detail = check()
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}")
        if not ok:
            failed.append(name)
    print(f"package-check: {'PASS' if not failed else 'FAIL (' + ', '.join(failed) + ')'}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
