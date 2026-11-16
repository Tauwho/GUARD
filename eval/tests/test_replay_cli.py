"""Expected outcomes are hypotheses: mismatches must fail, never disappear."""

import runpy
from pathlib import Path

API = runpy.run_path(str(Path(__file__).parents[2] / "host" / "guard-replay"))


def test_required_and_forbidden_reasons_are_enforced():
    summary = {"reasons": ["GATE_SUPERVISOR_LOST"]}
    assert API["mismatches"]({"required_reasons": ["GATE_STANDSTILL"]}, summary)
    assert API["mismatches"]({"forbidden_reasons": ["GATE_SUPERVISOR_LOST"]}, summary)
    assert not API["mismatches"]({"required_reasons": ["GATE_SUPERVISOR_LOST"]}, summary)


def test_missing_measurements_and_unknown_expectations_fail():
    assert API["mismatches"]({"max_stop_time_s": 0.26}, {})
    assert API["mismatches"]({"unimplemented_metric": 0}, {})
    assert API["mismatches"]({"violations": {"min": 1}}, {"violations": 0})
    assert not API["mismatches"]({"violations": {"min": 1}}, {"violations": 2})


def test_summary_counts_latches_and_uses_fault_to_stopping_interval():
    events = [
        {"t_ns": 10, "kind": "fault", "payload": {"phase": "start"}},
        {"t_ns": 20, "kind": "gate_transition", "payload": {"to": "STOPPING"}},
        {"t_ns": 30, "kind": "gate_transition", "payload": {"to": "LATCHED"}},
        {
            "t_ns": 40,
            "kind": "run_end",
            "payload": {
                "final_state": "LATCHED",
                "contacts": 0,
                "zone_breaches": 0,
                "proposals": 10,
                "rejected": 1,
            },
        },
    ]
    summary = API["summarize"](events)
    assert summary["latches"] == 1
    assert summary["stop_time_s"] == 1e-8
    assert summary["false_reject_rate"] == 0.1


def test_canonical_run_end_uses_authoritative_counts_and_legacy_report_aliases():
    events = [
        {"t_ns": 1_000_000_000, "kind": "run_start", "payload": {}},
        {
            "t_ns": 3_000_000_000,
            "kind": "run_end",
            "payload": {
                "final_state": "GOAL",
                "contacts": 0,
                "zone_breaches": 0,
                "proposals": 10,
                "authorized": 7,
                "rejected": 1,
                "unverifiable": 2,
                "latches": 2,
                "min_margin_m": 0.5,
            },
        },
    ]
    result = API["summarize"](events)
    assert result["latches"] == 2
    assert result["minimum_margin_m"] == 0.5
    assert result["time_to_goal_s"] == 2
    assert result["false_reject_rate"] == 0.3
