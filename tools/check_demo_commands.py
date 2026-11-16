#!/usr/bin/env python3
"""Check that every command in demo/DEMO.md exists on main (P2).

For each step of demo/DEMO.md (a `###` heading followed by a `Status:` line):

- `ready` steps: every command in each `sh` block is checked. Lines ending in `\\`
  are joined; `&&`, `||`, `;` and `|` split a line into separately checked segments.
  - `python <script> ...` (also `python3`, `py -3`): the script exists in the repo,
    `<script> --help` exits 0, and every `--flag` the demo uses appears in that help.
  - `python -m <module> ...`: `python -m <module> --help` exits 0.
  - a repo shell script (`x.sh` or `bash x.sh`): the file exists and `bash -n`
    parses it where a real bash is available. The image scripts have no `--help`
    and would boot QEMU, so they are never executed.
  - `ssh [options] <host> <cmd>`: <cmd> must be a target program with sources on
    main (`qnx/<cmd>/`). `ssh -N` (a port forward) needs no program.
  - `cmake --preset <name>`: the preset exists in CMakePresets.json.
  - a short list of standard tools (mkdir, grep, cut, ...) is accepted as external.
  - anything else fails as an unknown command.
- `planned (<SCOPE id>)` steps: commands are listed, not run. The step fails once
  that SCOPE item is DONE, so the script cannot stay behind main.
- Numbered step headings carry a video slot `[m:ss-m:ss]`; slots must be in order,
  not overlap, and end by 3:00. A numbered heading without a slot is an error.
- Fences other than `sh`/`bash` inside a step are an error (they would go unchecked).

Exit 0 when clean; exit 1 with one line per problem. Never uses the network.
This script is P2's scope check; it is deliberately not a `make check` stage, so a
CLI change in A's or B's PR can never turn their CI red because of a P document.
"""

import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "demo" / "DEMO.md"
EXTERNAL = {"mkdir", "grep", "cut", "head", "tail", "ls", "cat", "cd", "source", "sleep"}
HEADING = re.compile(r"^###\s+(.*)$")
NUMBERED_PREFIX = re.compile(r"^\d+\.\s")
NUMBERED = re.compile(r"^(\d+)\.\s+(.*?)\s*\[(\d+):([0-5]\d)-(\d+):([0-5]\d)\]\s*$")
STATUS = re.compile(r"^Status:\s*(ready|planned\s*\((\w+)\))\s*$")
FENCE = re.compile(r"^```(\S*)")
VIDEO_LIMIT_S = 180
SEPARATORS = {"&&", "||", ";", "|", "&"}
SSH_OPTS_WITH_ARG = set("BbcDEeFIiJLlmOoPpQRSWw")
PROGRAM = re.compile(r"^[a-z][a-z0-9_-]*$")


def parse(text):
    """Steps as dicts: title, number, slot, status, scope_id, commands, problems, line."""
    steps = []
    current = None
    fence = None
    pending = ""
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if fence is not None:
            if line.startswith("```"):
                fence = None
                continue
            if fence != "sh" or current is None:
                continue
            stripped = line.strip()
            if not pending and (not stripped or stripped.startswith("#")):
                continue
            if stripped.endswith("\\"):
                pending += stripped[:-1] + " "
                continue
            current["commands"].append((number, pending + stripped))
            pending = ""
            continue
        heading = HEADING.match(line)
        if heading:
            title = heading.group(1)
            current = {"title": title, "line": number, "status": None, "scope_id": None,
                       "number": None, "slot": None, "commands": [], "problems": []}
            numbered = NUMBERED.match(title)
            if numbered:
                n, _, m1, s1, m2, s2 = numbered.groups()
                current["number"] = int(n)
                current["slot"] = (int(m1) * 60 + int(s1), int(m2) * 60 + int(s2))
            elif NUMBERED_PREFIX.match(title):
                current["problems"].append("numbered step has no [m:ss-m:ss] video slot")
            steps.append(current)
            continue
        status = STATUS.match(line)
        if status and current is not None and current["status"] is None:
            current["status"] = "planned" if status.group(2) else "ready"
            current["scope_id"] = status.group(2)
            continue
        opening = FENCE.match(line)
        if opening:
            language = opening.group(1).lower()
            if language in ("sh", "bash"):
                fence = "sh"
            else:
                fence = "other"
                if current is not None and language in ("shell", "console", "zsh",
                                                        "powershell", "ps1", "cmd", ""):
                    current["problems"].append(
                        f"line {number}: ```{language} block is not checked; use ```sh"
                    )
    return [s for s in steps if s["status"] is not None or s["number"] is not None
            or s["problems"]]


