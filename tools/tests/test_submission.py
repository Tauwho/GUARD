"""Devpost draft checker (audit fix 7)."""

import importlib.util
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_submission", TOOLS / "check_submission.py")
submission = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(submission)

GOOD = """# GUARD - Gated Unexpired Authorization for Robotic Drives
Tagline: Stale decisions can't move the robot.
Category: Robotics & Medical
Repo: https://github.com/Tauwho/GUARD
Video: planned (#P4)
Built with: qnx, c++, python

## Inspiration
Text.
## What it does
Text.
## How it works
Text.
## QNX technologies (Evidence Map)
Text.
## How we built it
Text.
## Results
Functional results only.
## Challenges
Text.
## Accomplishments
Text.
## What we learned
Text.
## What's next
Text.
## Limits
Simulation only.
## AI-use disclosure
Text.
"""
SHOTS = """# Screenshots
- Diagram: docs/architecture.svg
- Dashboard: docs/video/dashboard.png
- GIF: docs/video/demo.gif
"""


def test_complete_draft_passes():
    assert submission.check_text(GOOD, SHOTS) == []


def test_missing_section_is_reported():
    errors = submission.check_text(GOOD.replace("## Limits\nSimulation only.\n", ""), SHOTS)
    assert any("Limits" in error for error in errors)


def test_tagline_over_60_characters_is_reported():
    errors = submission.check_text(GOOD.replace("Stale decisions can't move the robot.", "x" * 61), SHOTS)
    assert any("tagline" in error for error in errors)


def test_tbd_is_reported():
    assert any("TBD" in error for error in submission.check_text(GOOD + "\nTBD\n", SHOTS))


def test_screenshots_need_three_named_images():
    errors = submission.check_text(GOOD, "- one\n- two\n")
    assert any("3 bullet" in error for error in errors)
    assert any("dashboard" in error for error in errors)
