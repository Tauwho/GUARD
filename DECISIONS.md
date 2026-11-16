<!-- managed-by: guard-bootstrap v1 -->
# DECISIONS

## Decision log
| ID | Date | Decision |
|---|---|---|
| D1 | 2026-11-15 | Team: 3 people, SGT, Windows 11 + WSL2 or Ubuntu hosts, about 10 h/week each outside exams (assumption) |
| D2 | 2026-11-15 | Target: QSTI x86_64 on QEMU only. No Raspberry Pi 5 |
| D3 | 2026-11-15 | Repository: Tauwho/GUARD. PO has admin |
| D4 | 2026-11-15 | Eligibility confirmed by all members. PO is the Devpost Representative |
| D5 | 2026-11-15 | Public metrics are functional only until BlackBerry gives written approval (licence §4.6(i)). Superseded by D11 |
| D6 | 2026-11-15 | Planner: rule-based + fault modes in P0. Local LLM route advisor (Ollama) in P1 |
| D7 | 2026-11-15 | Framing: hospital logistics |
| D8 | 2026-11-15 | Schedule: P0 complete Wed 9 Dec (the exam window ends 4 Dec). Code freeze Thu 10 Dec. File Sun 13 Dec |
| D9 | 2026-11-15 | Each CODEOWNERS path has a primary and a secondary owner, so authors never need to self-approve. The approval part is replaced by D12 |
| D10 | 2026-11-15 | Anyone who needs the PO or a teammate asks in the relevant issue or PR with an @-mention. No DMs or private messages for project work (AGENTS.md hard rule 5) |
| D11 | 2026-11-16 | Supersedes D5 under assumption A5 (#2, #62): publish nothing measured on QNX (no timing numbers, no QNX-vs-Linux comparison, no numeric result tables from the target); numeric results come only from the host harness, labelled "host (Linux) harness" |
| D12 | 2026-11-16 | Reviews are optional: the author merges a PR when it is finished, CI is green and every conversation is resolved. Branch protection on `main` needs 0 approvals. CODEOWNERS stays for ownership and automatic review requests only; this replaces the approval part of D9 |
| D13 | 2026-11-16 | Required status checks on `main`: the 11 host CI jobs from GitHub Actions (4 builds, format, 5 policy checks, eval). Not strict, so a PR does not have to be re-synced with `main` before merging. Reviews stay optional (D12) |
| D14 | 2026-11-16 | M0 fallback (PO): PO setup (#1) is deferred until Sat 5 Dec, with the licence request held by the PO. Until then the PO works on the host path only, and target footage for reviews comes from B's own target session (B uses only B's own licence) |
| D15 | 2026-11-16 | No weekly sync call. The team works asynchronously in issues and PRs (D10); the Sync log below is not used |
| D16 | 2026-11-16 | Contracts (#56) and scenarios (#57) were merged to `main` early so both streams can build on them. The freeze and the `contracts-v1.0` tag stay at Fri 20 Nov 18:00 SGT, or earlier if A and B both comment "no more CONTRACT-Q" on #3. Until then, contract fixes are normal PRs. B's open checklist items from #56 keep the documented defaults unless B raises a `CONTRACT-Q:` |
| D17 | 2026-11-16 | CONTRACT-Q answers (#3): a lease takes effect only after its `LeaseAck` is delivered, otherwise it is discarded with `GATE_ACK_UNDELIVERED` (516); the stopping-distance check starts from the measured speed and uses obstacles inflated by `r_robot + margin`; v1 authorizes no reverse motion |
| D18 | 2026-11-16 | Under D12, a QNX-path PR (`core/`, `qnx/`, `image/`, `cmake/`, `contracts/`) is finished only when its target evidence bundle is committed (AGENTS.md hard rule 4), unless its issue exempts it. #74 merged without it, so #31 was reopened for a follow-up evidence PR |
| D19 | 2026-11-16 | M0 fallback (A): @phatdat220707's setup evidence (#15) is deferred until Sun 22 Nov. Until then A works on the host path only (core, harness, CI); A-track QNX-path PRs still need target evidence before they are finished (D18) |
| D20 | 2026-11-16 | M0 fallback (B): @baofrovn's setup evidence (#30) is deferred until Sun 22 Nov. Until then the toolchain work in #31 counts as setup progress; B's QNX-path PRs still need target evidence before they are finished (D18) |
| D21 | 2026-11-16 | Evidence SHA rule under squash merges: a bundle's manifest `sha` passes the `evidence` check if it is an ancestor of HEAD **or** the head commit of any PR in this repo (CI fetches `refs/pull/*/head`). Squash merging no longer breaks bundles, and nobody rewrites a manifest after merging. Owner: @baofrovn (`tools/check_policy.py`, `.github/workflows/ci.yml`) |

## ADR index
All entries were accepted on 2026-11-15. Full write-ups go in `docs/adr/` (issue PO-5).

| ADR | Decision | Rejected alternative | Why |
|---|---|---|---|
| [0001](docs/adr/0001-separate-gate-process.md) | A separate gate process owns the only actuator path | Supervisor writes the motor directly | Fails safe when the supervisor dies; isolation can be demonstrated |
| [0002](docs/adr/0002-expiring-leases-epoch-fencing.md) | Expiring leases with epoch + sequence fencing | Heartbeat watchdog | Liveness is not correctness; leases bound authority in time; epochs stop zombie supervisors |
| [0003](docs/adr/0003-provenance-freshness.md) | Provenance freshness against a supervisor-owned observation history | Message age (ROS 2 lifespan) | A message can be new while the decision behind it is old |
| [0004](docs/adr/0004-portable-core-time-injected.md) | Portable core with time injected; QNX layer only wires IPC and timers | Logic inside QNX handlers | Deterministic host CI; AGENTS.md rule 2 |
| [0005](docs/adr/0005-native-ipc-resource-managers.md) | Native message passing, pulses and resource managers on target; TCP only to the host | POSIX mqueue or sockets everywhere | Priority inheritance and bounded IPC; Qnet is removed in 8.0 |
| [0006](docs/adr/0006-priority-map-inheritance.md) | Priority map; default priority inheritance on the proposal channel | `_NTO_CHF_FIXED_PRIORITY` | Starving the planner path makes leases lapse, which fails safe |
| [0007](docs/adr/0007-sim-on-target-virtual-time-host.md) | Plant simulator runs on the target; host harness uses virtual time | Simulator on the host | One clock domain; reproducible CI |
| [0008](docs/adr/0008-shm-ring-telemetry.md) | Telemetry through shared-memory rings; read-only bridge and dashboard | Telemetry over IPC from the critical path | The critical path never blocks |
| [0009](docs/adr/0009-no-qnx-in-ci.md) | No QNX SDP in CI; evidence-commit protocol | Self-hosted runner with QNX | Licence §4.2.6, §4.6(b), §4.6(l) |
| [0010](docs/adr/0010-binary-authorize-reject.md) | Binary authorize / reject | Clamp to a safe speed | Simpler and explainable |
| [0011](docs/adr/0011-functional-metrics-only.md) | Publish functional metrics only | Publish latency numbers | Licence §4.6(i) |
| [0012](docs/adr/0012-compile-time-checker-registry.md) | Compile-time checker registry; all checkers run; first failure is the primary reason | `dlopen` plugins; fail-fast | Determinism, explainability, bounded cost |
| [0013](docs/adr/0013-uid-authz-p0-secpol-p1.md) | uid-based authorization in P0; secpol in P1 (P0 if spike S4 passes) | secpol only | Risk that the spike fails on QSTI |
| [0014](docs/adr/0014-simulation-only.md) | Simulation only | Hardware | Licence §4.2.5; no hardware; $0 budget |
| [0015](docs/adr/0015-single-threaded-gate.md) | Single-threaded gate dispatch | Thread pool | Deterministic ordering; bounded handlers |
| [0016](docs/adr/0016-mit-licence.md) | MIT licence | Apache-2.0 | Simplest; codelab can reuse code |
| [0017](docs/adr/0017-lease-ack-on-resmgr-channel.md) | **Accepted 2026-11-16 (#89).** Gate replies `LeaseAck` in the same handler; `ETIMEDOUT` means not received; a late ack is binding | Defer the reply and `MsgError(ETIMEDOUT)` on unblock | Spike S3: resmgr channels keep a reply-blocked client waiting, so the v1.0 `ESRCH` path never happens |

## Clarification log
Questions Q1–Q8 are in issue PO-1.

| Q | Topic | Sent | Answer / assumption | Plan impact |
|---|---|---|---|---|
| Q1 | Bonus stacking | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A1: a project wins at most one Bonus prize, and no second submission to carry the codelab. | None; codelab stays P1, decided at CK-M3 (#13). |
| Q2 | QEMU as "the device" | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A2: QSTI in QEMU on x86_64 is an acceptable target device. | None; say "QNX 8.0 quick-start target image in QEMU (x86_64)". |
| Q3 | AI assistants | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A3: allowed, with disclosure. | None; keep the AI-use disclosure (#5, #8). |
| Q4 | AI Creativity scoring | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A4: judges reward visible AI. | LLM route advisor becomes P1 rank 2 at CK-M3, after secpol (#13, #61). |
| Q5 | Licence §4.6(i) benchmarks | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A5: no written approval, treated as denied. | No QNX-measured numbers in public; numeric results only from the host (Linux) harness. |
| Q6 | Licence §4.2.5 high-risk | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A6: simulation-only is not a High-Risk Application. | Wording: "simulation-only research prototype" (#5, #7, #8). |
| Q7 | Codelab PR / hosting | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A7: an unmerged qnx/codelabs PR may not count. | Submit our own GitHub Pages copy; qnx/codelabs PR optional (#10, #50). |
| Q8 | QNX marks in the video | ≤ 16 Nov 2026 | Organisers unable to answer; assumption A8: no permission to use QNX logos. | No QNX logo; "QNX" as plain text only (#7). |

## Spike log
| Spike | Question | Owner | Due | Result | Decision |
|---|---|---|---|---|---|
| S1 | QSTI boot + `qcc` hello on each host | all | 18 Nov | — | — |
| S2 | CMake QNX toolchain + core tests on target | B | 19 Nov | Pass. `qnx-x86_64` preset builds `guard_core_tests` with SDP 8.0 after setting `DOCTEST_CONFIG_USE_STD_HEADERS` (libc++ `std::__2`). Same commit passes on host and QSTI: 1/1 test cases, 1/1 assertions. Evidence `evidence/20261116-161b1e8-core-on-target/` | Keep the CMake toolchain (`cmake/qnx-x86_64.cmake`); no `qcc` Makefile fallback needed (#31, #80) |
| S3 | resmgr delayed reply, OCB close on death, mutex priority inheritance; delayed `LeaseAck` (gate waits 10 ms before `MsgReply`, supervisor `TimerTimeout` 5 ms: expect `ETIMEDOUT`, `MsgReply` fails with `ESRCH`, lease not installed, `GATE_ACK_UNDELIVERED`) | B | 21 Nov | `__DIOTF` round trip: yes. Late `MsgReply` on a saved `rcvid`: yes. OCB close after `kill -9`: yes, but a reply-blocked client only dies once the server answers its unblock pulse; with `iofunc_unblock_default` that waits for the late reply. Default mutex protocol is `PRIO_INHERIT` and boosts the holder (10 → 30). One channel serves pulses and `devctl`s by priority, FIFO within a priority. Delayed `LeaseAck`: the client does **not** get `ETIMEDOUT` on its own (resmgr channels use `_NTO_CHF_UNBLOCK`); by default it waits and gets the late reply, and with an unblock handler it gets whatever `MsgError` the gate sends. `MsgReply` never fails with `ESRCH` in either case. Evidence `spike/s3:evidence/20261116-6e0eb27-spike-s3/` | Keep resmgr (ADR 0005/0006). `qnx/common` (#37) must answer unblock pulses for saved `rcvid`s. messages.md 3.2 step 4 (`ESRCH` path) does not match QNX; needs a contract change, owner to raise |
| S4 | secpol denial errno; abilities across `setuid` | B | 22 Nov | — | — |
| S5 | Instrumented `procnto` for tracelogger | B | 7 Dec | Yes. QSTI boots `procnto-smp-instr`; a 2 s `tracelogger -s 2` run decodes to `KER_CALL`, `KER_EXIT`, `THREAD`, `INT` and `PROCESS` events. The default image has no `traceprinter` (copied from the SDP), and `/tmp` is the 64 MiB `/data` partition, so traces must stay short or go elsewhere. Evidence `spike/s5:evidence/20261116-ae4410c-spike-s5/` | B-12c (#48) is GO: ship `traceprinter` in the image or decode on the host |
| S6 | Host ↔ target TCP 7700 | B | 19 Nov | Yes. Target `10.10.10.2` on bridge `br0` sent 1000 JSON lines to host `10.10.10.1:7700`; all 1000 arrived in order. `nc` is not on the default image, so `spike/s6/s6.c` (needs `-lsocket`) sent them. Evidence `spike/s6:evidence/20261116-a915ee1-spike-s6/` | Bridge streams C8 over TCP 7700; no file + `scp` fallback needed |
| S7 | Ollama Qwen2.5-3B latency | A | 7 Dec | — | — |
| S8 | `mkqnximage` snippet with GUARD binaries | B | 26 Nov | — | — |

## Contract log
| Version | Date | Change | Approved by |
|---|---|---|---|
| v1.0 | 2026-11-16 | Initial freeze at 5aac574 (tag `contracts-v1.0`); CONTRACT-Q answers D17; B's checklist defaults kept (D16) | -- (reviews optional, D12) |
| v1.1 | 2026-11-16 | C2 LeaseAck delivery on a resmgr channel (ADR 0017, #89); no wire-format change | -- (reviews optional, D12) |

## Sync log
Not used: there is no weekly sync call (D15).

| Date | Decisions | Owner |
|---|---|---|