def run_help(argv, timeout=120):
    """(ok, output) for a --help style call. Never raises."""
    try:
        result = subprocess.run(argv, cwd=ROOT, capture_output=True, encoding="utf-8",
                                errors="replace", timeout=timeout)
    except Exception as error:  # noqa: BLE001 - reported as a problem line
        return False, f"{type(error).__name__}: {error}"
    return result.returncode == 0, (result.stdout or "") + (result.stderr or "")


def real_bash():
    """A bash that can parse scripts, or None (the System32 WSL launcher may have no distro)."""
    found = shutil.which("bash")
    if found and "system32" in found.lower():
        git_bash = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Git/bin/bash.exe"
        return str(git_bash) if git_bash.exists() else None
    return found


def segments(command):
    """Split one command line into argv lists at shell separators."""
    lexer = shlex.shlex(command, posix=True, punctuation_chars=True)
    lexer.whitespace_split = True
    parts, current = [], []
    for token in lexer:
        if token in SEPARATORS:
            if current:
                parts.append(current)
            current = []
        else:
            current.append(token)
    if current:
        parts.append(current)
    return parts


def check_python(words, runner):
    if len(words) >= 2 and words[0] == "-m":
        ok, out = runner([sys.executable, "-m", words[1], "--help"])
        return None if ok else f"`python -m {words[1]} --help` failed ({tail(out)})"
    if not words:
        return "python without a script"
    script = words[0]
    if not (ROOT / script).is_file():
        return f"missing on main: {script}"
    ok, out = runner([sys.executable, script, "--help"])
    if not ok:
        return f"`{script} --help` failed ({tail(out)})"
    for flag in (w.split("=", 1)[0] for w in words[1:] if w.startswith("--")):
        if flag not in out:
            return f"`{script}` has no {flag} in its --help"
    return None


def check_shell_script(path, runner):
    if not (ROOT / path).is_file():
        return f"missing on main: {path}"
    bash = real_bash()
    if bash:
        ok, out = runner([bash, "-n", path])
        if not ok:
            return f"`bash -n {path}` failed ({tail(out)})"
    return None


def check_ssh(words):
    rest = list(words)
    no_command = False
    while rest and rest[0].startswith("-"):
        option = rest.pop(0)
        letters = option[1:]
        if "N" in letters:
            no_command = True
        last = letters[-1:] if letters else ""
        if last in SSH_OPTS_WITH_ARG and rest:  # e.g. -p 2222, -NL 7700:...
            rest.pop(0)
    if not rest:
        return "ssh without a host"
    if len(rest) == 1:
        return None if no_command else "ssh without a remote command"
    remote = rest[1:]
    if len(remote) == 1:
        remote_parts = segments(remote[0])
    else:
        remote_parts = [remote]
    for part in remote_parts:
        program = part[0]
        if program in EXTERNAL:
            continue
        if not PROGRAM.match(program):
            return f"not a target program name: {program}"
        folder = ROOT / "qnx" / program
        if not (folder.is_dir() and any(p.is_file() for p in folder.rglob("*"))):
            return f"target program not on main: qnx/{program}/"
    return None


def check_cmake(words):
    if "--preset" not in words:
        return None
    index = words.index("--preset")
    if index + 1 >= len(words):
        return "cmake --preset without a name"
    name = words[index + 1]
    presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
    kind = "buildPresets" if "--build" in words else "configurePresets"
    names = {p["name"] for p in presets.get(kind, [])}
    return None if name in names else f"cmake preset not in CMakePresets.json: {name}"


