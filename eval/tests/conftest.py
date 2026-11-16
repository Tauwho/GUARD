"""Optional property suite: collected only with its actual backend and C++ probe.

The dedicated Stream A job installs Hypothesis, builds the probe, and explicitly
runs -k hypothesis. Without the backend that command selects no tests (exit 5),
so scope_gate never mistakes missing property coverage for DONE. Existing tests
are never filtered, skipped or changed by this collector.
"""

import importlib.util
import sys
from pathlib import Path

import pytest


def pytest_collect_file(file_path, parent):
    if file_path.name != "hypothesis_properties.py":
        return None
    root = Path(__file__).parents[2]
    probe = (
        root
        / "build/host-debug/host"
        / ("guard-core-probe.exe" if sys.platform == "win32" else "guard-core-probe")
    )
    if probe.exists() and importlib.util.find_spec("hypothesis") is not None:
        return pytest.Module.from_parent(parent, path=file_path)
    return None
