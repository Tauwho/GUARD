"""Generate pure C++ reason metadata and fixture tests on the development host."""

import argparse
import json
from pathlib import Path

import yaml


def assignments(prefix, value):
    if isinstance(value, dict):
        return [
            line for key, item in value.items() for line in assignments(f"{prefix}.{key}", item)
        ]
    if isinstance(value, list):
        return [
            line for i, item in enumerate(value) for line in assignments(f"{prefix}[{i}]", item)
        ]
    if isinstance(value, str):
        literal = json.dumps(value)
        return [f"std::copy_n({literal}, {len(value.encode())}, {prefix}.begin());"]
    return [f"{prefix} = {str(value).lower()};"]


def generate(contracts, output):
    output.mkdir(parents=True, exist_ok=True)
    metadata = yaml.safe_load((contracts / "reason_codes.yaml").read_text())
    entries = metadata["codes"]
    numbers = [v["number"] for v in entries.values()]
    if len(set(numbers)) != len(numbers):
        raise ValueError("Reason numbers must be unique")
    lines = [
        "// Generated from contracts/reason_codes.yaml. Do not edit.",
        "#pragma once",
        "#include <cstdint>",
        "#include <string_view>",
        "namespace guard {",
        "enum class Reason : std::uint16_t {",
    ]
    lines += [f"    {name} = {data['number']}," for name, data in entries.items()]
    lines += [
        "};",
        "constexpr std::string_view reason_name(std::uint16_t value) {",
        "    switch (value) {",
    ]
    lines += [f'    case {data["number"]}: return "{name}";' for name, data in entries.items()]
    lines += ['    default: return "UNKNOWN";', "    }", "}", "} // namespace guard"]
    (output / "reason_codes.hpp").write_text("\n".join(lines) + "\n")
    tests = [
        "// Generated from contract fixtures. Do not edit.",
        '#include "guard/codec.hpp"',
        "#include <doctest/doctest.h>",
        "#include <algorithm>",
    ]
    errors = {
        "EBADMSG": "bad_message",
        "ENOTSUP": "unsupported_version",
        "EMSGSIZE": "wrong_size",
        "REJ_SCHEMA": "schema",
        "EINVAL": "invalid_value",
    }
    for path in sorted((contracts / "fixtures/messages").glob("*.json")):
        fixture = json.loads(path.read_text())
        typename = "".join(part.title() for part in fixture["message"].split("_"))
        wire = bytes.fromhex(fixture["hex"])
        tests += [
            f'TEST_CASE("fixture {path.name}") {{',
            f"    std::array<std::uint8_t, {len(wire)}> bytes{{"
            + ",".join(str(b) for b in wire)
            + "};",
            f"    guard::{typename} value{{}};",
        ]
        expected = "none" if fixture["valid"] else errors[fixture["expect_error"]]
        tests += [
            f"    CHECK(guard::decode(bytes.data(), bytes.size(), value) == "
            f"guard::DecodeError::{expected});"
        ]
        if fixture["valid"]:
            tests += [f"    guard::{typename} source{{}};"]
            tests += ["    " + line for line in assignments("source", fixture["fields"])]
            tests += [
                f"    std::array<std::uint8_t, sizeof(guard::{typename})> encoded{{}};",
                "    REQUIRE(guard::encode(source, encoded));",
                "    CHECK(encoded == bytes);",
            ]
        tests += ["}"]
    (output / "contracts_fixtures.cpp").write_text("\n".join(tests) + "\n")
    loader_tests = [
        "// Generated from shared YAML and JSON fixtures. No runtime file reads.",
        '#include "guard/scenario.hpp"',
        '#include "guard/config.hpp"',
        "#include <doctest/doctest.h>",
    ]
    for path in sorted((contracts / "fixtures/schemas").glob("config.*.json")):
        data = json.loads(path.read_text())
        expected = "true" if path.name.endswith(".valid.json") else "false"
        loader_tests += [
            f'TEST_CASE("loader {path.name}") {{',
            "    guard::Config config;",
            f"    CHECK(guard::load_config({json.dumps(json.dumps(data))}, config) == {expected});",
            "}",
        ]
    scenario_root = contracts.parent / "scenarios"
    for path in sorted((scenario_root / "maps").glob("*.yaml")):
        data = yaml.safe_load(path.read_text())
        loader_tests += [
            f'TEST_CASE("loader map {path.name}") {{',
            "    guard::MapSpec map;",
            f"    REQUIRE(guard::load_map({json.dumps(json.dumps(data))}, map));",
            f'    CHECK(guard::name_view(map.id) == "{data["id"]}");',
            "}",
        ]
    for path in sorted(scenario_root.glob("F*.yaml")):
        data = yaml.safe_load(path.read_text())
        map_data = yaml.safe_load((scenario_root / "maps" / f"{data['map']}.yaml").read_text())
        loader_tests += [
            f'TEST_CASE("loader scenario {path.name}") {{',
            "    guard::MapSpec map;",
            "    guard::ScenarioSpec scenario;",
            f"    REQUIRE(guard::load_map({json.dumps(json.dumps(map_data))}, map));",
            f"    REQUIRE(guard::load_scenario({json.dumps(json.dumps(data))}, map, scenario));",
            f"    CHECK(scenario.id == {int(data['id'][1])});",
            f"    CHECK(scenario.seed == {data['seed']}ULL);",
            "}",
        ]
    (output / "loaders_fixtures.cpp").write_text("\n".join(loader_tests) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("contracts", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    generate(arguments.contracts, arguments.output)
