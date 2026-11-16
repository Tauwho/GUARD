"""Host-only repository checks. Run with a check name, or 'all'."""

import argparse
import json
import re
import subprocess
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parents[1]
CHECKS = ("core-portable", "no-qnx-files", "claims-lint", "schemas", "evidence")
CPP = {".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".cxx"}
FORBIDDEN_CORE = re.compile(
    r"sys/(?:neutrino|iofunc|dispatch)\.h|\b(?:Msg\w+|resmgr_\w+|slog2\w*|"
    r"clock_gettime|ClockCycles|fopen|freopen|fread|fwrite|"
    r"open|read|write|socket|connect|recv|send)\s*\(|\b(?:ifstream|ofstream|fstream)\b"
)
FORBIDDEN_FILE = re.compile(
    r"(?:^|/)(?:qnxsdp-env[^/]*|id_rsa[^/]*|id_ed25519[^/]*|licen[cs]e(?:[_-]key|\.dat|\.lic)[^/]*)$"
    r"|\.(?:ifs|qcow2|vmdk|img|lic|pem|key|p12|exe|dll|so|a|o)$",
    re.I,
)
SECRET = re.compile(
    r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|"
    r"\bgh[pousr]_[A-Za-z0-9]{20,}|\bgithub_pat_[A-Za-z0-9_]{20,}|"
    r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b",
    re.I,
)


def tracked(root):
    result = subprocess.run(["git", "ls-files", "-z"], cwd=root, check=True, capture_output=True)
    return [Path(p) for p in result.stdout.decode().split("\0") if p]


def known_commit(root, sha):
    """D21: an ancestor of HEAD, or contained in a PR head fetched as refs/remotes/pr/*."""

    def git(*args):
        return subprocess.run(["git", *args], cwd=root, capture_output=True, text=True)

    if git("merge-base", "--is-ancestor", sha, "HEAD").returncode == 0:
        return True
    heads = git("branch", "-r", "--contains", sha, "--list", "pr/*")
    return heads.returncode == 0 and bool(heads.stdout.strip())


def check(root, name):
    errors = []
    paths = tracked(root)
    if name in ("core-portable", "no-qnx-files", "claims-lint"):
        patterns = (root / "tools/claims-lint.txt").read_text().splitlines()
        claims = re.compile("|".join(patterns), re.I)
        for path in paths:
            if name == "core-portable" and not (path.parts[0] == "core" and path.suffix in CPP):
                continue
            if name == "claims-lint" and not (path == Path("README.md") or path.parts[0] == "docs"):
                continue
            if name == "no-qnx-files" and FORBIDDEN_FILE.search(path.as_posix()):
                errors.append(f"{path}: prohibited artifact filename")
            raw = (root / path).read_bytes()
            if (
                name == "no-qnx-files"
                and path.suffix in CPP
                and re.search(rb"copyright[^\n]*(?:QNX|BlackBerry)", raw, re.I)
            ):
                errors.append(f"{path}: possible copied SDP source")
            if b"\0" in raw:
                if name == "no-qnx-files" and not (
                    path.as_posix().startswith("evidence/submission/") and path.suffix == ".png"
                ):
                    errors.append(f"{path}: binary artifact")
                continue
            exempt = False
            for number, line in enumerate(raw.decode("utf-8", errors="replace").splitlines(), 1):
                if name == "claims-lint":
                    if re.match(r"^#{1,6}\s", line):
                        exempt = False
                    if line.strip() == "<!-- claims-ok: negated -->":
                        exempt = True
                    bad = not exempt and claims.search(line)
                elif name == "core-portable":
                    bad = FORBIDDEN_CORE.search(line)
                else:
                    bad = SECRET.search(line)
                if bad:
                    # Never echo matching content: it might contain a credential.
                    errors.append(f"{path}:{number}: {name} violation")
    if name == "schemas":
        schemas = {}
        for path in paths:
            if path.name.endswith(".schema.json"):
                schema = json.loads((root / path).read_text())
                validator = jsonschema.validators.validator_for(schema)
                validator.check_schema(schema)
                schemas[path.name.removesuffix(".schema.json")] = validator(
                    schema, format_checker=jsonschema.FormatChecker()
                )
        for path in paths:
            key = None
            invalid = False
            if path.as_posix().startswith("contracts/fixtures/schemas/"):
                key = path.name.split(".")[0]
                invalid = path.name.endswith(".invalid.json")
            elif path.parts[0] == "scenarios" and path.suffix in (".yaml", ".yml"):
                key = "scenario"
            elif path.parts[0] == "evidence" and path.name == "manifest.json":
                key = "evidence-manifest"
            if key is None or path.suffix not in (".json", ".yaml", ".yml"):
                continue
            if key not in schemas:
                errors.append(f"{path}: missing {key} schema")
                continue
            data = yaml.safe_load((root / path).read_text())
            valid = schemas[key].is_valid(data)
            if valid == invalid:
                errors.append(f"{path}: unexpected schema validation result")
    if name == "evidence":
        for relative in paths:
            if relative.parts[0] != "evidence" or relative.name != "manifest.json":
                continue
            manifest = root / relative
            bundle = manifest.parent
            data = json.loads(manifest.read_text())
            sha = data.get("sha", "")
            if not re.fullmatch(r"[0-9a-f]{40}", sha):
                errors.append(f"{bundle.name}: invalid commit SHA")
            elif not known_commit(root, sha):
                errors.append(f"{bundle.name}: SHA is not an ancestor of HEAD or a PR head")
            files = [root / p for p in paths if p.is_relative_to(relative.parent)]
            if sum(p.stat().st_size for p in files) > 1024 * 1024:
                errors.append(f"{bundle.name}: bundle exceeds 1 MiB")
            if {p.name for p in files if p.name != "manifest.json"} != set(data.get("files", [])):
                errors.append(f"{bundle.name}: unlisted evidence files")
            for path in files:
                try:
                    path.read_bytes().decode("utf-8")
                except UnicodeDecodeError:
                    errors.append(f"{bundle.name}: non-text evidence")
            for filename in data.get("files", []):
                if (
                    filename
                    not in {
                        "transcript.txt",
                        "uname.txt",
                        "pidin.txt",
                        "binaries.txt",
                        "slog2.txt",
                        "results.json",
                    }
                    or not (bundle / filename).is_file()
                ):
                    errors.append(f"{bundle.name}: missing or invalid manifest file")
    return errors


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("check", choices=(*CHECKS, "all"))
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args()
    failures = []
    for name in CHECKS if args.check == "all" else (args.check,):
        try:
            found = check(args.root, name)
        except (ValueError, OSError, jsonschema.SchemaError, yaml.YAMLError) as error:
            found = [f"{name}: invalid input ({type(error).__name__})"]
        print(f"{name}: {'FAIL' if found else 'PASS'}")
        failures.extend(found)
    print("\n".join(failures))
    raise SystemExit(bool(failures))
