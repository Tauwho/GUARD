#!/usr/bin/env python3
"""Regenerate the README's Evidence Map, Results, Quick start and Limits blocks.

Sources, all on main:
  evidence/*/manifest.json          Evidence Map (target and host bundles)
  docs/results/host-linux.json      Results, from the newest Linux host-evaluation run
  host/, dashboard/, image/ READMEs Quick start commands
  docs/limits.md                    Limits (negated bullets only)

Usage:
  python tools/build_readme.py            rewrite the README from the sources
  python tools/build_readme.py --check    exit 1 if any generated block is stale
  python tools/build_readme.py --fetch    update docs/results/host-linux.json from the
                                          newest successful host-evaluation run on main

`--check` never uses the network, so `make check` stays deterministic. A missing
input renders `planned (<SCOPE id>)`, never a blank section or an invented number.
"""

import argparse
import glob
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
RESULTS = ROOT / "docs/results/host-linux.json"
BLOCKS = ("evidence-map", "results", "quickstart", "limits")
PLANNED = {"results": "A4", "evidence-map": "B1", "quickstart": "A3", "limits": "P5"}
LIMITS = ROOT / "docs/limits.md"


def markers(name):
    return f"<!-- gen:{name} -->", f"<!-- gen:{name}-end -->"


def replace_block(text, name, body):
    start, end = markers(name)
    pattern = re.compile(re.escape(start) + r".*?" + re.escape(end), re.S)
    if not pattern.search(text):
        raise SystemExit(f"build_readme: {name}: markers not found in README.md")
    return pattern.sub(f"{start}\n{body.strip()}\n{end}", text, count=1)


def planned(name):
    return f"_planned ({PLANNED[name]})_ - generated when the source lands on main."


def evidence_map():
    manifests = sorted(
        glob.glob(str(ROOT / "evidence/*/manifest.json")),
        key=lambda path: json.loads(Path(path).read_text(encoding="utf-8")).get("date", ""),
        reverse=True,
    )
    if not manifests:
        return f"## Evidence Map\n\n{planned('evidence-map')}"
    rows = [
        "## Evidence Map",
        "",
        "| Evidence | Where it ran | Commit | Contracts | Bundle |",
        "|---|---|---|---|---|",
    ]
    for path in manifests:
        manifest = json.loads(Path(path).read_text(encoding="utf-8"))
        bundle = Path(path).parent.name
        target = bool(manifest.get("sdp_version"))
        where = "QNX 8.0 target (behaviour only)" if target else "host (Linux) harness"
        contracts = manifest.get("contracts", "-")
        sha = manifest.get("sha", "")[:7] or "-"
        rows.append(f"| {manifest.get('topic', bundle)} | {where} | `{sha}` | {contracts} | "
                    f"[{bundle}](evidence/{bundle}/manifest.json) |")
    rows += [
        "",
        "Target bundles are transcripts, reason codes and test counts: behaviour, not",
        "measurements. Numbers appear only in the host (Linux) Results section above.",
    ]
    return "\n".join(rows)


def results():
    if not RESULTS.exists():
        return f"## Results\n\n{planned('results')}"
    data = json.loads(RESULTS.read_text(encoding="utf-8"))
    rows = [
        "## Results",
        "",
        f"Source: host (Linux) harness, commit `{data['commit'][:7]}`, "
        f"{data['repeats']} repeat(s) per scenario/config, "
        f"[run {data['run_id']}]({data['run_url']}). Simulation only; no QNX numbers.",
        "",
        "| Scenario | Runs | B2 gate mismatches | Baseline (B0/B1) mismatches | "
        "Ablation differences |",
        "|---|---:|---:|---:|---:|",
    ]
    for scenario in sorted(data["scenarios"], key=lambda key: (len(key), key)):
        row = data["scenarios"][scenario]
        rows.append(
            f"| {scenario} | {row['runs']} | {row['b2_mismatches']} | "
            f"{row['baseline_mismatches']} | {row['ablation_differences']} |"
        )
    rows += [
        "",
        "B2 mismatches are failures (CI gates them). Baseline mismatches are expected: "
        "B0/B1 remove the checks a scenario is meant to catch. Ablation differences are "
        "the hypothesis under test, reported even when the hypothesis is rejected.",
    ]
    return "\n".join(rows)


def first_shell_block(path):
    text = path.read_text(encoding="utf-8")
    match = re.search(r"```sh\n(.*?)```", text, re.S)
    return match.group(1).strip() if match else None


def quickstart():
    sources = [
        ("Host replay (Linux or WSL2)", ROOT / "host/README.md"),
        ("Dashboard (Linux or WSL2)", ROOT / "dashboard/README.md"),
        ("On target (QNX SDP required, B's and P's machines only)", ROOT / "image/README.md"),
    ]
    parts = ["## Quick start"]
    for title, path in sources:
        block = first_shell_block(path) if path.exists() else None
        if block is None:
            parts += [f"### {title}", "", planned("quickstart")]
            continue
        parts += [f"### {title}", "", "```sh", block, "```", ""]
    return "\n".join(parts).strip()


