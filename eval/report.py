"""Write a host functional report; target numbers are deliberately not imported (D11)."""

import argparse
import json
from collections import defaultdict
from pathlib import Path

from chart import heatmap

MODES = ("B0", "B1", "B2", "B2-freshness", "B2-stopping", "B2-expiry", "B2-epoch")


def render(data, out):
    if data.get("source") != "host":
        raise ValueError(
            "report accepts host results only; target verification is described in words"
        )
    out.mkdir(parents=True, exist_ok=True)
    groups = defaultdict(list)
    for run in data["runs"]:
        groups[(run["scenario"], run["mode"])].append(run)
    cells = {}
    rows = sorted({key[0] for key in groups})
    lines = [
        "# GUARD functional evaluation",
        "",
        f"Source: host ({data['platform']}) harness. Commit: `{data.get('commit', 'unknown')}`.",
        "",
        "Simulation only. No QNX measurements are included.",
        "",
        "## Limits",
        "",
    ]
    lines.extend("- " + limit for limit in data.get("limitations", []))
    if data["platform"] != "Linux":
        lines += [
            "",
            "Preliminary internal host run. Rerun on Linux before public numeric publication "
            "under D11.",
        ]
    lines += [
        "",
        "## Matrix",
        "",
        "![Reference mismatches](matrix.png)",
        "",
        "Columns: B0, B1, B2, -F freshness, -S stopping, -X expiry, -E epoch.",
        "Green: reference matches. Orange: ablation differs from B2 reference.",
        "Red: baseline/B2 mismatch. Grey: missing or failed execution.",
        "",
        "| Scenario | Mode | Runs | Errors | Reference mismatches | Contact episodes | "
        "Zone episodes | Min margin (m) | Final states |",
        "|---|---|---:|---:|---:|---:|---:|---:|---|",
    ]
    for row, scenario in enumerate(rows):
        for column, mode in enumerate(MODES):
            runs = groups.get((scenario, mode), [])
            errors = sum("error" in run for run in runs)
            mismatches = sum(bool(run["mismatches"]) for run in runs)
            good = [run["summary"] for run in runs if "summary" in run]
            cells[(row, column)] = (
                3 if not runs or errors else (1 if "-" in mode else 2) if mismatches else 0
            )
            contacts = sum(item["contacts"] for item in good)
            zones = sum(item["zone_breaches"] for item in good)
            margin = min(
                (
                    item["minimum_margin_m"]
                    for item in good
                    if item.get("minimum_margin_m") is not None
                ),
                default=None,
            )
            margin = "unavailable" if margin is None else f"{margin:.3f}"
            states = ", ".join(sorted({item["final_state"] for item in good})) or "unavailable"
            lines.append(
                f"| {scenario} | {mode} | {len(runs)} | {errors} | {mismatches} | "
                f"{contacts} | {zones} | {margin} | {states} |"
            )
    lines += [
        "",
        "## B2 functional measurements",
        "",
        "Times are simulated seconds. Ranges cover the requested repeats.",
        "",
        "| Scenario | Fault to STOPPING | STOPPING to STOPPED | Time to goal | F0 rejection rate |",
        "|---|---:|---:|---:|---:|",
    ]

    def extent(values):
        values = [value for value in values if value is not None]
        return f"{min(values):.4f}..{max(values):.4f}" if values else "unavailable"

    for scenario in rows:
        measured = [run["summary"] for run in groups.get((scenario, "B2"), []) if "summary" in run]
        stop = extent([item.get("stop_time_s") for item in measured])
        brake = extent([value for item in measured for value in item["braking_durations_s"]])
        goal = extent([item.get("time_to_goal_s") for item in measured])
        rate = (
            extent([item["false_reject_rate"] for item in measured]) if scenario == "F0" else "n/a"
        )
        lines.append(f"| {scenario} | {stop} | {brake} | {goal} | {rate} |")
    lines += ["", "## Mismatches", ""]
    unique = sorted(
        {
            (run["scenario"], run["mode"], message)
            for run in data["runs"]
            for message in run["mismatches"]
        }
    )
    lines.extend(f"- {scenario} / {mode}: {message}" for scenario, mode, message in unique)
    if not unique:
        lines.append("None in the requested cases.")
    lines += [
        "",
        "## Target verification",
        "",
        "No target numeric table is imported. Stream B target verification remains "
        "a separate handoff.",
        "",
        "## Interpretation",
        "",
        "Repeated seeds currently have identical physical inputs. Counts across seeds "
        "are repeated executions, not independent trials.",
        "Removing a check can change its emitted reasons without producing a contact. "
        "This alone does not demonstrate an effect on violations.",
        "Missing recovery measurements remain unavailable. F0 rejection rate uses "
        "the nominal-planner operational definition in host/README.md.",
        "",
    ]
    heatmap(out / "matrix.png", rows, ("B0", "B1", "B2", "-F", "-S", "-X", "-E"), cells)
    (out / "report.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", type=Path)
    parser.add_argument("--out", type=Path, default=Path("out/eval-report"))
    args = parser.parse_args()
    render(json.loads(args.results.read_text(encoding="utf-8")), args.out)


if __name__ == "__main__":
    main()