def check_words(words, runner):
    while words and re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", words[0]):
        words = words[1:]
    if not words:
        return None
    head, rest = words[0], words[1:]
    if head in ("python", "python3"):
        return check_python(rest, runner)
    if head == "py":
        return check_python(rest[1:] if rest and rest[0].startswith("-3") else rest, runner)
    if head == "bash" and rest and rest[0].endswith(".sh"):
        return check_shell_script(rest[0], runner)
    if head.endswith(".sh"):
        return check_shell_script(head, runner)
    if head == "ssh":
        return check_ssh(rest)
    if head == "cmake":
        return check_cmake(rest)
    if head in EXTERNAL:
        return None
    return f"unknown command: {head}"


def check_command(command, runner=run_help):
    """None if every segment of the command exists on main, else a problem string."""
    try:
        parts = segments(command)
    except ValueError as error:
        return f"cannot parse: {error}"
    for words in parts:
        problem = check_words(words, runner)
        if problem:
            return problem
    return None


def tail(output):
    lines = [line for line in output.strip().splitlines() if line.strip()]
    return lines[-1] if lines else "no output"


def scope_states(ids):
    """SCOPE id -> state, with tools/scope_gate.py's own rules (cmd checks not run)."""
    if not ids:
        return {}
    sys.path.insert(0, str(ROOT / "tools"))
    import datetime

    import scope_gate
    import yaml

    os.environ["GUARD_SCOPE_GATE_ACTIVE"] = "1"  # nested cmd checks report unknown
    scope = yaml.safe_load((ROOT / "SCOPE.yaml").read_text(encoding="utf-8"))
    dates = {
        k: datetime.datetime.fromisoformat(v) if isinstance(v, str) else v
        for k, v in scope["dates"].items()
    }
    now = datetime.datetime.now(scope_gate.SGT).replace(tzinfo=None)
    return {
        item["id"]: scope_gate.item_state(item, now, dates)[0]
        for item in scope["items"]
        if item["id"] in ids
    }


def problems(text, runner=run_help, states=None, check=None):
    check = check or (lambda command: check_command(command, runner))
    out = []
    steps = parse(text)
    if not any(s["number"] for s in steps):
        out.append("DEMO.md: no numbered steps")
    planned_ids = {s["scope_id"] for s in steps if s["status"] == "planned"}
    if states is None:
        states = scope_states(planned_ids)
    previous_end = 0
    expected_number = 1
    for step in steps:
        where = f"DEMO.md:{step['line']}"
        out += [f"{where}: {problem}" for problem in step["problems"]]
        if step["status"] is None:
            out.append(f"{where}: step has no Status line")
            continue
        if step["number"] is not None:
            if step["number"] != expected_number:
                out.append(
                    f"{where}: step {step['number']} out of order (expected {expected_number})"
                )
            expected_number = step["number"] + 1
            start, end = step["slot"]
            if start < previous_end or end <= start:
                out.append(f"{where}: video slot overlaps or is empty")
            if end > VIDEO_LIMIT_S:
                out.append(f"{where}: video slot ends after 3:00")
            previous_end = end
        if not step["commands"]:
            out.append(f"{where}: step has no commands")
        if step["status"] == "planned":
            state = states.get(step["scope_id"])
            if state is None:
                out.append(f"{where}: planned ({step['scope_id']}) is not a SCOPE id")
            elif state == "DONE":
                out.append(f"{where}: {step['scope_id']} is DONE; mark the step ready")
            continue
        for line, command in step["commands"]:
            problem = check(command)
            if problem:
                out.append(f"DEMO.md:{line}: {problem}")
    return out


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--file", type=Path, default=DEMO)
    args = parser.parse_args()
    if not args.file.is_file():
        print(f"check_demo_commands: {args.file} missing")
        return 1
    found = problems(args.file.read_text(encoding="utf-8"))
    for problem in found:
        print(problem)
    if found:
        print(f"check_demo_commands: FAIL ({len(found)} problem(s))")
        return 1
    print("check_demo_commands: PASS (every ready command exists on main)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
