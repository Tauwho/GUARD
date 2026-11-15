<!-- managed-by: guard-bootstrap v1 -->
# core/

Portable C++17 logic shared by the host harness and the QNX processes (AGENTS.md rule 2).

- No QNX-only calls: no `sys/neutrino.h`, `sys/iofunc.h`, `sys/dispatch.h`, `Msg*`, `resmgr_*` or `slog2*`.
- No clock reads and no file or network I/O. Time is always passed in.
- Deterministic for a given seed. Per-tick paths use fixed-capacity containers.
- The same doctest binary runs on the host and on the QNX target.
