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
| D18 | 2026-11-16 | Under D12, a QNX-path PR (`core/`, `qnx/`, `image/`, `cmake/`, `contracts/`) is finished only when its target evidence bundle is committed (AGENTS.md hard rule 4), unless its issue exempts it. #74 merged without it, so #31 was reopened for a follow-up evidence PR. For `core/`, D25 moves the evidence after the merge |
| D19 | 2026-11-16 | M0 fallback (A): @phatdat220707's setup evidence (#15) is deferred until Sun 22 Nov. Until then A works on the host path only (core, harness, CI); A-track QNX-path PRs still need target evidence before they are finished (D18) |
| D20 | 2026-11-16 | M0 fallback (B): @baofrovn's setup evidence (#30) is deferred until Sun 22 Nov. Until then the toolchain work in #31 counts as setup progress; B's QNX-path PRs still need target evidence before they are finished (D18) |
| D21 | 2026-11-16 | Evidence SHA rule under squash merges: a bundle's manifest `sha` passes the `evidence` check if it is an ancestor of HEAD **or** the head commit of any PR in this repo (CI fetches `refs/pull/*/head`). Squash merging no longer breaks bundles, and nobody rewrites a manifest after merging. Owner: @baofrovn (`tools/check_policy.py`, `.github/workflows/ci.yml`) |
| D22 | 2026-11-16 | CK-M3 P1 plan decided early (#13). secpol (B-12a, #46) is **P0**, as ADR 0013 allows now that spike S4 passed (S4 row); a denied secpol connect and a uid denial both give `EPERM`, so #46 logs which one. P1 rank 1 is the LLM advisor (#27, #49), conditional GO after the host F0/F1/F3 runs pass in #22. SLM, tracelogger, Hypothesis and MkDocs only if slack remains on 7 Dec. Codelab (#10, #50) is NO-GO and moves to P2 (A1, A7). On 7 Dec any conditional item is cut if F0, F1 and F3 lack target evidence |
| D23 | 2026-11-16 | LLM advisor model (P1, #27, #29): **Qwen2.5-1.5B-Instruct** (Apache-2.0, official LICENSE on Hugging Face), not Qwen2.5-3B-Instruct, whose Qwen Research Licence is non-commercial only. The repo then has no non-commercial dependency. Weights are never committed or redistributed; each member pulls them locally (`ollama pull qwen2.5:1.5b-instruct`). If 1.5B routes are too poor, the next step up is Qwen2.5-7B-Instruct (also Apache-2.0); 3B stays out. Owner: @phatdat220707 (`host/llm_advisor.py`, `host/ADVISOR.md`) |
| D24 | 2026-11-16 | P1 route admission (C9 to supervisor), answering the `CONTRACT-Q:` on #3. (a) The supervisor keeps a **separate route-provenance ring** of `{obs_id, stamp_ns, map_version}` with fixed capacity `ceil(route_max_age_ms * obs_rate_hz / 1000)` (100 at the defaults 5000 ms and 20 Hz). The C1 64-entry history is unchanged. Route age = the supervisor's current observation stamp minus the recorded stamp of the route's `based_on.obs_id` (C9 clock rule, #3). This is internal to the supervisor, not a wire change. (b) `guard-planner` forwards a C9 route to the supervisor over C1 as a new `RouteProposal` message. That is a wire change, so it goes through a **v1.3 contract change** after `contracts-v1.2` is tagged; it is not folded into v1.2. @phatdat220707 drafts the fixed-size layout (at most 16 waypoints, fields from `planner-remote.schema.json`); the PO opens the decision issue. Until v1.3 is tagged nobody adds the wire type, and the host demo (#146) stays host-only. A route never authorizes motion: every motion still needs a C1 `Proposal` through checkers 1-11. Owners: @phatdat220707 (ring, layout draft), @baofrovn (#49 forwarding after v1.3) |
| D25 | 2026-11-16 | Core lands before its target evidence (exception to D18 for `core/` only). A `core/` or `host/` PR may merge to main once the 11 required host checks pass (D13); it does not wait for a target run. The `core-on-target` evidence for that code is a follow-up: the PO (or @baofrovn) captures it on their target with `tools/capture-evidence.py` after the core lands, within 7 days, and @phatdat220707 does not need a target for it. A's own setup evidence (#15, D19) is unchanged. `qnx/`, `image/`, `cmake/` and `contracts/` keep D18 as written. Reason: B's processes #38-#42 need A's core on main, and A has no target yet (#17) |

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
| S4 | secpol denial errno; abilities across `setuid` | B | 22 Nov | Q1 yes: a `--secpol=develop` image with `secpolgenerate` records a policy, and `--secpol=secure --policy=<it>` builds and boots QSTI. Q2: a type with no `channel_connect` rule gets `EPERM` from `name_open` (the same errno as the uid check, so logs must say which one). Q3: a `procmgr_ability` lock survives `setuid`, but re-allowing a locked ability returns `EOK` and has no effect, so check the effect, not the return code. Q4: `ConnectClientInfo` gives the client's uid for all six GUARD uids; after `seteuid` it shows ruid 0 and the new euid, so check `euid`. Evidence `spike/s4:evidence/20261116-fe2feac-spike-s4/` | Secpol in P0, as ADR 0013 allows when S4 passes. B-12a (#46) relabelled P0. Policy comes from `secpolgenerate` in develop mode, not hand-written |
| S5 | Instrumented `procnto` for tracelogger | B | 7 Dec | Yes. QSTI boots `procnto-smp-instr`; a 2 s `tracelogger -s 2` run decodes to `KER_CALL`, `KER_EXIT`, `THREAD`, `INT` and `PROCESS` events. The default image has no `traceprinter` (copied from the SDP), and `/tmp` is the 64 MiB `/data` partition, so traces must stay short or go elsewhere. Evidence `spike/s5:evidence/20261116-ae4410c-spike-s5/` | B-12c (#48) is GO: ship `traceprinter` in the image or decode on the host |
| S6 | Host ↔ target TCP 7700 | B | 19 Nov | Yes. Target `10.10.10.2` on bridge `br0` sent 1000 JSON lines to host `10.10.10.1:7700`; all 1000 arrived in order. `nc` is not on the default image, so `spike/s6/s6.c` (needs `-lsocket`) sent them. Evidence `spike/s6:evidence/20261116-a915ee1-spike-s6/` | Bridge streams C8 over TCP 7700; no file + `scp` fallback needed |
| S7 | Ollama Qwen2.5-1.5B latency (D23) | A | 7 Dec | Dat CPU measured 16 Nov: 20 valid goal-reaching samples after one warmup; actual responses exercised the D24 supervisor and World/F6. Changed-map responses reject; the cold/slow response rejects as stale. Numeric median/p95 stay internal at local `out/s7-1p5b/summary-internal.json`; other laptops pending | GO for D22 advisory prototype with recorded fallback. Live C9 TCP and exact recorded replay verified; no target evidence claimed. Reproduce with `host/benchmark_advisor.py` (#27, #29) |
| S8 | `mkqnximage` snippet with GUARD binaries | B | 26 Nov | `--extra-dirs=<dir>` with `snippets/system_files.guard` (adds files to `/system`) and `snippets/post_start.guard` (runs at the end of `post_startup.sh`) works on QSTI. The same files named `*.custom` in an extra dir are overwritten by the local placeholders. `ifs_start` runs before `/system` is mounted. SDP 8.0 macros are `START_P` / `STARTU_P(type,uid)` and only take effect with `--secpol`; `on -u <uid>:<uid>` sets a uid without it. Evidence `spike/s8:evidence/20261116-1d7c171-spike-s8/` | B-9 (#43): `--extra-dirs=image/mkqnximage` with `*.guard` snippets; processes start as root and drop privileges themselves |

## Contract log
| Version | Date | Change | Approved by |
|---|---|---|---|
| v1.0 | 2026-11-16 | Initial freeze at 5aac574 (tag `contracts-v1.0`); CONTRACT-Q answers D17; B's checklist defaults kept (D16) | -- (reviews optional, D12) |
| v1.1 | 2026-11-16 | C2 LeaseAck delivery on a resmgr channel (ADR 0017, #89); no wire-format change | -- (reviews optional, D12) |

## Sync log
Not used: there is no weekly sync call (D15).

| Date | Decisions | Owner |
|---|---|---|
