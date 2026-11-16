"""Tests for tools/check_architecture_svg.py (P5). Fixture text only: no repo files."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_architecture_svg as arch  # noqa: E402

DOC = """# ref

## REF-CONTRACTS

| ID | Contract |
|---|---|
| C1 | Proposal |
| C2 | Lease |

## REF-CHECKERS

| # | Checker |
|---|---|
| 1 | Freshness |
| 2 | Map |

## REF-PRIORITIES

| Thread | Prio | Mechanism |
|---|---|---|
| gate tick | 35 | pulse |
| proposal server | Inherits the planner (10) | name_attach |
"""


def svg(*texts):
    return "<svg>" + "".join(f'<text x="1">{t}</text>' for t in texts) + "</svg>"


def test_matching_labels_pass():
    found = arch.problems(svg("C1 proposal", "prio 35", "inherits 10", "checks 1-2"), DOC)
    assert found == []


def test_unknown_contract_id_fails():
    assert arch.problems(svg("C9 remote"), DOC) == [
        "architecture.svg: 'C9 remote': C9 is not in REF-CONTRACTS"
    ]


def test_contract_range_is_expanded():
    found = arch.problems(svg("C1-C3: contracts"), DOC)
    assert found == ["architecture.svg: 'C1-C3: contracts': C3 is not in REF-CONTRACTS"]


def test_wrong_priority_fails():
    assert arch.problems(svg("prio 30"), DOC) == [
        "architecture.svg: 'prio 30': priority 30 not in REF-PRIORITIES"
    ]


def test_checker_range_beyond_reference_fails():
    assert arch.problems(svg("checks 1-11"), DOC) == [
        "architecture.svg: 'checks 1-11': checks 1-11 outside 1-2"
    ]


def test_missing_contracts_table_is_reported():
    assert arch.problems(svg("plain"), "# nothing\n") == [
        "architecture.md: REF-CONTRACTS table not found"
    ]
