"""Generated properties exercise production C++ code, not a Python reimplementation."""

import json
import struct
import subprocess
import sys
from pathlib import Path

from hypothesis import example, given, settings
from hypothesis import strategies as st

ROOT = Path(__file__).parents[2]
PROBE = (
    ROOT
    / "build/host-debug/host"
    / ("guard-core-probe.exe" if sys.platform == "win32" else "guard-core-probe")
)
PROPERTY = settings(max_examples=100, derandomize=True, database=None, deadline=None)


def call(*arguments):
    result = subprocess.run(
        [str(PROBE), *map(str, arguments)], capture_output=True, text=True, timeout=5
    )
    assert result.returncode == 0, result.stderr
    return json.loads(result.stdout)


@st.composite
def lease_wires(draw):
    ident = draw(st.integers(1, 2**64 - 1))
    epoch = draw(st.integers(1, 2**32 - 1))
    issued = draw(st.integers(0, 2**63 - 1 - 1_000_000_000))
    duration = draw(st.integers(50, 1000)) * 1_000_000
    speed = draw(st.floats(0, 1.2, allow_nan=False, allow_infinity=False))
    turn = draw(st.floats(-1, 1, allow_nan=False, allow_infinity=False))
    return struct.pack(
        "<HHHHQIIQQdddqq",
        0x4447,
        0x11,
        1,
        80,
        ident,
        epoch,
        0,
        ident,
        ident,
        speed,
        turn,
        1.2,
        issued,
        issued + duration,
    )


@PROPERTY
@given(lease_wires())
def test_hypothesis_lease_roundtrip_preserves_every_contract_byte(wire):
    result = call("lease", wire.hex())
    assert result["valid"]
    assert bytes.fromhex(result["wire"]) == wire


@PROPERTY
@given(
    lease_wires(),
    st.sampled_from((40, 48, 56)),
    st.sampled_from((float("nan"), float("inf"), -float("inf"))),
)
def test_hypothesis_nonfinite_lease_values_are_rejected(wire, offset, value):
    damaged = bytearray(wire)
    struct.pack_into("<d", damaged, offset, value)
    assert not call("lease", damaged.hex())["valid"]


@PROPERTY
@given(st.binary(min_size=0, max_size=160))
def test_hypothesis_arbitrary_wire_is_rejected_or_canonically_roundtrips(wire):
    result = call("lease", wire.hex())
    if result["valid"]:
        canonical = call("lease", result["wire"])
        assert canonical == result
        assert len(bytes.fromhex(result["wire"])) == 80


@PROPERTY
@given(lease_wires(), st.one_of(st.integers(0, 79), st.integers(81, 160)))
def test_hypothesis_valid_header_never_accepts_truncated_or_oversized_packet(wire, size):
    candidate = wire[:size] if size < 80 else wire + b"\0" * (size - 80)
    assert not call("lease", candidate.hex())["valid"]


@PROPERTY
@given(
    st.floats(0, 1.2, allow_nan=False, allow_infinity=False), st.integers(50, 1000), st.booleans()
)
@example(speed=2.225073858507e-311, duration=50, delivered=False)
@example(speed=1.2, duration=1000, delivered=True)
def test_hypothesis_gate_ack_expiry_refusal_and_nonincreasing_brake(speed, duration, delivered):
    result = call("gate", repr(speed), duration, int(delivered))
    assert result["active"] == delivered
    assert result["refused"] == 503
    assert result["sequence"] == 1
    previous = 0.0
    expiry_seen = False
    for stamp, mode, velocity in result["ticks"]:
        assert 0 <= velocity <= 1.2
        if delivered and stamp < 50 + duration:
            assert mode == 3
            assert velocity == speed
        if delivered and stamp >= 50 + duration and not expiry_seen:
            assert velocity == max(0.0, speed - 0.01)
            expiry_seen = True
        if not delivered:
            assert velocity == 0
            assert mode != 3  # MOVING
        if mode != 3:
            assert velocity <= previous + 1e-12
        if stamp >= 50 + duration:
            assert mode != 3
        previous = velocity
    assert previous == 0
    assert result["ticks"][-1][1] == 2  # READY after bounded braking/standstill.
