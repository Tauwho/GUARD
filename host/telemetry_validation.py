"""Dispatch the frozen C8 payload schemas after validating the common envelope.

This avoids evaluating every mutually exclusive kind condition for every event.
All values still pass through jsonschema; unrecognized schema shapes use the full
validator. No contract constraints are replaced by handwritten checks.
"""

import jsonschema


def local_definition_refs(value, definitions):
    if isinstance(value, list):
        return all(local_definition_refs(item, definitions) for item in value)
    if not isinstance(value, dict):
        return True
    if any(
        key in value for key in ("$id", "$anchor", "$dynamicAnchor", "$dynamicRef", "$recursiveRef")
    ):
        return False
    if "$ref" in value and value["$ref"] not in {"#/$defs/" + key for key in definitions}:
        return False
    return all(local_definition_refs(item, definitions) for item in value.values())


class C8Validator:
    def __init__(self, schema):
        self.full = jsonschema.Draft202012Validator(schema)
        self.payloads = {}
        self.envelope = None
        if not {"kind", "payload"}.issubset(schema.get("required", [])):
            return
        definitions = schema.get("$defs", {})
        if not local_definition_refs(definitions, definitions):
            return
        for branch in schema.get("allOf", []):
            if not isinstance(branch, dict):
                return
            condition = branch.get("if", {})
            then = branch.get("then", {})
            if not isinstance(condition, dict) or not isinstance(then, dict):
                return
            properties = condition.get("properties", {})
            if not isinstance(properties, dict) or not isinstance(then.get("properties", {}), dict):
                return
            if not isinstance(properties.get("kind"), dict):
                return
            if (
                set(branch) != {"if", "then"}
                or set(condition) != {"properties"}
                or set(properties) != {"kind"}
                or set(properties["kind"]) != {"const"}
                or set(then) != {"properties"}
                or set(then["properties"]) != {"payload"}
            ):
                self.envelope = None
                return
            kind = properties["kind"]["const"]
            if not isinstance(kind, str) or kind in self.payloads:
                self.envelope = None
                return
            if not isinstance(then["properties"]["payload"], dict):
                return
            payload = dict(then["properties"]["payload"])
            if set(payload) != {"$ref"} or payload["$ref"] not in {
                "#/$defs/" + key for key in definitions
            }:
                return
            payload["$defs"] = schema.get("$defs", {})
            self.payloads[kind] = jsonschema.Draft202012Validator(payload)
        envelope = {key: value for key, value in schema.items() if key != "allOf"}
        self.envelope = jsonschema.Draft202012Validator(envelope)

    def validate(self, event):
        if self.envelope is None:
            self.full.validate(event)
            return
        self.envelope.validate(event)
        payload = self.payloads.get(event["kind"])
        if payload is not None:
            payload.validate(event["payload"])
