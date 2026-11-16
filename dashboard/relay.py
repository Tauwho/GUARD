"""Read-only localhost replay relay. No robot control endpoints."""

import argparse
import asyncio
import json
import math
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parents[1]
STATIC = Path(__file__).resolve().parent


def decode_event(line, validator):
    def finite(text):
        value = float(text)
        if not math.isfinite(value):
            raise ValueError("non-finite telemetry number")
        return value

    def integer(text):
        value = int(text)
        if abs(value) > int(sys.float_info.max):
            raise ValueError("telemetry integer exceeds browser number range")
        return value

    def constant(_text):
        raise ValueError("non-finite JSON constant")

    event = json.loads(line, parse_float=finite, parse_int=integer, parse_constant=constant)
    validator.validate(event)
    return event


def load_events(path):
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("replay exceeds the 32 MiB limit")
    validator = jsonschema.Draft202012Validator(
        json.loads((ROOT / "contracts/telemetry.schema.json").read_text())
    )
    events = []
    for line in path.read_text(encoding="utf-8").splitlines():
        event = decode_event(line, validator)
        events.append(event)
    if not events:
        raise ValueError("empty replay")
    return sorted(events, key=lambda event: event["t_ns"])


def http_handler(metadata):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path == "/meta.json":
                body = json.dumps(metadata).encode()
                content_type = "application/json"
            elif self.path in ("/", "/index.html", "/app.js", "/style.css"):
                filename = "index.html" if self.path == "/" else self.path[1:]
                body = (STATIC / filename).read_bytes()
                content_type = {"html": "text/html", "js": "text/javascript", "css": "text/css"}[
                    filename.rsplit(".", 1)[1]
                ]
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", content_type + "; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header(
                "Content-Security-Policy",
                "default-src 'self'; connect-src 'self' ws://127.0.0.1:* ws://localhost:*; "
                "object-src 'none'",
            )
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_args):
            pass

    return Handler


async def run(args):
    from websockets.asyncio.server import serve

    events = load_events(args.trace) if args.trace else []
    world = yaml.safe_load(args.map.read_text(encoding="utf-8"))
    jsonschema.validate(world, json.loads((ROOT / "contracts/scenario.schema.json").read_text()))
    zone_events = []
    identity = events[0].get("payload", {}).get("scenario", "") if events else ""
    if identity in {f"F{i}" for i in range(9)}:
        scenario = yaml.safe_load(
            (ROOT / "scenarios" / (identity + ".yaml")).read_text(encoding="utf-8")
        )
        if scenario["map"] == world["id"]:
            zone_events = scenario["zones"]
    metadata = {
        "map": world,
        "zone_events": zone_events,
        "source": "recorded telemetry" if args.trace else "live telemetry via localhost",
        "live": not bool(args.trace),
        "websocket_port": args.port + 1,
    }
    http = ThreadingHTTPServer(("127.0.0.1", args.port), http_handler(metadata))
    threading.Thread(target=http.serve_forever, daemon=True).start()

    async def replay(connection):
        if connection.request.path != "/events":
            await connection.close(1008, "read-only events endpoint")
            return
        if args.bridge:
            try:
                host, port = args.bridge.rsplit(":", 1)
                await stream_bridge(connection, host, int(port))
            except (OSError, ValueError, asyncio.TimeoutError) as error:
                await connection.send(json.dumps({"type": "error", "message": str(error)}))
            return
        for start in range(0, len(events), 200):
            await connection.send(
                json.dumps({"type": "events", "events": events[start : start + 200]})
            )
        await connection.send(json.dumps({"type": "end"}))

    try:
        async with serve(
            replay,
            "127.0.0.1",
            args.port + 1,
            origins=[f"http://127.0.0.1:{args.port}", f"http://localhost:{args.port}"],
            max_size=1024,
            close_timeout=2,
        ):
            print(f"Dashboard: http://127.0.0.1:{args.port} (read-only)", flush=True)
            await asyncio.Future()
    finally:
        http.shutdown()
        http.server_close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--trace", type=Path)
    source.add_argument("--bridge", help="localhost TCP bridge, e.g. 127.0.0.1:7700")
    parser.add_argument("--map", type=Path, default=ROOT / "scenarios/maps/hospital_corridor.yaml")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not 1024 <= args.port <= 65534:
        parser.error("port must be 1024..65534")
    if args.bridge:
        try:
            host, port = args.bridge.rsplit(":", 1)
            if host not in ("localhost", "127.0.0.1") or not 1 <= int(port) <= 65535:
                raise ValueError()
        except ValueError:
            parser.error(
                "bridge must be localhost:port or 127.0.0.1:port; forward the target port locally"
            )
    asyncio.run(run(args))


async def bridge_line(reader):
    oversized = False
    while True:
        try:
            line = await reader.readuntil(b"\n")
        except asyncio.LimitOverrunError as error:
            await reader.readexactly(error.consumed)
            oversized = True
            continue
        except asyncio.IncompleteReadError as error:
            return b"", oversized or bool(error.partial), True
        return line, oversized, False


async def stream_bridge(connection, host, port):
    validator = jsonschema.Draft202012Validator(
        json.loads((ROOT / "contracts/telemetry.schema.json").read_text())
    )
    reader, writer = await asyncio.wait_for(asyncio.open_connection(host, port, limit=65536), 5)
    try:
        while True:
            line, invalid, ended = await asyncio.wait_for(bridge_line(reader), 30)
            if not invalid and line:
                try:
                    event = decode_event(line, validator)
                except (ValueError, UnicodeError, RecursionError, jsonschema.ValidationError):
                    invalid = True
            if invalid:
                await connection.send(
                    json.dumps(
                        {"type": "drop", "count": 1, "message": "Malformed bridge record ignored"}
                    )
                )
            elif line:
                await connection.send(json.dumps({"type": "events", "events": [event]}))
            if ended:
                await connection.send(json.dumps({"type": "end"}))
                return
    finally:
        writer.close()
        await writer.wait_closed()


if __name__ == "__main__":
    main()
