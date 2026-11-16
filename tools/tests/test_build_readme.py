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

{readme.markers("limits")[0]}
old limits
{readme.markers("limits")[1]}
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


def test_limits_block_mirrors_limits_md_with_claims_marker():
    out = readme.render(SKELETON)
    assert "old limits" not in out
    block = out.split(readme.markers("limits")[0])[1].split(readme.markers("limits")[1])[0]
    lines = block.strip().splitlines()
    assert lines[0] == "<!-- claims-ok: negated -->"
    assert lines[1:] == readme.limits_bullets(readme.LIMITS.read_text(encoding="utf-8"))


def test_missing_limits_source_renders_planned(tmp_path, monkeypatch):
    monkeypatch.setattr(readme, "LIMITS", tmp_path / "absent.md")
    assert readme.limits() == readme.planned("limits")


def _raises(text):
    try:
        readme.limits_bullets(text)
    except SystemExit as error:
        return str(error)
    return None


def test_limits_missing_heading_is_an_error():
    assert "no '## Limits' section" in _raises("# limits\n\n- No x.\n")


def test_limits_empty_list_is_an_error():
    assert "empty" in _raises("## Limits\n\n")


def test_limits_wrapped_bullet_is_folded():
    text = "## Limits\n- No real actuators,\n  braking or vehicles.\n- No formal proof.\n"
    assert readme.limits_bullets(text) == [
        "- No real actuators, braking or vehicles.",
        "- No formal proof.",
    ]


def test_limits_unnegated_bullet_is_rejected():
    assert "not a negation" in _raises("## Limits\n- This system guarantees safety.\n")


def test_limits_stray_prose_is_rejected():
    assert "not a '- ' bullet" in _raises("## Limits\n- No x.\nsurprise prose\n")
