"""Read-only localhost replay relay. No robot control endpoints."""

import argparse
import asyncio
import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parents[1]
STATIC = Path(__file__).resolve().parent


def load_events(path):
    if path.stat().st_size > 32 * 1024 * 1024:
        raise ValueError("replay exceeds the 32 MiB limit")
    validator = jsonschema.Draft202012Validator(
        json.loads((ROOT / "contracts/telemetry.schema.json").read_text())
    )
    events = []
    for line in path.read_text(encoding="utf-8").splitlines():
        event = json.loads(line)
        validator.validate(event)
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

    events = load_events(args.trace)
    world = yaml.safe_load(args.map.read_text(encoding="utf-8"))
    jsonschema.validate(world, json.loads((ROOT / "contracts/scenario.schema.json").read_text()))
    zone_events = []
    identity = events[0].get("payload", {}).get("scenario", "")
    if identity in {f"F{i}" for i in range(9)}:
        scenario = yaml.safe_load(
            (ROOT / "scenarios" / (identity + ".yaml")).read_text(encoding="utf-8")
        )
        if scenario["map"] == world["id"]:
            zone_events = scenario["zones"]
    metadata = {
        "map": world,
        "zone_events": zone_events,
        "source": "recorded telemetry",
        "websocket_port": args.port + 1,
    }
    http = ThreadingHTTPServer(("127.0.0.1", args.port), http_handler(metadata))
    threading.Thread(target=http.serve_forever, daemon=True).start()

    async def replay(connection):
        if connection.request.path != "/events":
            await connection.close(1008, "read-only events endpoint")
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
            print(f"Dashboard: http://127.0.0.1:{args.port} (read-only replay)", flush=True)
            await asyncio.Future()
    finally:
        http.shutdown()
        http.server_close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--map", type=Path, default=ROOT / "scenarios/maps/hospital_corridor.yaml")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    if not 1024 <= args.port <= 65534:
        parser.error("port must be 1024..65534")
    asyncio.run(run(args))


if __name__ == "__main__":
    main()
