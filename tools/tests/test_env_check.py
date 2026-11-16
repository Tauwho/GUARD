import importlib.util
from pathlib import Path

import pytest

SPEC = importlib.util.spec_from_file_location(
    "env_check", Path(__file__).resolve().parents[1] / "env-check.py"
)
env_check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(env_check)

REAL_HOST = env_check.Host
TOOLS = {"qcc", "cmake", "ninja", "mkqnximage", "qemu-system-x86_64", "ssh", "scp"}
HELPER = "/usr/lib/qemu/qemu-bridge-helper"


def make_host(**overrides):
    files = {"/sdp/host", "/sdp/target", "/dev/kvm", "/sys/class/net/br0", HELPER}
    options = dict(
        env={"QNX_HOST": "/sdp/host", "QNX_TARGET": "/sdp/target"},
        system="Linux",
        which=lambda name: f"/usr/bin/{name}" if name in TOOLS else None,
        exists=lambda p: p in files,
        access_rw=lambda p: True,
        is_setuid=lambda p: True,
        read_text=lambda p: "allow br0\n",
        run=lambda argv: "cc/12.2.0,gcc_ntox86_64\ncc/12.2.0,gcc_ntoaarch64le\n",
    )
    options.update(overrides)
    return REAL_HOST(**options)


def by_name(results):
    return {r.name: r for r in results}


def test_all_pass_on_a_complete_linux_host():
    results = env_check.run_checks(make_host())
    assert all(r.status == env_check.PASS for r in results), results


def test_build_only_skips_run_checks():
    names = {r.name for r in env_check.run_checks(make_host(), build_only=True)}
    assert names == {"sdp-env", "qcc", "cmake", "ninja"}


@pytest.mark.parametrize("env", [{}, {"QNX_HOST": "/sdp/host"}])
def test_unsourced_env_fails(env):
    assert by_name(env_check.run_checks(make_host(env=env)))["sdp-env"].status == "FAIL"


def test_env_dir_missing_fails():
    host = make_host(env={"QNX_HOST": "/gone", "QNX_TARGET": "/sdp/target"})
    assert by_name(env_check.run_checks(host))["sdp-env"].status == "FAIL"


def test_qcc_without_x86_64_target_fails():
    host = make_host(run=lambda argv: "cc/12.2.0,gcc_ntoaarch64le\n")
    assert by_name(env_check.run_checks(host))["qcc"].status == "FAIL"


def test_qcc_that_cannot_run_fails():
    host = make_host(run=lambda argv: None)
    assert by_name(env_check.run_checks(host))["qcc"].status == "FAIL"


def test_missing_mkqnximage_only_warns():
    host = make_host(which=lambda n: f"/usr/bin/{n}" if n in TOOLS - {"mkqnximage"} else None)
    results = by_name(env_check.run_checks(host))
    assert results["mkqnximage"].status == "WARN"
    assert results["qemu-system-x86_64"].status == "PASS"


def test_kvm_and_bridge_skipped_off_linux():
    results = by_name(env_check.run_checks(make_host(system="Windows")))
    assert results["kvm"].status == "WARN"
    assert results["bridge"].status == "WARN"


def test_kvm_not_writable_warns():
    host = make_host(access_rw=lambda p: False)
    assert by_name(env_check.run_checks(host))["kvm"].status == "WARN"


def test_bridge_conf_missing_fails():
    def no_file(path):
        raise FileNotFoundError(path)

    host = make_host(read_text=no_file)
    assert by_name(env_check.run_checks(host))["bridge"].status == "FAIL"


def test_bridge_named_in_conf_must_exist():
    host = make_host(read_text=lambda p: "allow virbr9\n")
    assert by_name(env_check.run_checks(host))["bridge"].status == "FAIL"


def test_bridge_helper_must_be_setuid():
    host = make_host(is_setuid=lambda p: False)
    assert by_name(env_check.run_checks(host))["bridge"].status == "FAIL"


def test_output_never_contains_env_values():
    secret = "/home/someone/qnx800-LICENCE-1234"
    host = make_host(env={"QNX_HOST": secret, "QNX_TARGET": secret}, exists=lambda p: True)
    text = " ".join(r.detail for r in env_check.run_checks(host))
    assert secret not in text


def test_main_exit_code_and_json(capsys, monkeypatch):
    monkeypatch.setattr(env_check, "Host", lambda env, system: make_host())
    assert env_check.main(["--json"]) == 0
    assert '"ok": true' in capsys.readouterr().out
    monkeypatch.setattr(env_check, "Host", lambda env, system: make_host(env={}))
    assert env_check.main([]) == 1
