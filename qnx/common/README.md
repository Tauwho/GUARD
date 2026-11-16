# qnx/common (B-3, #37)

Thin QNX 8.0 wrappers that the GUARD processes share. No GUARD decisions live here (AGENTS.md rule 2). Every call returns `EOK` or an errno.

| Header | Wraps |
|---|---|
| `guard/qnx/sched.hpp` | `SCHED_FIFO` at priority 1..63 |
| `guard/qnx/ipc.hpp` | `name_attach` / `name_open`, `MsgSend` bounded by `TimerTimeout` (`SIGEV_UNBLOCK`), side-channel self connect |
| `guard/qnx/timer.hpp` | `timer_create` + `SIGEV_PULSE_INIT`, periodic and one-shot |

On a channel with `_NTO_CHF_UNBLOCK` (resmgr and `name_attach` channels), a `TimerTimeout` that fires while the client is reply-blocked only ends when the server answers the unblock pulse (spike S3, ADR 0017). `send_bounded` documents this; the test `reply-blocked timeout ends via the server's unblock answer` shows it.

## Build and test (QNX host, SDP env sourced)

```sh
cmake -S qnx/common -B build/qnx-common -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake --build build/qnx-common
scp build/qnx-common/guard_qnx_common_tests root@10.10.10.2:/tmp/
ssh root@10.10.10.2 /tmp/guard_qnx_common_tests     # as root: SCHED_FIFO tests; expect Status: SUCCESS!
```

Host CI does not build this directory: it needs the SDP (ADR 0009).
