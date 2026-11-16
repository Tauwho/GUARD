#!/usr/bin/env python3
"""Final package gate (P4): `make package-check` runs this after `make check` and the
fresh-clone demo.

One line per item, PASS or FAIL, then `package-check: PASS` or `FAIL`. Items:

- readme-current    `tools/build_readme.py --check` (generated blocks match main)
- readme-links      `tools/check_readme_links.py`
- readme-no-tbd     no `TBD` in README.md
- readme-results    the generated Results block cites a Linux host-evaluation run
- readme-video      README links the demo video (a YouTube or Vimeo URL on a video line)
- video-duration    docs/video/duration.txt (`m:ss`, written at export; the video file
                    itself is never committed) is under 3:00
- captions          docs/video/captions.srt: every timing line parses, every cue ends
                    after it starts, and the last cue ends within the video duration
- demo-commands     `tools/check_demo_commands.py`
- architecture-svg  `tools/check_architecture_svg.py`
- submission        `tools/check_submission.py`, plus a video URL (not `planned`) in
                    the submission draft
- release-tag       tag `v1.0-rc1` exists (P3)
- no-qnx-logo       no tracked image file is named like a QNX logo (D11)

Exit 0 only if every item passes. Never uses the network.
"""

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
CAPTIONS = ROOT / "docs/video/captions.srt"
DURATION = ROOT / "docs/video/duration.txt"
DESCRIPTION = ROOT / "docs/devpost/description.md"
VIDEO_LIMIT_S = 180
TIMESTAMP = r"(\d{2}):([0-5]\d):([0-5]\d),(\d{3})"
CUE = re.compile(rf"^\s*{TIMESTAMP}\s*-->\s*{TIMESTAMP}\s*$")
VIDEO_URL = re.compile(r"https?://(?:youtu\.be|(?:www\.)?youtube\.com|(?:www\.)?vimeo\.com)/\S+")
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp", ".ico", ".bmp",
                  ".tif", ".tiff", ".pdf", ".ai", ".eps", ".psd"}
CHILD_ENV = {**os.environ, "PYTHONUTF8": "1"}


def read(path):
    return path.read_bytes().decode("utf-8-sig", errors="replace")


def tool(script, *args):
    result = subprocess.run([sys.executable, f"tools/{script}", *args], cwd=ROOT,
                            capture_output=True, encoding="utf-8", errors="replace",
                            env=CHILD_ENV)
    lines = (result.stdout + result.stderr).strip().splitlines()
    return result.returncode == 0, lines[-1] if lines else "no output"


def readme_no_tbd(text):
    hits = [n for n, line in enumerate(text.splitlines(), 1) if re.search(r"\bTBD", line)]
    return (not hits), ("no TBD" if not hits else f"TBD on line(s) {hits}")


def readme_results(text):
    block = re.search(r"<!-- gen:results -->(.*?)<!-- gen:results-end -->", text, re.S)
    if not block:
        return False, "no gen:results block"
    ok = "host (Linux) harness" in block.group(1) and re.search(
        r"github\.com/Tauwho/GUARD/actions/runs/\d+", block.group(1))
    return bool(ok), "Results cite a host (Linux) run" if ok else "Results lack the Linux run link"


def readme_video(text):
    for line in text.splitlines():
        if re.search(r"(?i)video", line) and VIDEO_URL.search(line):
            return True, "video linked"
    return False, "no YouTube/Vimeo URL on a video line in README (planned (P4))"


def seconds(h, m, s, ms):
    return int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000


def duration(path=DURATION):
    if not path.exists():
        return None, "docs/video/duration.txt missing (write m:ss at export)"
    match = re.fullmatch(r"\s*(\d+):([0-5]\d)\s*", read(path))
    if not match:
        return None, "duration.txt must be m:ss"
    return int(match.group(1)) * 60 + int(match.group(2)), "ok"


