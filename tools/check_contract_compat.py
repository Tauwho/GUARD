#!/usr/bin/env python3
"""Contracts are frozen at a tag; only additive files under contracts/ext/ may change (D26).

python tools/check_contract_compat.py --base contracts-v1.2
Checks (fail closed: a Git or parse error is a FAIL, never a silent PASS):
  1. every file under contracts/ at the base tag is unchanged, except that an existing
     contracts/ext/*.md file may only be appended to; new files live directly in contracts/ext/;
  2. every contracts/ext/<A|B>-*.json parses (no duplicate keys or non-finite numbers); a schema
     with "extends": "<file>" adds only optional properties or new `kind` enum values;
  3. reason codes and message type ids in ext files are integers in the stream's reserved range
     and never reuse a frozen or another extension's id;
  4. scenario safety invariants: for every scenario at the base tag, B2 keeps violations 0 and
     forbidden_reasons only grows.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

import yaml

ROOT = Path(__file__).resolve().parents[1]
RANGES = {
    "A": {"code": (900, 949), "msg": (0x0100, 0x01FF)},
    "B": {"code": (950, 999), "msg": (0x0200, 0x02FF)},
}
MAX_BYTES = 2 * 1024 * 1024
SCHEMA_KEYS = {
    "extends",
    "$schema",
    "$id",
    "title",
    "description",
    "type",
    "properties",
    "required",
    "additionalProperties",
    "x-reason-codes",
    "x-message-types",
}


class ContractError(ValueError):
    pass


def git(root, *args):
    return subprocess.run(
        ["git", *args], cwd=root, capture_output=True, check=True, timeout=60
    ).stdout


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"duplicate key {key!r}")
        result[key] = value
    return result


class UniqueLoader(yaml.SafeLoader):
    def compose_node(self, parent, index):
        if self.check_event(yaml.AliasEvent):
            raise ContractError("YAML aliases are not supported in contract inputs")
        return super().compose_node(parent, index)


def yaml_mapping(loader, node):
    loader.flatten_mapping(node)
    return unique_pairs(
        (loader.construct_object(k), loader.construct_object(v)) for k, v in node.value
    )


UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, yaml_mapping)


def reject_constant(value):
    raise ContractError(f"non-finite JSON value {value}")


def parse_json(raw):
    return json.loads(raw, object_pairs_hook=unique_pairs, parse_constant=reject_constant)


def resolve_tag(root, tag):
    ref = f"refs/tags/{tag}"
    try:
        git(root, "rev-parse", "-q", "--verify", f"{ref}^{{commit}}")
    except subprocess.CalledProcessError:
        git(root, "fetch", "-q", "--no-recurse-submodules", "origin", f"+{ref}:{ref}")
        git(root, "rev-parse", "-q", "--verify", f"{ref}^{{commit}}")
    return ref


def snapshot(root, ref, *paths):
    names = git(root, "ls-tree", "-r", "--name-only", "-z", ref, "--", *paths).decode().split("\0")
    return {name: git(root, "show", f"{ref}:{name}") for name in names if name}


def local_bytes(root, name):
    path = root / name
    if not path.resolve().is_relative_to(root.resolve()):
        raise ContractError(f"{name}: path escapes the checkout")
    if not path.is_file():
        raise ContractError(f"{name}: missing file")
    if path.stat().st_size > MAX_BYTES:
        raise ContractError(f"{name}: file exceeds the 2 MiB inspection limit")
    return path.read_bytes()


def integer(value, hex_allowed=False):
    if type(value) is int:
        return value
    if hex_allowed and isinstance(value, str) and re.fullmatch(r"0[xX][0-9a-fA-F]+|[0-9]+", value):
        return int(value, 0 if value.lower().startswith("0x") else 10)
    raise ContractError(f"expected an integer identifier, got {value!r}")


def check_extension_schema(data, frozen):
    name = data["extends"]
    if not isinstance(name, str) or ".." in PurePosixPath(name).parts or name.startswith("/"):
        raise ContractError("extends must name a frozen schema inside contracts/")
    key = f"contracts/{name}"
    if key not in frozen:
        raise ContractError(f"extends unknown frozen schema {name!r}")
    base = parse_json(frozen[key])
    unknown = set(data) - SCHEMA_KEYS
    if unknown:
        raise ContractError(f"keywords this check cannot verify as additive: {sorted(unknown)}")
    if data.get("required", []) or data.get("additionalProperties", True) is not True:
        raise ContractError("an extension may add only optional properties")
    if "type" in data and data["type"] != base.get("type"):
        raise ContractError("an extension may not change the frozen schema type")
    for prop, schema in data.get("properties", {}).items():
        old = base.get("properties", {}).get(prop)
        if old is None:
            continue
        if prop != "kind" or not isinstance(old, dict) or not isinstance(schema, dict):
            raise ContractError(f"redefines frozen property {prop!r}")
        old_values = old.get("enum", [old["const"]] if "const" in old else None)
        values = schema.get("enum")
        if old_values is None or not isinstance(values, list):
            raise ContractError("kind may only be extended as an enum")
        if not all(value in values for value in old_values):
            raise ContractError("kind enum must keep every frozen value")
        if set(schema) - {"enum", "type", "description", "title"} or (
            "type" in schema and schema["type"] != old.get("type", "string")
        ):
            raise ContractError("kind extension adds unsupported constraints")


def check_extension(root, name, frozen, identifiers):
    raw = local_bytes(root, name)
    path = PurePosixPath(name)
    if path.suffix == ".md":
        return
    if path.suffix != ".json":
        raise ContractError("extensions are .json or .md files")
    stream = path.name.split("-", 1)[0]
    if stream not in RANGES:
        raise ContractError("extension file names start with A- or B-")
    data = parse_json(raw)
    if not isinstance(data, dict) or not isinstance(data.get("properties", {}), dict):
        raise ContractError("an extension is a JSON object whose properties is an object")
    if "extends" in data:
        check_extension_schema(data, frozen)
    for field, domain in (("x-reason-codes", "code"), ("x-message-types", "msg")):
        entries = data.get(field, {})
        if not isinstance(entries, dict):
            raise ContractError(f"{field} must be an object")
        for symbol, value in entries.items():
            number = integer(value, hex_allowed=domain == "msg")
            low, high = RANGES[stream][domain]
            if not low <= number <= high:
                raise ContractError(
                    f"{symbol}: {domain} {number} outside stream {stream} range {low}-{high}"
                )
            if number in identifiers[domain]:
                raise ContractError(
                    f"{symbol}: {domain} {number} already used by {identifiers[domain][number]}"
                )
            identifiers[domain][number] = name


def check_scenario(root, name, raw):
    before = yaml.load(raw, Loader=UniqueLoader) or {}
    after = yaml.load(local_bytes(root, name), Loader=UniqueLoader) or {}
    old_b2 = (before.get("expected") or {}).get("B2") or {}
    new_b2 = (after.get("expected") or {}).get("B2") or {}
    if not isinstance(new_b2, dict) or not isinstance(new_b2.get("forbidden_reasons", []), list):
        raise ContractError("expected.B2 must be a mapping with a forbidden_reasons list")
    problems = []
    if old_b2.get("violations") == 0 and not (
        type(new_b2.get("violations")) is int and new_b2["violations"] == 0
    ):
        problems.append(f"{name}: B2 violations must stay 0")
    missing = set(old_b2.get("forbidden_reasons", [])) - set(new_b2.get("forbidden_reasons", []))
    if missing:
        problems.append(f"{name}: B2 forbidden_reasons removed {sorted(missing)}")
    return problems


def validate(root, base):
    ref = resolve_tag(root, base)
    frozen = snapshot(root, ref, "contracts", "scenarios")
    if not any(name.startswith("contracts/") for name in frozen):
        raise ContractError(f"{base} contains no contracts")
    problems = []
    for name, before in frozen.items():
        if not name.startswith("contracts/"):
            continue
        try:
            after = local_bytes(root, name)
        except ContractError as error:
            problems.append(f"{error}; frozen at {base}")
            continue
        appended = name.startswith("contracts/ext/") and name.endswith(".md")
        if after != before and not (appended and after.startswith(before)):
            problems.append(f"{name}: frozen at {base}; use contracts/ext/ instead")
    identifiers = {"code": {}, "msg": {}}
    reasons = frozen.get("contracts/reason_codes.yaml")
    if reasons:
        for symbol, entry in (yaml.load(reasons, Loader=UniqueLoader)["codes"]).items():
            identifiers["code"][integer(entry["number"])] = f"contracts/reason_codes.yaml {symbol}"
    listed = git(root, "ls-files", "-z", "--cached", "--others", "--exclude-standard", "contracts")
    for name in sorted(set(listed.decode().split("\0")) - {""}):
        if name in frozen and not name.startswith("contracts/ext/"):
            continue
        if PurePosixPath(name).parent != PurePosixPath("contracts/ext"):
            problems.append(f"{name}: new contract files go directly in contracts/ext/")
            continue
        try:
            check_extension(root, name, frozen, identifiers)
        except (ContractError, json.JSONDecodeError, UnicodeDecodeError) as error:
            problems.append(f"{name}: {error}")
    for name, raw in frozen.items():
        if re.fullmatch(r"scenarios/F\d+\.yaml", name):
            try:
                problems += check_scenario(root, name, raw)
            except (ContractError, yaml.YAMLError, AttributeError) as error:
                problems.append(f"{name}: invalid scenario ({error})")
    return problems


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--base", default="contracts-v1.2")
    parser.add_argument("--root", type=Path, default=ROOT, help="repository to check")
    args = parser.parse_args(argv)
    try:
        problems = validate(args.root.resolve(), args.base)
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or b"").decode(errors="replace").strip() or str(error)
        print(f"contract-compat: FAIL git: {detail}")
        return 1
    except (OSError, subprocess.SubprocessError, ValueError, yaml.YAMLError) as error:
        print(f"contract-compat: FAIL {error}")
        return 1
    except (KeyError, TypeError, RecursionError) as error:
        print(f"contract-compat: FAIL malformed contract input ({error!r})")
        return 1
    if problems:
        print("contract-compat: FAIL")
        print("\n".join("  " + p for p in problems))
        return 1
    print(f"contract-compat: PASS against {args.base}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
