"""Drive C14 and validate C8: local virtual-time mock or an SSH target via a local bridge."""

import argparse
import json
import re
import runpy
import shlex
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parents[1]
API = runpy.run_path(str(ROOT / "host/guard-replay"))
SCHEMA = json.loads((ROOT / "contracts/telemetry.schema.json").read_text())


def validate_trace(path, scenario, mode, seed=None):
    events = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    validator = jsonschema.Draft202012Validator(SCHEMA)
    for event in events:
        validator.validate(event)
    if not events or events[0]["kind"] != "run_start" or events[-1]["kind"] != "run_end":
        raise ValueError("trace must contain a complete run_start/run_end pair")
    if (
        sum(e["kind"] == "run_start" for e in events) != 1
        or sum(e["kind"] == "run_end" for e in events) != 1
    ):
        raise ValueError("trace contains multiple runs")
    start = events[0]["payload"]
    if start["scenario"] != scenario["id"] or start["mode"] != mode:
        raise ValueError("trace scenario/mode differs from requested run")
    if seed is not None and start["seed"] != seed:
        raise ValueError("trace seed differs from requested run")
    if any(b["t_ns"] < a["t_ns"] for a, b in zip(events, events[1:])):
        raise ValueError("trace clock moved backwards")
    summary = API["summarize"](events)
    return {
        "summary": summary,
        "mismatches": API["mismatches"](scenario["expected"][mode], summary),
    }


def injection(fault):
    command = ["inject", fault["fault"]]
    params = fault.get("params", {})
    duration = (
        params.get("resume_after_ms", fault.get("duration_ms"))
        if fault["fault"] == "F7"
        else fault.get("duration_ms")
    )
    if duration is not None:
        command += ["--duration-ms", str(duration)]
    key = {"F1": "lag_ms", "F5": "v_cmd", "F8": "rate_hz"}.get(fault["fault"])
    if key in params:
        value = round(params[key] * 1000) if key == "v_cmd" else params[key]
        command += ["--param", str(value)]
    return command


class LocalTarget:
    """C14 lifecycle mock; the harness injects scheduled process faults in virtual time."""

    def __init__(self, scenario, seed, mode, output, binary=None):
        self.scenario, self.seed, self.mode = scenario, seed, mode
        self.output, self.binary = output, binary
        self.commands = []

    def control(self, *arguments):
        self.commands.append(["guardctl", *arguments])

    def collect(self):
        command = [
            sys.executable,
            str(ROOT / "host/guard-replay"),
            "--scenario",
            str(self.scenario),
            "--seed",
            str(self.seed),
            "--mode",
            self.mode,
            "--out",
            str(self.output),
        ]
        if self.binary:
            command += ["--binary", str(self.binary)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=45)
        if result.returncode not in (0, 1):
            raise ValueError("local replay failed: " + result.stderr[-1000:])