def video_duration(path=DURATION):
    value, detail = duration(path)
    if value is None:
        return False, detail
    if value >= VIDEO_LIMIT_S:
        return False, f"video is {value} s, limit is under 180 s"
    return True, f"video is {value} s"


def captions(path=CAPTIONS, duration_path=DURATION):
    if not path.exists():
        return False, "docs/video/captions.srt missing"
    timing_lines = [line for line in read(path).splitlines() if "-->" in line]
    if not timing_lines:
        return False, "captions.srt has no cues"
    ends = []
    for line in timing_lines:
        match = CUE.match(line)
        if not match:
            return False, f"malformed timing line: {line.strip()!r}"
        start, end = seconds(*match.groups()[:4]), seconds(*match.groups()[4:])
        if end <= start:
            return False, f"cue ends before it starts: {line.strip()!r}"
        ends.append(end)
    limit, _ = duration(duration_path)
    limit = VIDEO_LIMIT_S if limit is None else min(limit, VIDEO_LIMIT_S)
    if max(ends) > limit:
        return False, f"last cue ends at {max(ends):.1f} s, after the video ({limit} s)"
    return True, f"{len(ends)} cues, last ends at {max(ends):.1f} s"


def submission_video(path=DESCRIPTION):
    if not path.exists():
        return False, "docs/devpost/description.md missing"
    match = re.search(r"^\s*(?:[-*]\s*)?\**Video:\**\s*(.+)$", read(path), re.M | re.I)
    if not match or not VIDEO_URL.search(match.group(1)):
        return False, "submission Video: is not a YouTube/Vimeo URL yet"
    return True, "submission links the video"


def submission():
    ok, detail = tool("check_submission.py")
    if not ok:
        return ok, detail
    return submission_video()


def release_tag():
    result = subprocess.run(["git", "rev-parse", "-q", "--verify", "refs/tags/v1.0-rc1"],
                            cwd=ROOT, capture_output=True, text=True)
    ok = result.returncode == 0
    return ok, f"v1.0-rc1 = {result.stdout.strip()[:7]}" if ok else "tag v1.0-rc1 missing (P3)"


def tracked_files(root=ROOT):
    result = subprocess.run(["git", "ls-files", "-z"], cwd=root, capture_output=True)
    return [root / name for name in result.stdout.decode("utf-8", "replace").split("\0") if name]


def logo_like(path):
    stem = path.stem.lower()
    return path.suffix.lower() in IMAGE_SUFFIXES and "qnx" in stem and (
        stem == "qnx" or re.search(r"logo|mark|brand|badge", stem))


def no_qnx_logo(files=None):
    files = tracked_files() if files is None else files
    hits = [p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.name
            for p in files if logo_like(p)]
    return (not hits), ("none" if not hits else ", ".join(hits))


def items():
    text = read(README)
    return [
        ("readme-current", lambda: tool("build_readme.py", "--check")),
        ("readme-links", lambda: tool("check_readme_links.py")),
        ("readme-no-tbd", lambda: readme_no_tbd(text)),
        ("readme-results", lambda: readme_results(text)),
        ("readme-video", lambda: readme_video(text)),
        ("video-duration", video_duration),
        ("captions", captions),
        ("demo-commands", lambda: tool("check_demo_commands.py")),
        ("architecture-svg", lambda: tool("check_architecture_svg.py")),
        ("submission", submission),
        ("release-tag", release_tag),
        ("no-qnx-logo", no_qnx_logo),
    ]


def main():
    failed = []
    for name, check in items():
        try:
            ok, detail = check()
        except Exception as error:  # noqa: BLE001 - one line per item, always
            ok, detail = False, f"{type(error).__name__}: {error}"
        print(f"{'PASS' if ok else 'FAIL'} {name}: {detail}")
        if not ok:
            failed.append(name)
    print(f"package-check: {'PASS' if not failed else 'FAIL (' + ', '.join(failed) + ')'}")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
