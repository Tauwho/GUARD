"""Local Ollama adapter. Model output cannot select or refresh its provenance."""

import asyncio
import json
import math
import time
from pathlib import Path
from urllib.request import Request, urlopen

import jsonschema

DEFAULT_MODEL = "qwen2.5:1.5b-instruct"

OUTPUT_SCHEMA = {
    "type": "object",
    "required": ["waypoints"],
    "additionalProperties": False,
    "properties": {
        "waypoints": {
            "type": "array",
            "minItems": 1,
            "maxItems": 16,
            "items": {
                "type": "array",
                "minItems": 2,
                "maxItems": 2,
                "items": {"type": "number", "minimum": -100, "maximum": 100},
            },
        },
        "rationale": {"type": "string", "maxLength": 500},
    },
}


class OllamaGenerator:
    def __init__(self, model=DEFAULT_MODEL, timeout=30):
        self.model, self.timeout = model, timeout

    def __call__(self, request):
        if "goal" not in request:
            raise ValueError("live route request requires a goal")
        body = {
            "model": self.model,
            "stream": False,
            "format": {
                **OUTPUT_SCHEMA,
                "properties": {
                    "waypoints": {**OUTPUT_SCHEMA["properties"]["waypoints"], "maxItems": 3}
                },
            },
            "prompt": "Plan a short polyline from the observed x,y to the goal x,y. "
            "The LAST waypoint MUST equal the goal exactly. Use 1 to 3 waypoints. "
            "Do not include the starting point or pad a sequence of tiny steps. "
            "Output ONLY a JSON object with waypoints, no rationale. "
            "This is an advisory route in a simulation. "
            "Observation and goal: "
            + json.dumps({"obs": request["obs"], "goal": request["goal"]}, allow_nan=False),
            "options": {
                "temperature": 0,
                "seed": 0,
                "num_predict": 256,
                "num_ctx": 4096,
                "num_thread": 2,
                "num_gpu": 0,
            },
        }
        message = Request(
            "http://127.0.0.1:11434/api/generate",
            data=json.dumps(body).encode(),
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        with urlopen(message, timeout=self.timeout) as response:
            raw = response.read(1_048_577)
        if len(raw) > 1_048_576:
            raise ValueError("Ollama response exceeds 1 MiB")
        from llm_advisor import strict_json

        payload = strict_json(raw)
        if not payload.get("done"):
            raise ValueError("Ollama response incomplete")
        result = strict_json(payload["response"])
        jsonschema.validate(result, OUTPUT_SCHEMA)
        return result


class OllamaAdvisor:
    def __init__(self, generator=None, clock=time.perf_counter_ns, record_path=None):
        self.generator = generator or OllamaGenerator()
        self.clock = clock
        self.record_path = record_path
        self.route_id = 0

    async def reply(self, request):
        from llm_advisor import VALIDATOR

        VALIDATOR.validate(request)
        if request["type"] != "obs_request":
            raise ValueError("expected observation request")
        started = self.clock()
        generated = await asyncio.wait_for(asyncio.to_thread(self.generator, request), 30)
        elapsed = self.clock() - started
        if elapsed < 0 or elapsed > 60_000_000_000:
            raise ValueError("invalid or excessive compute duration")
        jsonschema.validate(generated, OUTPUT_SCHEMA)
        goal = request.get("goal")
        if goal is None or math.dist(generated["waypoints"][-1], goal) > 0.01:
            raise ValueError("model route does not reach the requested goal")
        source = {key: request["obs"][key] for key in ("obs_id", "stamp_ns", "map_version")}
        created = source["stamp_ns"] + elapsed
        if created > 2**63 - 1:
            raise ValueError("target-clock timestamp exceeds int64")
        self.route_id += 1
        response = {
            "v": 1,
            "type": "route",
            "req_id": request["req_id"],
            "route_id": self.route_id,
            "based_on": source,
            "waypoints": generated["waypoints"],
            "created_ns": created,
        }
        if "rationale" in generated:
            response["rationale"] = generated["rationale"]
        VALIDATOR.validate(response)
        if self.record_path:
            record = {
                "request": request,
                "response": response,
                "delay_ns": elapsed,
                "source": "ollama-recording",
            }
            with Path(self.record_path).open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, allow_nan=False) + "\n")
        return response
