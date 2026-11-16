import json
import runpy
import struct
import zlib
from pathlib import Path

import pytest


def test_report_keeps_failures_and_emits_valid_png(tmp_path, monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[1]))
    api = runpy.run_path(str(Path(__file__).parents[1] / "report.py"))
    data = {
        "source": "host",
        "platform": "Windows",
        "seed_variation": False,
        "runs": [
            {
                "scenario": "F5",
                "mode": "B2",
                "seed": 1,
                "mismatches": ["missing stopping reason"],
                "summary": {
                    "contacts": 0,
                    "zone_breaches": 0,
                    "minimum_margin_m": None,
                    "final_state": "READY",
                    "braking_durations_s": [],
                    "time_to_goal_s": None,
                },
            }
        ],
    }
    api["render"](data, tmp_path)
    report = (tmp_path / "report.md").read_text(encoding="utf-8")
    assert "missing stopping reason" in report
    assert "Preliminary internal host run" in report
    assert "unavailable" in report
    png = (tmp_path / "matrix.png").read_bytes()
    assert png[:8] == b"\x89PNG\r\n\x1a\n"
    offset = 8
    while offset < len(png):
        length = struct.unpack(">I", png[offset : offset + 4])[0]
        content = png[offset + 4 : offset + 8 + length]
        checksum = struct.unpack(">I", png[offset + 8 + length : offset + 12 + length])[0]
        assert zlib.crc32(content) == checksum
        offset += length + 12
    data["source"] = "target"
    with pytest.raises(ValueError, match="host results only"):
        api["render"](data, tmp_path)
    assert "latency" not in json.dumps(data)
