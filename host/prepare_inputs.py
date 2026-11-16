"""Validate shared YAML on the host and emit JSON-subset YAML for the portable loaders."""

import argparse
import hashlib
import json
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parents[1]


def prepare(source, destination, config_path=None):
    source, destination = source.resolve(), destination.resolve()
    if destination.is_relative_to(source) or source.is_relative_to(destination):
        raise ValueError("use a separate output directory, for example out/runtime-inputs")
    schema = json.loads((ROOT / "contracts/scenario.schema.json").read_text())
    files = {}
    maps = set()
    scenarios = []
    for path in sorted(source.rglob("*.yaml")):
        data = yaml.safe_load(path.read_text(encoding="utf-8"))
        jsonschema.validate(data, schema)
        if data["kind"] == "map":
            if data["id"] in maps:
                raise ValueError("duplicate map id")
            maps.add(data["id"])
        else:
            scenarios.append(data)
        files[path.relative_to(source)] = data
    if not files:
        raise ValueError("no scenario/map inputs found")
    for scenario in scenarios:
        if scenario["map"] not in maps:
            raise ValueError(f"missing map {scenario['map']}")
    config = (
        yaml.safe_load(config_path.read_text(encoding="utf-8"))
        if config_path
        else {"version": 1, "mode": "B2"}
    )
    jsonschema.validate(config, json.loads((ROOT / "contracts/config.schema.json").read_text()))
    files[Path("config.yaml")] = config
    encoded = {
        path: json.dumps(data, ensure_ascii=True, allow_nan=False, indent=2) + "\n"
        for path, data in files.items()
    }
    manifest = {}
    for relative, text in encoded.items():
        output = destination / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8", newline="\n")
        manifest[relative.as_posix()] = hashlib.sha256(text.encode()).hexdigest()
    (destination / "inputs.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "scenarios")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--config", type=Path)
    args = parser.parse_args()
    result = prepare(args.source, args.out, args.config)
    print(f"Prepared {len(result)} validated inputs under {args.out}")


if __name__ == "__main__":
    main()
