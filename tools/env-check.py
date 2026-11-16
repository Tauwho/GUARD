#!/usr/bin/env python3
"""Check a GUARD developer host for the QNX build and QEMU target tools.

Usage:
    python3 tools/env-check.py               # build + run checks
    python3 tools/env-check.py --build-only  # only the qcc toolchain
    python3 tools/env-check.py --json        # machine-readable output

Exit code 0 when no check FAILs, 1 otherwise. WARN never fails the run.

The tool prints no paths, environment values or licence data, so its output
can go into an evidence bundle as is. Stdlib only.
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

PASS, WARN, FAIL = "PASS", "WARN", "FAIL"
QCC_TARGET = "gcc_ntox86_64"
BRIDGE_CONF = "/etc/qemu/bridge.conf"


@dataclass(frozen=True)
class Result:
    name: str
    status: str
    detail: str


@dataclass
class Host:
    """Everything the checks read from the machine. Tests replace it."""

    env: dict
    system: str
    which: object = shutil.which
    exists: object = os.path.exists
    access_rw: object = lambda p: os.access(p, os.R_OK | os.W_OK)  # noqa: E731
    is_setuid: object = lambda p: bool(os.stat(p).st_mode & 0o4000)  # noqa: E731
    read_text: object = lambda p: Path(p).read_text()  # noqa: E731
    run: object = None

    def output(self, argv):
        """Return combined stdout+stderr of argv, or None if it cannot run."""
        if self.run is not None:
            return self.run(argv)
        try:
            done = subprocess.run(argv, capture_output=True, text=True, timeout=20)
        except (OSError, subprocess.SubprocessError):
            return None
        return (done.stdout or "") + (done.stderr or "")


def check_sdp_env(host):
    missing = [v for v in ("QNX_HOST", "QNX_TARGET") if not host.env.get(v)]
    if missing:
        return Result("sdp-env", FAIL, f"{', '.join(missing)} not set; source qnxsdp-env first")
    bad = [v for v in ("QNX_HOST", "QNX_TARGET") if not host.exists(host.env[v])]
    if bad:
        return Result("sdp-env", FAIL, f"{', '.join(bad)} points to a missing directory")
    return Result("sdp-env", PASS, "QNX_HOST and QNX_TARGET set")


def check_qcc(host):
    if not host.which("qcc"):
        return Result("qcc", FAIL, "qcc not on PATH; source qnxsdp-env first")
    text = host.output(["qcc", "-V"])
    if text is None:
        return Result("qcc", FAIL, "qcc found but could not run")
    if QCC_TARGET not in text:
        return Result("qcc", FAIL, f"qcc -V does not list {QCC_TARGET}")
    return Result("qcc", PASS, f"qcc lists {QCC_TARGET}")


def check_tool(name, status_if_missing, why):
    def check(host):
        if host.which(name):
            return Result(name, PASS, "on PATH")
        return Result(name, status_if_missing, f"not on PATH; {why}")

    return check


def check_kvm(host):
    if host.system != "Linux":
        return Result("kvm", WARN, f"skipped on {host.system}; QEMU runs without KVM (slow)")
    if not host.exists("/dev/kvm"):
        return Result("kvm", WARN, "/dev/kvm missing; enable virtualisation (or WSL2 nested)")
    if not host.access_rw("/dev/kvm"):
        return Result("kvm", WARN, "/dev/kvm not writable; add your user to the kvm group")
    return Result("kvm", PASS, "/dev/kvm usable")


def check_bridge(host):
    if host.system != "Linux":
        return Result("bridge", WARN, f"skipped on {host.system}; use the documented fallback")
    try:
        conf = host.read_text(BRIDGE_CONF)
    except OSError:
        return Result("bridge", FAIL, f"{BRIDGE_CONF} missing; see docs/setup.md 'Bridge'")
    bridges = [ln.split()[1] for ln in conf.splitlines() if ln.split()[:1] == ["allow"]]
    bridges = [b for b in bridges if b != "all"] or ["br0"]
    up = [b for b in bridges if host.exists(f"/sys/class/net/{b}")]
    if not up:
        return Result("bridge", FAIL, f"allowed bridge {bridges[0]} does not exist")
    helper = host.which("qemu-bridge-helper") or "/usr/lib/qemu/qemu-bridge-helper"
    if not host.exists(helper):
        return Result("bridge", FAIL, "qemu-bridge-helper not found")
    if not host.is_setuid(helper):
        return Result("bridge", FAIL, "qemu-bridge-helper is not setuid root")
    return Result("bridge", PASS, f"bridge {up[0]} allowed and present")


BUILD_CHECKS = (
    check_sdp_env,
    check_qcc,
    check_tool("cmake", FAIL, "install CMake >= 3.24"),
    check_tool("ninja", FAIL, "install Ninja"),
)
RUN_CHECKS = (
    check_tool("mkqnximage", WARN, "only needed to build your own image"),
    check_tool("qemu-system-x86_64", FAIL, "install QEMU"),
    check_kvm,
    check_bridge,
    check_tool("ssh", FAIL, "install an OpenSSH client"),
    check_tool("scp", FAIL, "install an OpenSSH client"),
)


def run_checks(host, build_only=False):
    checks = BUILD_CHECKS if build_only else BUILD_CHECKS + RUN_CHECKS
    return [check(host) for check in checks]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build-only", action="store_true", help="skip QEMU/network checks")
    parser.add_argument("--json", action="store_true", help="print JSON")
    args = parser.parse_args(argv)
    results = run_checks(Host(env=dict(os.environ), system=platform.system()), args.build_only)
    failed = any(r.status == FAIL for r in results)
    if args.json:
        print(json.dumps({"ok": not failed, "checks": [asdict(r) for r in results]}, indent=2))
    else:
        for r in results:
            print(f"{r.status:4}  {r.name:20} {r.detail}")
        print("env-check:", "FAIL" if failed else "PASS")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
