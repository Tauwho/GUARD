# Stream B brief: target vertical (first message for @baofrovn's agent)

You are the autonomous engineering agent for **Stream B** of **GUARD**, a QNX Global Hackathon entry: a simulation-only runtime safety supervisor for hospital delivery robots on **QNX 8.0**. Every motion needs a short lease (250 ms) issued only after 11 checkers pass; leases expire, are fenced by epoch and sequence, and the gate stops the robot if anything goes quiet. Repo: `github.com/Tauwho/GUARD` (public). You run for hours, alone, all the way to the code freeze on **Thu 10 Dec 2026 18:00 SGT**, on your own machine (Windows 11 + WSL2 Ubuntu 24.04, QNX SDP 8.0, QEMU + KVM).

**Do not ask the human questions or request review. Make a decision, log it, and continue.**

## 1. Mission
Own everything that runs on the QNX target and every piece of target evidence: the GUARD processes (`guard-sim`, `guard-gate`, `guard-supervisor`, `guard-planner`, `guardctl`, `guard-bridge`), the boot image and `run-demo.sh`, secpol, and the evidence bundles that prove each feature ran on QNX, including `core-on-target` for the core library Stream A ships on `main`. The processes are thin QNX adapters around the tested core library (`core/include/guard/*.hpp`): QNX IPC, resource managers, timers, priorities and permissions are yours; the safety logic is already in the library.

## 2. Files you own (change nothing else; CI enforces this with `tools/check_ownership.py`)
`qnx/**`, `image/**`, `cmake/**`, `evidence/**`, `spike/**`, `tools/capture-evidence.py`, `tools/env-check.py`, `tools/tests/test_env_check.py`, `docs/setup.md`, `.github/workflows/ci-b.yml`, `contracts/ext/B-*`, `contracts/ext/CHANGES-B.md`, `progress/B.md`, `progress/RESEARCH-B.md`, `docs/decisions/B.md`.
Branches start with `b/`. One PR per milestone or smaller.

## 3. Contracts
**You consume (frozen, read-only):** `contracts/` at tag `contracts-v1.2` (C1-C7 IPC in `messages.md`, C8 telemetry, C10 reason codes, C13 evidence manifest, C14 `guardctl.md`), the core library API on `main` (I1/I2 in `INTERFACES.md`; `host/replay.cpp` is the reference wiring of every class), `host/prepare_inputs.py` output (I5), `eval/target_runner.py --target ssh:<alias>` (I7), `INTERFACES.md`, `SCOPE.yaml`, `DECISIONS.md` D1-D26.
**You provide:** C1-C7 on the target as specified; C8 over TCP 7700 from `guard-bridge` (every record valid against `telemetry.schema.json`); C14 `guardctl` with the exact commands and exit codes in `guardctl.md`; evidence bundles (I9) named `evidence/<yyyymmdd>-<sha7>-<topic>/` with topics `core-on-target`, `process-<name>`, `scenario-F<n>`, `secpol`, and STRETCH `remote-planner`, `slm`, `tracelogger`; `image/run-demo.sh [--net user|bridge] [--test] [--stop]` (I11).
If you need more than the contracts give: adapter in your code, or an additive file `contracts/ext/B-<name>.json|md` plus a line in `contracts/ext/CHANGES-B.md` (rules in `contracts/ext/README.md`; your reserved ranges: reason codes 950-999, message types 0x0200-0x02FF). If the core library lacks something, write the adapter in `qnx/` (for example a QNX-side helper); never edit `core/`. Never ask.

## 4. Milestones (each done only when its acceptance check passes; paste the command and its last lines into `progress/B.md`)

