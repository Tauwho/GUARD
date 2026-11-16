"""build_readme.py: generated blocks come from main, --check catches drift (P1)."""

import importlib.util
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("build_readme", TOOLS / "build_readme.py")
readme = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(readme)

SKELETON = f"""# Title

Prose that must survive.

{readme.markers("quickstart")[0]}
old quick start
{readme.markers("quickstart")[1]}

{readme.markers("results")[0]}
old results
{readme.markers("results")[1]}

{readme.markers("evidence-map")[0]}
old evidence
{readme.markers("evidence-map")[1]}
"""


def test_generated_blocks_replace_placeholders_and_keep_prose():
    out = readme.render(SKELETON)
    assert "Prose that must survive." in out
    assert "old quick start" not in out and "old results" not in out
    assert "## Evidence Map" in out
    assert "core-on-target" in out


def test_missing_markers_are_an_error():
    try:
        readme.render("# no markers here\n")
    except SystemExit as error:
        assert "markers not found" in str(error)
    else:
        raise AssertionError("render accepted a README without markers")


def test_missing_results_snapshot_renders_planned(monkeypatch, tmp_path):
    monkeypatch.setattr(readme, "RESULTS", tmp_path / "absent.json")
    assert "planned (A4)" in readme.results()


def test_missing_results_snapshot_never_invents_numbers(monkeypatch, tmp_path):
    monkeypatch.setattr(readme, "RESULTS", tmp_path / "absent.json")
    assert "Source: host" not in readme.results()


def test_evidence_map_marks_target_bundles_as_behaviour():
    block = readme.evidence_map()
    assert "QNX 8.0 target (behaviour only)" in block
    assert "measurements" in block


def test_quickstart_commands_come_from_component_readmes():
    block = readme.quickstart()
    host = readme.first_shell_block(readme.ROOT / "host/README.md")
    assert host in block
    assert "image/run-demo.sh" in block


def test_checked_in_readme_is_current():
    current = readme.README.read_text(encoding="utf-8")
    assert readme.render(current) == current
