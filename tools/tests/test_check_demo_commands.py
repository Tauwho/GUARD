"""Tests for tools/check_demo_commands.py (P2).

No test here runs a real CLI: the demo command check is P2's scope check, not a
`make check` stage, so A's or B's CLI changes can never fail their CI through it.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_demo_commands as cdc  # noqa: E402

HELP = "usage: x [--scenario S] [--seed N] [--mode M] [--out O] [--trace T] [--map M]"


def ok_runner(argv, timeout=120):
    return True, HELP


def failing_runner(argv, timeout=120):
    return False, "boom"


def demo(*steps):
    return "# Demo\n\n" + "\n".join(steps)


def step(title, status, *commands):
    body = "\n".join(commands)
    return f"### {title}\nStatus: {status}\n\n```sh\n{body}\n```\n"


def one(*commands, runner=ok_runner):
    return cdc.problems(demo(step("1. A [0:00-0:10]", "ready", *commands)), runner, states={})


def test_repo_demo_is_well_formed_without_running_clis():
    text = cdc.DEMO.read_text(encoding="utf-8")
    planned = {s["scope_id"] for s in cdc.parse(text) if s["status"] == "planned"}
    found = cdc.problems(text, states=dict.fromkeys(planned, "OPEN"), check=lambda c: None)
    assert found == []
    assert planned <= {"B4", "B5", "B6"}


def test_ready_python_script_must_exist():
    assert one("python host/does-not-exist --x") == [
        "DEMO.md:7: missing on main: host/does-not-exist"
    ]


def test_ready_python_help_must_pass():
    found = one("python host/guard-replay --scenario x", runner=failing_runner)
    assert len(found) == 1 and "--help` failed (boom)" in found[0]


def test_flag_missing_from_help_fails():
    found = one("python host/guard-replay --scenario x --speed 3")
    assert found == ["DEMO.md:7: `host/guard-replay` has no --speed in its --help"]


def test_unknown_command_is_named():
    assert one("guardctl status") == ["DEMO.md:7: unknown command: guardctl"]


def test_compound_commands_check_every_segment():
    assert one("mkdir -p out && python host/missing --x") == [
        "DEMO.md:7: missing on main: host/missing"
    ]
    assert one("mkdir -p out; frobnicate") == ["DEMO.md:7: unknown command: frobnicate"]


def test_line_continuation_is_one_command():
    assert one("python host/guard-replay \\", "  --scenario x --seed 1") == []


def test_py_launcher_and_bash_script_forms():
    assert one("py -3 host/guard-replay --scenario x") == []
    assert one("bash image/does-not-exist.sh") == [
        "DEMO.md:7: missing on main: image/does-not-exist.sh"
    ]


def test_ssh_remote_program_needs_target_sources():
    found = one("ssh -i key -p 2222 root@127.0.0.1 guardctl status")
    assert found == ["DEMO.md:7: target program not on main: qnx/guardctl/"]


def test_ssh_port_forward_and_quoted_remote_pipeline():
    assert one("ssh -N -L 7700:127.0.0.1:7700 -p 2222 root@127.0.0.1") == []
    found = one("ssh -L 1:h:1 -p 2222 root@h 'guardctl status | grep -E \"^mode=\"'")
    assert found == ["DEMO.md:7: target program not on main: qnx/guardctl/"]
    assert one("ssh -p 2222 root@h ..") == ["DEMO.md:7: not a target program name: .."]


def test_cmake_preset_must_exist():
    assert one("cmake --preset host-debug") == []
    assert one("cmake --preset nope") == [
        "DEMO.md:7: cmake preset not in CMakePresets.json: nope"
    ]


def test_planned_step_is_not_run_until_done():
    text = demo(step("1. Target [0:00-0:10]", "planned (B5)", "guardctl inject F3"))
    assert cdc.problems(text, failing_runner, states={"B5": "OPEN"}) == []
    found = cdc.problems(text, failing_runner, states={"B5": "DONE"})
    assert found == ["DEMO.md:3: B5 is DONE; mark the step ready"]


def test_planned_step_needs_a_real_scope_id():
    text = demo(step("1. Target [0:00-0:10]", "planned (Z9)", "guardctl inject F3"))
    assert cdc.problems(text, ok_runner, states={}) == [
        "DEMO.md:3: planned (Z9) is not a SCOPE id"
    ]


def test_video_slots_in_order_and_under_three_minutes():
    text = demo(
        step("1. A [0:00-1:00]", "ready", "mkdir -p out"),
        step("2. B [0:50-1:30]", "ready", "mkdir -p out"),
        step("3. C [1:30-3:10]", "ready", "mkdir -p out"),
    )
    found = cdc.problems(text, ok_runner, states={})
    assert any("overlaps" in p for p in found)
    assert any("after 3:00" in p for p in found)


def test_slot_ending_exactly_at_three_minutes_is_allowed():
    text = demo(step("1. A [2:00-3:00]", "ready", "mkdir -p out"))
    assert cdc.problems(text, ok_runner, states={}) == []


def test_seconds_above_59_are_not_a_slot():
    text = demo(step("1. A [0:75-1:30]", "ready", "mkdir -p out"))
    found = cdc.problems(text, ok_runner, states={})
    assert "DEMO.md:3: numbered step has no [m:ss-m:ss] video slot" in found


def test_numbered_heading_without_slot_fails():
    text = demo(step("1. A [0:00-0:10]", "ready", "mkdir -p out"),
                step("2. Wrap-up", "ready", "mkdir -p out"))
    found = cdc.problems(text, ok_runner, states={})
    assert any("no [m:ss-m:ss] video slot" in p for p in found)


def test_unchecked_fence_language_fails():
    text = "### 1. A [0:00-0:10]\nStatus: ready\n\n```console\nfrobnicate\n```\n"
    found = cdc.problems(text, ok_runner, states={})
    assert "DEMO.md:1: line 4: ```console block is not checked; use ```sh" in found


def test_missing_status_line_fails():
    text = "### 1. A [0:00-0:10]\n\n```sh\nmkdir -p out\n```\n"
    assert cdc.problems(text, ok_runner, states={}) == ["DEMO.md:1: step has no Status line"]


def test_comments_and_env_prefixes_are_ignored():
    assert one("# a comment", "FOO=1 mkdir -p out", runner=failing_runner) == []


def test_runner_survives_undecodable_output():
    code = "import sys; sys.stdout.buffer.write(bytes([255]))"
    ok, out = cdc.run_help([sys.executable, "-c", code])
    assert ok and out