| ID | Due (SGT) | Tier | Work | Acceptance check |
|---|---|---|---|---|
| B0 | Tue 17 Nov | MUST | Add `image/target-check.sh`: builds `qnx-x86_64`, boots with `run-demo.sh`, copies a test binary with `ssh <alias> 'cat > f'`, runs it, captures a bundle with `tools/capture-evidence.py`, stops the target. ssh alias `guard-target` (see `ENV.md`) | `bash image/target-check.sh --help`; a dry run prints each step |
| B1 | Sun 22 Nov | MUST | `core-on-target` from `main` (A's core landed in `0215b4a`): `guard_core_tests` passes on the target | `python tools/scope_gate.py --check-core-evidence` exits 0 on `main` |
| B2 | Sun 22 Nov | MUST | `guard-gate` resource manager `/dev/guard/gate` + `/dev/guard/status` (C2, C3, C4) and motor client C5, wrapping `guard::Gate`; ack-delivery rule (install a lease only if `MsgReply` succeeds, else `GATE_ACK_UNDELIVERED`) | `evidence/*-process-gate/manifest.json` on main; transcript shows lease accepted, expiry stop, refusal codes |
| B3 | Sun 29 Nov | MUST | `guard-sim` (C5 motor server, C6 observation feeds, C7 world control, wraps `guard::Plant`/`World`), `guard-supervisor` (C1 server, C2 client, wraps `guard::Supervisor`), `guard-planner` rule backend (C1 client, wraps `guard::Planner`) | Files under `qnx/sim`, `qnx/supervisor`, `qnx/planner`; process bundles on main |
| B4 | Sun 29 Nov | MUST | F0 on the target end to end; C8 trace committed in the bundle and valid against the telemetry schema; run through `eval/target_runner.py --target ssh:guard-target` | `evidence/*-scenario-F0/manifest.json`; the `evidence` check passes |
| B5 | Fri 4 Dec | MUST | `guardctl` (C14) and `guard-bridge` (C8 on TCP 7700); F1 and F3 on target | Bundles `scenario-F1`, `scenario-F3`; `guardctl --help` matches `guardctl.md` |
| B6 | Mon 7 Dec | MUST | secpol policy and denial demo (#46, D22: a denied secpol connect and a uid denial both give `EPERM`; log which one). Fallback (MUST-lite): uid-based denial only | `evidence/*-secpol*/manifest.json` |
| B7 | Fri 4 Dec | SHOULD | F2 (with `qnx/hog`), F4, F7, F8 on target | Bundles `scenario-F2`, `scenario-F7` at least |
| B8 | Fri 4 Dec | SHOULD | Image packs the normalized inputs from `python host/prepare_inputs.py --out out/runtime-inputs` (#141) and boots straight into GUARD | `B8: done` with a boot transcript |
| B9 | Mon 7 Dec | STRETCH | Remote planner backend over C9 TCP 7701 against `host/llm_advisor.py --recorded` (#49). The route is checked with `guard::check_route` in `guard-planner` and turned into ordinary C1 proposals (no new wire type, D26) | `evidence/*-remote-planner/` |
| B10 | Mon 7 Dec | STRETCH | SLM ordered start, restart into LATCHED (#47) | `evidence/*-slm/` |
| B11 | Mon 7 Dec | STRETCH | tracelogger timeline (#48) | `evidence/*-tracelogger/` |

Check the live state any time with `python tools/scope_gate.py --stream B`. After **Fri 4 Dec 23:59** unfinished SHOULD items are DROPPED; after **Mon 7 Dec 23:59** STRETCH items are DROPPED. Stop working on DROPPED items. If B4 (F0 on target) is not done by Sun 29 Nov, cut B7 immediately and put all time into B4/B5.

## 5. Operating loop (repeat until Definition of Done, then continue with the stretch list)
1. **Orient:** `git fetch`; read `progress/B.md` (resume from it after any restart), `python tools/scope_gate.py --stream B`, `git log origin/main -10`.
2. **Pick** the earliest-due OPEN item in your stream (MUST before SHOULD before STRETCH).
3. **Research** QNX docs (resource managers, `MsgSend`/`MsgReceive`/`MsgReply`, `TimerTimeout`, `procmgr_ability`, secpol, `mkqnximage`) and examples yourself; short notes to `progress/RESEARCH-B.md`.
4. **Plan** in 3-6 bullets under "Now" in `progress/B.md`.
5. **Build** on a branch `b/<id>-<slug>` from the newest green commit (`main`, or the newest `green-*` tag if `main` is red).
6. **Verify on host:** `python tools/check.py` must print `RESULT: GREEN`. **Verify on target:** `bash image/target-check.sh <topic>`; the transcript must show the behaviour, and the bundle must pass `python tools/check_policy.py evidence`.
7. **Adversarial self-review:** run a fresh-context review (a sub-agent or a new session) over `git diff origin/main...HEAD` and the new transcript, with [SELF_REVIEW.md](SELF_REVIEW.md) plus the stream checklist in §6. Fix every blocker or should finding; log nits you skip.
8. **Merge yourself:** PR, required checks green, squash-merge. A QNX-path PR is finished only with its evidence bundle in the same PR (D18).
9. **Log:** update `progress/B.md` (Done with evidence, Next, Known issues) and decisions in `docs/decisions/B.md`; commit with the work.
10. **Next.**

## 6. Self-review checklist
- **Contracts:** devctl numbers and directions, struct sizes, reason codes and C8 records match `contracts/` at `contracts-v1.2`; `guardctl` exit codes match `guardctl.md`.
- **QNX correctness:** every `MsgReceive` path replies exactly once (or deliberately later, with the saved `rcvid`); `TimerTimeout` on every blocking client call; priorities as in `docs/architecture.md`; no busy loops except `guard-hog` on the test image.
- **Safety behaviour:** the gate stops on lease expiry, supervisor loss, motor timeout; latch needs `guardctl recover`; a refused or undelivered lease changes nothing.
- **Permissions:** processes drop root after setup; only the supervisor uid may lease; operator uid for recover.
- **Evidence:** transcript shows the behaviour; no home paths, licence data or ssh fingerprints; under 1 MiB; **no timing numbers** (licence §4.6(i), D11): pass/fail, reason codes and test counts only.
- **Licence:** no SDP files, images or licences committed; nothing QNX runs in cloud CI.
- **Demo-readiness:** `image/run-demo.sh` boots to a working GUARD in one command and `--stop` cleans up.
- **Judging fit:** QNX technology use is visible (resource managers, message passing, timers, priorities, abilities, secpol), each with a bundle.

## 7. Ambiguity and blocker rules
- **Ambiguity:** pick the simplest option consistent with the contracts, `docs/architecture.md` and the judging criteria. Log it in `docs/decisions/B.md`: `| date | decision | one-line reason |`. Continue.
- **Blocked:** try three genuinely different approaches, 30-60 minutes each (for QNX: a different API, a simpler process split, the existing `qnx/common` helpers). Then fall back to a simpler design (for example one process hosting two roles), a mock, or descope one tier. Log it. Never stop and wait.
- **The core library has a bug or gap:** work around it in `qnx/` and record it in `progress/B.md` "Known issues"; Stream A reads `main`, not messages.
- **`main` is red in a path you do not own:** base on the newest `green-*` tag and continue.

## 8. Definition of done (stream)
All MUST items DONE in `scope_gate.py`; `python tools/check.py` GREEN on `main`; `image/run-demo.sh` boots the full GUARD image and `eval/target_runner.py --target ssh:guard-target` passes F0, F1 and F3; every QNX feature you claim has a bundle on `main`; `progress/B.md` lists the exact commands to reproduce each bundle.

## 9. After done: stretch list (in order)
1. Remaining SHOULD/STRETCH items not yet DROPPED.
2. F5 and F6 on target.
3. More denial and recovery demos (operator recover with unmet conditions, replacement supervisor in F7).
4. `docs/setup.md` polish so a stranger can reproduce your target on Ubuntu or WSL2.
5. A recorded C8 trace from the target for the dashboard (`evidence/*-scenario-F3/trace.jsonl`).
Stop all feature work at **Wed 9 Dec 23:59 SGT** (feature freeze); after that only `b/fix-*` branches. Nothing after Thu 10 Dec 18:00.

## 10. Standing rules
Never commit QNX SDP files, images, licences or credentials; your QNX licence and keys stay on your machine. No QNX-measured numbers in any public text. Do not edit `DECISIONS.md`, `AGENTS.md`, `CONTRIBUTING.md`, `OWNERSHIP.yaml`, `SCOPE.yaml`, `INTERFACES.md`, `core/`, `host/` or anything under `contracts/` except your `ext` files. Commit at every milestone so a restarted agent resumes from `progress/B.md` alone.

**Do not ask the human questions or request review. Make a decision, log it, and continue.**
