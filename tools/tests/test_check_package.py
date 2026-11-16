"""Tests for tools/check_package.py (P4). Fixture text and temp files only."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_package as pkg  # noqa: E402

RESULTS = (
    "<!-- gen:results -->\nSource: host (Linux) harness, "
    "[run](https://github.com/Tauwho/GUARD/actions/runs/1)\n<!-- gen:results-end -->\n"
)


def write(path, text, encoding="utf-8"):
    path.write_text(text, encoding=encoding, newline="")
    return path


def test_tbd_is_found_with_its_line():
    assert pkg.readme_no_tbd("ok\nstill TBD here\n") == (False, "TBD on line(s) [2]")
    assert not pkg.readme_no_tbd("two TBDs left\n")[0]
    assert pkg.readme_no_tbd("tbd lowercase is prose\n")[0]


def test_results_must_be_inside_the_generated_block():
    assert pkg.readme_results(RESULTS)[0]
    outside = "host (Linux) harness https://github.com/Tauwho/GUARD/actions/runs/1\n"
    assert pkg.readme_results(outside) == (False, "no gen:results block")


def test_video_needs_a_video_host_url():
    assert not pkg.readme_video("Video and GIF: planned (P4)\n")[0]
    assert pkg.readme_video("Video: https://youtu.be/abc\n")[0]
    storyboard = "video [storyboard](https://github.com/Tauwho/GUARD/blob/main/docs/video/x.md)"
    assert not pkg.readme_video(storyboard)[0]


def test_video_duration(tmp_path):
    assert not pkg.video_duration(tmp_path / "missing.txt")[0]
    assert pkg.video_duration(write(tmp_path / "d.txt", "2:58\n")) == (True, "video is 178 s")
    assert not pkg.video_duration(write(tmp_path / "d.txt", "3:00"))[0]
    assert pkg.video_duration(write(tmp_path / "d.txt", "2 min"))[1] == "duration.txt must be m:ss"


def test_captions_within_video_duration(tmp_path):
    dur = write(tmp_path / "d.txt", "2:50")
    srt = tmp_path / "c.srt"
    assert not pkg.captions(srt, dur)[0]
    write(srt, "1\n00:00:00,000 --> 00:02:49,500\nEnd.\n")
    assert pkg.captions(srt, dur) == (True, "1 cues, last ends at 169.5 s")
    write(srt, "1\n00:00:00,000 --> 00:02:55,000\nAfter the video ends.\n")
    assert not pkg.captions(srt, dur)[0]


def test_captions_fail_closed_on_malformed_or_reversed_cues(tmp_path):
    srt = tmp_path / "c.srt"
    missing = tmp_path / "none.txt"
    write(srt, "1\n00:00:00,000 --> 00:00:05,000\na\n\n2\n00:02:59.5 --> 00:03:10,000\nb\n")
    assert "malformed timing line" in pkg.captions(srt, missing)[1]
    write(srt, "1\n00:00:09,000 --> 00:00:05,000\na\n")
    assert "ends before it starts" in pkg.captions(srt, missing)[1]
    write(srt, "1\r\n00:00:00,000 --> 01:00:00,000\r\nx\r\n")
    assert not pkg.captions(srt, missing)[0]
    write(srt, "1\n00:00:00,000 --> 00:02:59,000\nx\n", encoding="utf-8-sig")
    assert pkg.captions(srt, missing)[0]
    write(srt, "1\n00:00:00,000 --> 00:03:00,000\nx\n")
    assert pkg.captions(srt, missing)[0]  # exactly 180.0 s is allowed for captions


def test_captions_survive_non_utf8_bytes(tmp_path):
    srt = tmp_path / "c.srt"
    srt.write_bytes(b"1\n00:00:00,000 --> 00:00:05,000\ncaf\xe9\n")
    assert pkg.captions(srt, tmp_path / "none.txt")[0]


def test_submission_video_must_be_a_url(tmp_path):
    assert not pkg.submission_video(write(tmp_path / "d.md", "- **Video:** planned (P4)\n"))[0]
    assert pkg.submission_video(write(tmp_path / "d.md", "- **Video:** https://youtu.be/x\n"))[0]


def test_qnx_logo_files_are_named():
    root = pkg.ROOT
    assert pkg.no_qnx_logo([root / "docs/architecture.svg"]) == (True, "none")
    assert pkg.no_qnx_logo([root / "docs/QNX_Logo.png"]) == (False, "docs/QNX_Logo.png")
    assert not pkg.no_qnx_logo([root / "img/qnx.ico"])[0]
    assert pkg.no_qnx_logo([root / "qnx/common/README.md"])[0]
