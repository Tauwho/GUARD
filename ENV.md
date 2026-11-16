# Environment (no secrets anywhere)

| Variable / setting | Default | Used by | Notes |
|---|---|---|---|
| `QNX_SDP` | `~/qnx800` | B, P (target only) | Source `$QNX_SDP/qnxsdp-env.sh` before any `qnx-x86_64` build. Never in CI (licence) |
| `GUARD_TARGET` | `guard-target` | B, P | ssh alias: `HostName 127.0.0.1`, `Port 2222`, `User root`, `IdentityFile ~/.ssh/guard_target_ed25519`, `StrictHostKeyChecking no`, `UserKnownHostsFile /dev/null` (each image rebuild changes the host key) |
| `GUARD_NET` | `user` | B, P | `image/run-demo.sh --net user` (no sudo) or `bridge` (needs `br0`) |
| `GUARD_BRIDGE` | `127.0.0.1:7700` | A (relay), P | C8 telemetry; forward the target port to localhost first |
| `GUARD_ADVISOR_PORT` | `7701` | A, B (STRETCH) | C9 |
| `GUARD_ADVISOR_MODE` | `recorded` | A, P | `recorded` (default, deterministic) or `ollama` |
| `OLLAMA_MODEL` | `qwen2.5:1.5b-instruct` | A | D23. Never 3B (non-commercial licence) |
| `GUARD_SEEDS` | `50` (CI: `50` on Linux) | A | `eval/run_matrix.py --seeds` |
| `GUARD_OUT` | `out/` | all | Git-ignored output root |
| Python | 3.10+ local, 3.12 CI | all | `pip install PyYAML==6.0.3 jsonschema==4.26.0 websockets==17.3 pytest==9.0.2 ruff==0.14.6` |
| CMake | >= 3.24 | A, B, P | Ubuntu 22.04: `python3 -m pip install --user "cmake>=3.24"` |
| Compilers | GCC or Clang (host), `qcc` gcc 12.2 (target) | A, B | |

Accounts: GitHub only (each person their own). QNX licence: per person (B, P). No API keys, no paid services ($0 budget).
