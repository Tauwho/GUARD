import runpy
from pathlib import Path

API = runpy.run_path(str(Path(__file__).parents[1] / "metrics.py"))


def test_braking_duration_is_separate_from_fault_to_stopping():
    events = [
        {"kind": "fault", "t_ns": 1_000_000_000, "payload": {"phase": "start"}},
        {"kind": "gate_transition", "t_ns": 1_200_000_000, "payload": {"to": "STOPPING"}},
        {"kind": "gate_transition", "t_ns": 2_000_000_000, "payload": {"to": "STOPPED"}},
        {
            "kind": "run_end",
            "t_ns": 3_000_000_000,
            "payload": {
                "final_state": "READY",
                "contacts": 2,
                "zone_breaches": 1,
                "proposals": 10,
                "rejected": 2,
                "minimum_margin_m": -0.1,
                "time_to_goal_s": None,
            },
        },
    ]
    result = API["measure"](events)
    assert result["stop_time_s"] == 0.2
    assert result["braking_durations_s"] == [0.8]
    assert result["violations"] == 3
    assert result["minimum_margin_m"] == -0.1
    assert result["time_to_goal_s"] is None
    assert result["recovery_correctness"] is None
