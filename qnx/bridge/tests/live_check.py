"""Host-side live check of guard-bridge (B5, #42).

Run on the QNX host while the target image runs (ssh alias guard-target, TCP 7700 forwarded):

    python3 qnx/bridge/tests/live_check.py

1. Loads and starts F0 with guardctl over ssh.
2. Reads the C8 stream straight from the bridge (127.0.0.1:7700) and validates every line
   against contracts/telemetry.schema.json (t_ns non-decreasing, seq +1 per src).
3. Starts dashboard/relay.py --bridge 127.0.0.1:7700 and counts the events the dashboard
   WebSocket delivers during the run.
Prints CHECK lines and counts only (no timing values, D11). Exit 0 when all pass.
"""

import asyncio
import json
import socket
import subprocess
import sys
import time
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[3]
SCHEMA = json.loads((ROOT / "contracts/telemetry.schema.json").read_text())
FAILED = []


def check(name, ok):
    print(f"CHECK {name:<58} {'PASS' if ok else 'FAIL'}")
    if not ok:
        FAILED.append(name)


def guardctl(*args):
    command = ["ssh", "-o", "BatchMode=yes", "guard-target", "guardctl " + " ".join(args)]
    return subprocess.run(command, capture_output=True, text=True, timeout=20).returncode


def read_bridge(seconds):
    validator = jsonschema.Draft202012Validator(SCHEMA)
    events, bad = [], 0
    buffer = b""
    with socket.create_connection(("127.0.0.1", 7700), timeout=5) as conn:
        conn.settimeout(0.5)
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            try:
                chunk = conn.recv(65536)
            except TimeoutError:
                continue
            if not chunk:
                break
            buffer += chunk
            *lines, buffer = buffer.split(b"\n")
            for line in lines:
                event = json.loads(line)
                bad += 0 if validator.is_valid(event) else 1
                events.append(event)
    return events, bad


async def dashboard_events(seconds):
    from websockets.asyncio.client import connect

    relay = subprocess.Popen(
        [sys.executable, str(ROOT / "dashboard/relay.py"), "--bridge", "127.0.0.1:7700"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    kinds = {}
    try:
        await asyncio.sleep(2)
        async with connect("ws://127.0.0.1:8766/events", origin="http://127.0.0.1:8765") as ws:
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                try:
                    message = json.loads(await asyncio.wait_for(ws.recv(), timeout=1))
                except TimeoutError:
                    continue
                for event in message.get("events", []):
                    kinds[event["kind"]] = kinds.get(event["kind"], 0) + 1
    finally:
        relay.terminate()
        relay.wait(timeout=5)
    return kinds


def main():
    guardctl("scenario", "stop")
    check(
        "guardctl scenario load F0",
        guardctl("scenario", "load", "/system/guard/scenarios/F0.yaml", "--seed", "1000") == 0,
    )
    check("guardctl scenario start", guardctl("scenario", "start") == 0)
    events, bad = read_bridge(6)
    order = all(b["t_ns"] >= a["t_ns"] for a, b in zip(events, events[1:]))
    seqs, gaps = {}, 0
    for e in events:
        if e["src"] in seqs and e["seq"] != seqs[e["src"]] + 1:
            gaps += 1
        seqs[e["src"]] = e["seq"]
    sources = sorted({e["src"] for e in events})
    kinds = sorted({e["kind"] for e in events})
    print(f"bridge: {len(events)} events, sources {sources}, kinds {kinds}, seq gaps {gaps}")
    check("bridge stream: every line valid C8", events and bad == 0)
    check("bridge stream: t_ns never decreases", order)
    check(
        "bridge stream: sim, gate and supervisor events",
        {"sim", "gate", "supervisor"} <= set(sources),
    )
    check(
        "bridge stream: decision, lease and observation records",
        {"decision", "lease", "observation"} <= set(kinds),
    )
    check("bridge stream: no seq gaps (no drops)", gaps == 0)
    shown = asyncio.run(dashboard_events(6))
    print(f"dashboard: {sum(shown.values())} events relayed, kinds {sorted(shown)}")
    check("dashboard relay shows the live target run", shown.get("decision", 0) > 10)
    guardctl("scenario", "stop")
    print(f"RESULT {'FAIL' if FAILED else 'PASS'} ({len(FAILED)} failed)")
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