NEGATED_BULLET = re.compile(r"^- (No|Not|Never)\b")


def limits_bullets(text):
    """Bullets under `## Limits`, continuation lines folded in. Raises on a malformed list.

    Every bullet must be a negation (`- No`, `- Not`, `- Never`): the generated block
    carries a claims-ok marker, so an un-negated claim must not slip in under it.
    """
    pattern = r"^## Limits[ \t]*\n(.*?)(?=^## |\Z)"
    match = re.search(pattern, text.lstrip("﻿"), re.S | re.M)
    if not match:
        raise SystemExit("build_readme: docs/limits.md has no '## Limits' section")
    bullets = []
    for line in match.group(1).splitlines():
        if not line.strip() or line.strip() == "<!-- claims-ok: negated -->":
            continue
        if line.startswith("- "):
            bullets.append(line.rstrip())
        elif line.startswith((" ", "\t")) and bullets:
            bullets[-1] += " " + line.strip()
        else:
            raise SystemExit(f"build_readme: docs/limits.md: not a '- ' bullet: {line!r}")
    if not bullets:
        raise SystemExit("build_readme: docs/limits.md: the Limits list is empty")
    for bullet in bullets:
        if not NEGATED_BULLET.match(bullet):
            raise SystemExit(f"build_readme: docs/limits.md: bullet is not a negation: {bullet!r}")
    return bullets


def limits():
    """The Limits list from docs/limits.md, with its claims-ok marker."""
    if not LIMITS.exists():
        return planned("limits")
    bullets = limits_bullets(LIMITS.read_text(encoding="utf-8"))
    return "\n".join(["<!-- claims-ok: negated -->", *bullets])


RENDERERS = {
    "evidence-map": evidence_map,
    "results": results,
    "quickstart": quickstart,
    "limits": limits,
}


def render(text):
    for name, renderer in RENDERERS.items():
        text = replace_block(text, name, renderer())
    return text


def fetch_results():
    def run(*command):
        done = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
        if done.returncode:
            raise SystemExit(f"build_readme: {' '.join(command)} failed\n{done.stderr}")
        return done.stdout

    listed = json.loads(
        run(
            "gh", "run", "list", "--workflow", "host-evaluation.yml", "--branch", "main",
            "--status", "success", "--limit", "1", "--json", "databaseId,headSha,url",
        )
    )
    if not listed:
        raise SystemExit("build_readme: no successful host-evaluation run on main")
    run_id, sha, url = listed[0]["databaseId"], listed[0]["headSha"], listed[0]["url"]
    with tempfile.TemporaryDirectory(prefix="host-eval-") as cache:
        run("gh", "run", "download", str(run_id), "-D", cache)
        files = list(Path(cache).rglob("results.json"))
        if not files:
            raise SystemExit("build_readme: artifact has no results.json")
        matrix = json.loads(files[0].read_text(encoding="utf-8"))
    scenarios = {}
    pairs = {}
    for entry in matrix["runs"]:
        row = scenarios.setdefault(
            entry["scenario"],
            {"runs": 0, "b2_mismatches": 0, "baseline_mismatches": 0, "ablation_differences": 0},
        )
        row["runs"] += 1
        key = (entry["scenario"], entry["mode"])
        pairs[key] = pairs.get(key, 0) + 1
        mismatched = bool(entry["mismatches"])
        if entry["mode"].startswith("B2-"):
            row["ablation_differences"] += int(mismatched)
        elif entry["mode"] == "B2":
            row["b2_mismatches"] += int(mismatched)
        else:
            row["baseline_mismatches"] += int(mismatched)
    snapshot = {
        "run_id": run_id,
        "run_url": url,
        "commit": matrix.get("commit", sha),
        "repeats": max(pairs.values(), default=0),
        "scenarios": scenarios,
    }
    RESULTS.parent.mkdir(parents=True, exist_ok=True)
    RESULTS.write_text(json.dumps(snapshot, indent=2) + "\n", encoding="utf-8")
    print(f"build_readme: results <- run {run_id} ({RESULTS.relative_to(ROOT)})")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="exit 1 if the README is stale")
    parser.add_argument("--fetch", action="store_true", help="refresh the host results snapshot")
    args = parser.parse_args()
    if args.fetch:
        fetch_results()
    current = README.read_text(encoding="utf-8")
    expected = render(current)
    if args.check:
        stale = [name for name in BLOCKS
                 if render_blocks(current)[name] != render_blocks(expected)[name]]
        if expected != current:
            print("build_readme: FAIL stale blocks: " + ", ".join(stale or list(BLOCKS)))
            print("  run: python tools/build_readme.py")
            return 1
        print("build_readme: PASS (README blocks match their sources on main)")
        return 0
    README.write_text(expected, encoding="utf-8", newline="\n")
    print("build_readme: README blocks regenerated")
    return 0


def render_blocks(text):
    found = {}
    for name in BLOCKS:
        start, end = markers(name)
        match = re.search(re.escape(start) + r"(.*?)" + re.escape(end), text, re.S)
        found[name] = match.group(1).strip() if match else ""
    return found


if __name__ == "__main__":
    sys.exit(main())
