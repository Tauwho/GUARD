import copy
import json
import runpy
from pathlib import Path

import jsonschema

ROOT = Path(__file__).parents[2]
SCHEMA = json.loads((ROOT / "contracts/telemetry.schema.json").read_text())
FAST = runpy.run_path(str(ROOT / "host/telemetry_validation.py"))["C8Validator"]


def accepts(validator, event):
    try:
        validator.validate(event)
        return True
    except jsonschema.ValidationError:
        return False


def test_dispatched_validation_matches_frozen_schema_for_all_kinds_and_mutations():
    full, fast = jsonschema.Draft202012Validator(SCHEMA), FAST(SCHEMA)
    fixture = json.loads((ROOT / "contracts/fixtures/schemas/telemetry.valid.json").read_text())
    candidates = [fixture, None, [], {}, {**fixture, "extra": 1}]
    for kind in SCHEMA["properties"]["kind"]["enum"]:
        for payload in ({}, fixture["payload"], None, [], {"not-a-field": 1}):
            candidates.append({**fixture, "kind": kind, "payload": payload})
    for key in fixture:
        for value in (None, -1, "wrong", [], {}, True):
            candidates.append({**fixture, key: value})
        missing = dict(fixture)
        del missing[key]
        candidates.append(missing)
    assert any(accepts(full, event) for event in candidates)
    assert any(not accepts(full, event) for event in candidates)
    for event in candidates:
        assert accepts(fast, event) == accepts(full, event), event


def test_unknown_conditional_shape_uses_the_full_schema():
    schema = copy.deepcopy(SCHEMA)
    schema["allOf"].append({"not": {"properties": {"seq": {"const": 88}}}})
    fast = FAST(schema)
    assert fast.envelope is None
    event = json.loads((ROOT / "contracts/fixtures/schemas/telemetry.valid.json").read_text())
    assert not accepts(fast, event)
    event["seq"] = 89
    assert accepts(fast, event)


def test_payload_reference_to_original_root_uses_full_validator():
    schema = copy.deepcopy(SCHEMA)
    schema["allOf"][0]["then"]["properties"]["payload"] = {"$ref": "#/properties/payload"}
    fast = FAST(schema)
    assert fast.envelope is None
    event = json.loads((ROOT / "contracts/fixtures/schemas/telemetry.valid.json").read_text())
    event["payload"] = {}
    assert accepts(fast, event) == accepts(jsonschema.Draft202012Validator(schema), event)


def test_nonlocal_definition_reference_disables_dispatch():
    schema = copy.deepcopy(SCHEMA)
    schema["$defs"]["decision"]["properties"]["proposal_id"] = {"$ref": "#/properties/seq"}
    fast = FAST(schema)
    assert fast.envelope is None
