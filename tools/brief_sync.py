#!/usr/bin/env python3
"""SCOPE.yaml is the single source of truth for milestone ids, tiers and dates.

Each brief's milestone table lives between `<!-- gen:milestones-<stream> -->` and
`<!-- gen:milestones-<stream>-end -->`. `sync_briefs.py` rewrites the Due and Tier
cells; `check_brief_sync.py` fails on drift. Work and Acceptance stay prose so the
brief keeps its explanation, but the ids, tiers and dates cannot drift (audit 4).
"""

import datetime
from dataclasses import dataclass
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
BRIEFS = {
    "A": ROOT / "docs/streams/BRIEF-A.md",
    "B": ROOT / "docs/streams/BRIEF-B.md",
    "P": ROOT / "docs/streams/BRIEF-P.md",
}
HEADER = ["ID", "Due", "Tier", "Work", "Acceptance"]


class BriefError(Exception):
    pass


@dataclass
class Row:
    index: int
    cells: list

    @property
    def id(self):
        return self.cells[0]

    @property
    def due(self):
        return self.cells[1]

    @property
    def tier(self):
        return self.cells[2]


def load_scope(path=ROOT / "SCOPE.yaml"):
    data = yaml.safe_load(path.read_text(encoding="utf-8"))
    dates = {key: datetime.datetime.fromisoformat(value) for key, value in data["dates"].items()}
    items = {item["id"]: item for item in data["items"]}
    return dates, items


def due_text(value):
    due = value if isinstance(value, datetime.datetime) else datetime.datetime.fromisoformat(value)
    text = f"{due:%a} {due.day} {due:%b}"
    if (due.hour, due.minute) != (23, 59):
        text += due.strftime(" %H:%M")
    return text


def table(text, stream):
    begin = f"<!-- gen:milestones-{stream} -->"
    end = f"<!-- gen:milestones-{stream}-end -->"
    lines = text.splitlines()
    try:
        start, stop = lines.index(begin), lines.index(end)
    except ValueError as error:
        raise BriefError(f"{stream}: missing {begin} / {end} markers") from error
    if stop < start:
        raise BriefError(f"{stream}: end marker before begin marker")
    rows = []
    for index in range(start + 1, stop):
        line = lines[index].strip()
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        if cells[0] == HEADER[0] or all(set(cell) <= set("-: ") for cell in cells):
            continue
        if len(cells) != len(HEADER):
            raise BriefError(f"{stream}: row {index + 1} has {len(cells)} cells, want 5")
        rows.append(Row(index, cells))
    return lines, rows


def problems(stream, dates, items, text):
    lines, rows = table(text, stream)
    expected = {item_id for item_id, item in items.items() if item["owner"] == stream}
    found = [row.id for row in rows]
    errors = []
    for item_id in sorted(expected - set(found)):
        errors.append(f"{stream}: SCOPE item {item_id} is missing from the brief table")
    for item_id in sorted(set(found) - expected):
        errors.append(f"{stream}: brief row {item_id} is not a SCOPE item for this stream")
    for row in rows:
        item = items.get(row.id)
        if not item:
            continue
        want_due = due_text(dates[item["due"]])
        if row.due != want_due:
            errors.append(f"{stream}/{row.id}: due {row.due!r} != SCOPE {want_due!r}")
        if row.tier != item["tier"]:
            errors.append(f"{stream}/{row.id}: tier {row.tier!r} != SCOPE {item['tier']!r}")
    return errors


def rewrite(stream, dates, items, text):
    lines, rows = table(text, stream)
    for row in rows:
        item = items.get(row.id)
        if not item:
            continue
        row.cells[1] = due_text(dates[item["due"]])
        row.cells[2] = item["tier"]
        lines[row.index] = "| " + " | ".join(row.cells) + " |"
    return "\n".join(lines) + "\n"


def check_all():
    dates, items = load_scope()
    errors = []
    seen = {}
    for stream, path in BRIEFS.items():
        try:
            text = path.read_text(encoding="utf-8")
            errors.extend(problems(stream, dates, items, text))
            for row in table(text, stream)[1]:
                if row.id in seen:
                    errors.append(f"{row.id}: appears in both {seen[row.id]} and {stream}")
                seen[row.id] = stream
        except BriefError as error:
            errors.append(str(error))
    for item_id, item in items.items():
        if item["owner"] not in BRIEFS:
            errors.append(f"{item_id}: owner {item['owner']} has no brief")
        elif item_id not in seen:
            errors.append(f"{item_id}: not in any brief table")
    return errors


def sync_all():
    dates, items = load_scope()
    for stream, path in BRIEFS.items():
        text = path.read_text(encoding="utf-8")
        path.write_text(rewrite(stream, dates, items, text), encoding="utf-8")
        print(f"synced {stream}: {path.relative_to(ROOT)}")
    return check_all()