class SshTarget:
    def __init__(self, alias, bridge, output):
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", alias):
            raise ValueError("use an SSH config alias, not flags or shell commands")
        host, port = bridge.rsplit(":", 1)
        if host not in ("127.0.0.1", "localhost") or not 1 <= int(port) <= 65535:
            raise ValueError("forward the bridge to localhost:port")
        self.alias, self.bridge, self.output = alias, (host, int(port)), output
        self.commands = []

    def control(self, *arguments):
        self.commands.append(["guardctl", *arguments])
        result = subprocess.run(
            [
                "ssh",
                "-o",
                "BatchMode=yes",
                "-o",
                "ConnectTimeout=5",
                self.alias,
                shlex.join(["guardctl", *arguments]),
            ],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode:
            raise ValueError(f"guardctl exit {result.returncode}: {result.stderr.strip()[:500]}")

    def collect(self, connection, errors, finished):
        try:
            total = 0
            active = False
            with connection.makefile("rb") as reader, self.output.open("wb") as output:
                while True:
                    line = reader.readline(65537)
                    total += len(line)
                    if not line or len(line) > 65536 or total > 32 * 1024 * 1024:
                        raise ValueError("bridge ended early or exceeded trace/line limits")
                    event = json.loads(line)
                    active = active or event.get("kind") == "run_start"
                    if not active:
                        continue
                    output.write(line)
                    if event.get("kind") == "run_end":
                        break
        except (OSError, ValueError) as error:
            errors.append(error)
        finally:
            finished.set()


def run_case(
    target,
    scenario_path,
    output,
    seed=None,
    mode="B2",
    binary=None,
    bridge="127.0.0.1:7700",
    remote_path=None,
):
    scenario_path, output = Path(scenario_path).resolve(), Path(output).resolve()
    if output == scenario_path:
        raise ValueError("output must not overwrite the input scenario")
    scenario = yaml.safe_load(scenario_path.read_text(encoding="utf-8"))
    jsonschema.validate(scenario, json.loads((ROOT / "contracts/scenario.schema.json").read_text()))
    seed = scenario["seed"] if seed is None else seed
    if not 0 <= seed <= 2**64 - 1 or mode not in ("B0", "B1", "B2"):
        raise ValueError("invalid seed or mode")
    output.parent.mkdir(parents=True, exist_ok=True)
    local = target == "local"
    if not local and not target.startswith("ssh:"):
        raise ValueError("target must be local or ssh:<config-alias>")
    adapter = (
        LocalTarget(scenario_path, seed, mode, output, binary)
        if local
        else SshTarget(target[4:], bridge, output)
    )
    # World handles model faults. Only real process faults need external C14 injection.
    faults = sorted(
        (
            f
            for f in scenario["faults"]
            if f["fault"] in ("F2", "F3", "F7") and f["t_s"] < scenario["duration_s"]
        ),
        key=lambda f: f["t_s"],
    )
    errors, finished, connection, worker = [], threading.Event(), None, None
    loaded = False
    try:
        adapter.control(
            "scenario",
            "load",
            str(scenario_path)
            if local
            else (remote_path or "/system/guard/scenarios/" + scenario_path.name),
            "--seed",
            str(seed),
            "--mode",
            mode,
        )
        loaded = True
        if not local:
            connection = socket.create_connection(adapter.bridge, timeout=5)
            connection.settimeout(scenario["duration_s"] + 15)
            worker = threading.Thread(
                target=adapter.collect, args=(connection, errors, finished), daemon=True
            )
            worker.start()
        adapter.control("scenario", "start")
        started = time.monotonic()
        for fault in faults:
            if not local and finished.wait(max(0, started + fault["t_s"] - time.monotonic())):
                break
            adapter.control(*injection(fault))
        if local:
            adapter.collect()
        elif not finished.wait(max(0, started + scenario["duration_s"] + 15 - time.monotonic())):
            raise ValueError("target run_end deadline exceeded")
        if errors:
            raise ValueError("bridge capture failed: " + str(errors[0]))
    finally:
        try:
            if loaded:
                adapter.control("scenario", "stop")
        finally:
            if connection:
                try:
                    connection.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass
                connection.close()
            if worker:
                worker.join(timeout=2)
    result = validate_trace(output, scenario, mode, seed)
    result.update(
        source="local virtual-time mock" if local else "target bridge", commands=adapter.commands
    )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", default="local")
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--mode", choices=("B0", "B1", "B2"), default="B2")
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--bridge", default="127.0.0.1:7700")
    parser.add_argument(
        "--remote-path", help="normalized scenario path already installed on target"
    )
    args = parser.parse_args()
    try:
        result = run_case(
            args.target,
            args.scenario,
            args.out,
            args.seed,
            args.mode,
            args.binary,
            args.bridge,
            args.remote_path,
        )
        args.out.with_suffix(".results.json").write_text(
            json.dumps(result, indent=2), encoding="utf-8"
        )
        print(json.dumps(result))
        return 1 if result["mismatches"] else 0
    except (OSError, ValueError, subprocess.SubprocessError, jsonschema.ValidationError) as error:
        print(f"target-runner: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
