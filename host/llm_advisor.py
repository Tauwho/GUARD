"""C9 recorded-response service; preserves original provenance and recorded delays."""

import argparse
import asyncio
import json
import math
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = json.loads((ROOT / "contracts/planner-remote.schema.json").read_text())
VALIDATOR = jsonschema.Draft202012Validator(SCHEMA)


def strict_json(text):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result

    def number(text):
        value = float(text)
        if not math.isfinite(value):
            raise ValueError("non-finite JSON number")
        return value

    def constant(_text):
        raise ValueError("non-finite JSON literal")

    return json.loads(text, object_pairs_hook=pairs, parse_float=number, parse_constant=constant)


def load_recording(path):
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("recording exceeds 16 MiB")
    records = []
    for line in path.read_text(encoding="utf-8").splitlines():
        record = strict_json(line)
        if set(record) != {"request", "response", "delay_ns", "source"}:
            raise ValueError("record needs request, response, delay_ns and source")
        request, response = record["request"], record["response"]
        VALIDATOR.validate(request)
        VALIDATOR.validate(response)
        if request["type"] != "obs_request" or response["type"] != "route":
            raise ValueError("recorded advisor accepts observation requests and route responses")
        if response["req_id"] != request["req_id"]:
            raise ValueError("recorded request/response correlation differs")
        expected = {key: request["obs"][key] for key in ("obs_id", "map_version", "stamp_ns")}
        if response["based_on"] != expected:
            raise ValueError("recorded response changed its source provenance")
        delay = record["delay_ns"]
        if type(delay) is not int or not 0 <= delay <= 60_000_000_000:
            raise ValueError("recorded delay must be 0..60 seconds in integer nanoseconds")
        if record["source"] not in ("ollama-recording", "synthetic-test"):
            raise ValueError("recording source must identify real Ollama data or a synthetic test")
        records.append(record)
    if not records:
        raise ValueError("empty recording")
    return records


class RecordedAdvisor:
    def __init__(self, records, sleep=asyncio.sleep):
        self.records = records
        self.index = 0
        self.sleep = sleep

    async def reply(self, request):
        VALIDATOR.validate(request)
        if self.index >= len(self.records):
            raise ValueError("recording exhausted")
        record = self.records[self.index]
        if record["request"] != request:
            raise ValueError("request differs from recording; provenance is never rewritten")
        self.index += 1
        await self.sleep(record["delay_ns"] / 1e9)
        return record["response"]


async def serve_client(reader, writer, records=None, advisor_factory=None):
    advisor = advisor_factory() if advisor_factory else RecordedAdvisor(records)
    try:
        while True:
            line = await asyncio.wait_for(reader.readline(), 30)
            if not line:
                return
            request = strict_json(line)
            response = await advisor.reply(request)
            writer.write((json.dumps(response, allow_nan=False) + "\n").encode())
            await writer.drain()
    except (ValueError, OSError, asyncio.TimeoutError, jsonschema.ValidationError) as error:
        # C9 has no error response variant: close the link rather than inventing one.
        print(f"advisor connection closed: {type(error).__name__}", flush=True)
    finally:
        writer.close()
        await writer.wait_closed()


async def run(args):
    records = load_recording(args.trace) if args.trace else None
    factory = None
    if args.ollama:
        from ollama_route import OllamaAdvisor, OllamaGenerator

        def factory():
            return OllamaAdvisor(OllamaGenerator(args.model), record_path=args.record)

    server = await asyncio.start_server(
        lambda reader, writer: serve_client(reader, writer, records, factory),
        "127.0.0.1",
        args.port,
        limit=65536,
    )
    async with server:
        source = records[0]["source"] if records else "live Ollama"
        print(f"Advisor: 127.0.0.1:{args.port}; source={source}", flush=True)
        await server.serve_forever()


def main():
    from ollama_route import DEFAULT_MODEL

    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--trace", type=Path)
    mode.add_argument("--ollama", action="store_true")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--record", type=Path)
    parser.add_argument("--port", type=int, default=7701)
    args = parser.parse_args()
    if args.record and not args.ollama:
        parser.error("--record is for live Ollama mode only")
    if not 1024 <= args.port <= 65535:
        parser.error("port must be 1024..65535")
    asyncio.run(run(args))


if __name__ == "__main__":
    main()
