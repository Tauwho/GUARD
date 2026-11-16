import json
import runpy
from pathlib import Path

import jsonschema
import pytest


def test_duplicate_body_reuses_measurement_but_changed_invalid_data_fails(monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[1]))
    api = runpy.run_path(str(Path(__file__).parents[1] / "run_matrix.py"))
    header = {
        "v": 1,
        "t_ns": 0,
        "src": "harness",
        "seq": 0,
        "kind": "run_start",
        "payload": {"scenario": "F0", "mode": "B2", "seed": 1},
    }
    end = {
        "v": 1,
        "t_ns": 10,
        "src": "harness",
        "seq": 1,
        "kind": "run_end",
        "payload": {
            "contacts": 0,
            "zone_breaches": 0,
            "proposals": 1,
            "rejected": 0,
            "authorized": 1,
            "unverifiable": 0,
            "latches": 0,
            "final_state": "GOAL",
        },
    }
    lines = [json.dumps(header), json.dumps(end)]
    first, digest = api["validated_measurement"](lines)
    header["payload"]["seed"] = 2
    second, repeated = api["validated_measurement"]([json.dumps(header), lines[1]])
    assert first is second
    assert digest == repeated
    end["v"] = 99
    with pytest.raises(jsonschema.ValidationError):
        api["validated_measurement"]([json.dumps(header), json.dumps(end)])
    header["v"] = 99
    with pytest.raises(jsonschema.ValidationError):
        api["validated_measurement"]([json.dumps(header), lines[1]])


def test_b2_gate_preserves_control_differences_but_fails_regressions(monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[1]))
    api = runpy.run_path(str(Path(__file__).parents[1] / "run_matrix.py"))
    records = [
        {"mode": "B2", "mismatches": []},
        {"mode": "B0", "mismatches": ["hypothesis differs"]},
        {"mode": "B2-stopping", "mismatches": ["contact"]},
    ]
    assert api["failed_gate"](records)
    assert not api["failed_gate"](records, True)
    records[0]["mismatches"] = ["unexpected contact"]
    assert api["failed_gate"](records, True)
    records[0]["mismatches"] = []
    records[1]["error"] = "bad schema"
    assert api["failed_gate"](records, True)
    assert api["failed_gate"]([], True)
