# qnx/hog (B-10, #44)

`guard-hog <prio> <ms> [-c <cpu>]` spins at `SCHED_FIFO` `<prio>` (1..63) for `<ms>` milliseconds, optionally pinned to one CPU. Fault F2 runs it at priority 32 for 400 ms (docs/architecture.md, REF-SCENARIOS). **Test image only.**

Exit codes: 0 done, 2 bad arguments, 1 other errors (message on stderr).

## Build and test (QNX host, SDP env sourced)

```sh
cmake -S qnx/hog -B build/qnx-hog -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/qnx-x86_64.cmake
cmake --build build/qnx-hog
scp build/qnx-hog/guard-hog build/qnx-hog/guard_hog_tests root@10.10.10.2:/tmp/
ssh root@10.10.10.2 /tmp/guard_hog_tests     # as root; expect Status: SUCCESS!
```

The test pins a priority-10 counter and the hog to CPU 0 and checks the counter does not move while the hog runs. It prints no timing numbers (AGENTS.md section 2). Reproducing F2 end to end needs the gate and supervisor (#39, #40).
