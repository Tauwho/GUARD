"""Tests for tools/check_package.py (P4). Fixture text and temp files only."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_package as pkg  # noqa: E402


def test_tbd_is_found_with_its_line():
    assert pkg.readme_no_tbd("ok\nstill TBD here\n") == (False, "TBD on line(s) [2]")
    assert pkg.readme_no_tbd("TBDs are words? no: tbd lowercase is fine\n")[0]


def test_results_need_the_linux_label_and_a_run_link():
    good = "Source: host (Linux) harness, [run](https://github.com/Tauwho/GUARD/actions/runs/1)"
    assert pkg.readme_results(good)[0]
    assert not pkg.readme_results("Source: host (Linux) harness")[0]


def test_video_needs_a_url_on_a_video_line():
    assert not pkg.readme_video("Video and GIF: planned (P4)\n")[0]
    assert pkg.readme_video("Video: https://youtu.be/abc\n")[0]
    assert not pkg.readme_video("Results [run](https://github.com/x)\n")[0]


def test_captions_must_end_before_three_minutes(tmp_path, monkeypatch):
    monkeypatch.setattr(pkg, "ROOT", tmp_path)
    srt = tmp_path / "captions.srt"
    assert not pkg.captions(srt)[0]
    srt.write_text("1\n00:00:00,000 --> 00:02:58,500\nEnd.\n", encoding="utf-8")
    assert pkg.captions(srt) == (True, "1 cues, last ends at 178.5 s")
    srt.write_text("1\n00:02:50,000 --> 00:03:00,000\nToo long.\n", encoding="utf-8")
    assert not pkg.captions(srt)[0]
    srt.write_text("no cues\n", encoding="utf-8")
    assert pkg.captions(srt) == (False, "captions.srt has no cues")


def test_qnx_logo_files_are_named(tmp_path):
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs/diagram.svg").write_text("<svg/>", encoding="utf-8")
    assert pkg.no_qnx_logo(tmp_path) == (True, "none")
    (tmp_path / "docs/QNX_Logo.png").write_bytes(b"")
    assert pkg.no_qnx_logo(tmp_path) == (False, "docs/QNX_Logo.png")
