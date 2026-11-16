"""check_doc_links.py catches missing link targets and dangling § references (audit fix 5)."""

import importlib.util
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("check_doc_links", TOOLS / "check_doc_links.py")
links = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(links)


def write(tmp_path, monkeypatch, text):
    monkeypatch.setattr(links, "ROOT", tmp_path)
    doc = tmp_path / "docs" / "streams" / "BRIEF-X.md"
    doc.parent.mkdir(parents=True)
    doc.write_text(text, encoding="utf-8")
    return doc


def test_repo_stream_docs_are_clean():
    assert links.main() == 0


def test_missing_link_target_fails(tmp_path, monkeypatch):
    doc = write(tmp_path, monkeypatch, "## 1. Loop\nSee [list](NOPE.md).\n")
    assert any("link target missing" in p for p in links.problems(doc))


def test_dangling_section_reference_fails(tmp_path, monkeypatch):
    doc = write(tmp_path, monkeypatch, "## 1. Loop\nUse the checklist in §6.\n")
    assert any("§6 has no heading" in p for p in links.problems(doc))


def test_section_reference_in_named_file(tmp_path, monkeypatch):
    other = tmp_path / "docs" / "streams" / "README.md"
    doc = write(tmp_path, monkeypatch, "## 1. Loop\nSee `docs/streams/README.md` §2.\n")
    other.write_text("## 2. Phase 0\n", encoding="utf-8")
    assert links.problems(doc) == []
