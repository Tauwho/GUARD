# GUARD — Gated Unexpired Authorization for Robotic Drives

Submission draft (P6). Each statement names its source on `main` in brackets, or says
`planned (<SCOPE id>)` when the work is not built yet. P refreshes this file at P4
(13 Dec) from `v1.0-rc1`; `python tools/check_submission.py` checks its shape. A person
on the team does the upload, not an agent.

- **Tagline:** Stale decisions can't move a simulated robot.
- **Category:** Robotics & Medical
- **Repo:** https://github.com/Tauwho/GUARD
- **Video:** planned (P4) - under 3:00, recorded from `v1.0-rc1` per `demo/DEMO.md`
- **Built with:** qnx, c++, cmake, python, qemu, github-actions

## Inspiration

AI planners are getting good at choosing a path. The hard part is knowing whether the
world they planned for still exists. In a hospital corridor a nurse can step out, or
an MRI zone can switch on, between the moment a plan is made and the moment the
wheels turn. A heartbeat tells you the planner is alive. It does not tell you its
decision is still valid.

## What it does

GUARD is a runtime supervisor design for a simulated hospital delivery robot. An
untrusted planner only proposes motion; a supervisor checks each proposal against its
own view of the world and grants a short, epoch-fenced lease; a separate gate stops
the robot when the lease runs out (`docs/architecture.md`).

The checks, the lease rules and the gate state machine are a portable C++17 core.
It runs under a deterministic host harness (`core/`, `host/guard-replay`) and inside
the QNX 8.0 processes that wire it to message passing, timers and resource managers
(gate, supervisor, planner, simulator, bridge, guardctl; `qnx/`). The target runs in
QEMU (x86_64), not on hardware.

GUARD is a simulation-only research prototype. It is not a safety product, not a
medical device, and not for real robots or patients (`docs/limits.md`).

## How it works

1. The planner proposes a speed and turn rate and names the observation and map
   version it used (`docs/architecture.md` REF-DATAMODEL).
2. The supervisor runs eleven checkers against its own observation history:
   provenance and freshness, map version, sensor health, speed envelope, restricted
   zones and stopping distance (REF-CHECKERS). A rejection changes nothing and
   carries a reason code.
3. An accepted proposal becomes a lease with an id, an epoch and a deadline
   (`T_lease` default 250 ms, `docs/parameters.md`).
4. On lease expiry or revocation the gate stops the robot; a lease from an old epoch
   is refused with `GATE_EPOCH_STALE` and changes nothing. For some causes the gate
   latches until an operator recovers it (REF-FSM).
5. Every decision is a telemetry record; a read-only dashboard replays them
   (`dashboard/README.md`).

Diagram of the design: `docs/architecture.svg` (README "How it works").

## QNX technologies

The design assigns each job to a QNX 8.0 mechanism so the component that stops the
robot does not share a fate with the component that authorises it (`qnx/README.md`,
REF-CONTRACTS, REF-PRIORITIES):

- timers, a resource-manager skeleton and a privilege drop (`setuid`, then
  `procmgr_ability` locks): built and tested on the target in `qnx/common`
  (README Evidence Map, behaviour only);
- a CPU hog at a fixed `SCHED_FIFO` priority for fault F2: built and tested in
  `qnx/hog`;
- a boot image from `mkqnximage` with GUARD's users (uids 1101-1106) that starts the
  GUARD processes; `image/run-demo.sh` boots it in QEMU (evidence `image-demo`);
- the gate as the `/dev/guard/gate` resource manager on its own timer: lease
  expiry stops the robot, a stale epoch is refused, and after `kill -9` of the
  supervisor it stops and latches `SUPERVISOR_LOST` (evidence `process-gate`,
  `scenario-F3`);
- the supervisor on native message passing (`name_attach`, `MsgSend` /
  `MsgReceive` / `MsgReply`): it authorises a proposal, refuses replays, unknown
  observations and stale observations with reason codes, and refuses a client
  that is not the planner (evidence `process-supervisor`, `process-planner`);
- a secpol policy so the planner cannot connect to the gate (evidence `secpol`),
  SLM ordered start and restart into LATCHED (evidence `slm`), and a tracelogger
  timeline from proposal to lease expiry (evidence `tracelogger`);
- F0-F8 on the target (evidence `scenario-F0` to `scenario-F8`).

Target results are shown as behaviour only: transcripts, reason codes and test
results, never measurements (D11).

## How we built it

Three people (DECISIONS.md D1), three autonomous streams with one-way dependencies:
core and host harness (A), QNX target and image (B), and the package (P)
(DECISIONS.md D26). The portable core holds all the logic with time passed in, so
the same code can run in the host harness and on the QNX target. Interfaces were
frozen as versioned contracts first (tags `contracts-v1.0` to `contracts-v1.2`,
`contracts/CHANGELOG.md`), and the F0-F8 scenarios were merged before the core
(PR #57 before PR #71). CI runs on the host only; QNX runs happen on our own
machines and are committed as text evidence bundles (`evidence/`).

## Results

Numbers come only from the host (Linux) harness: F0-F8 under no supervisor (B0), a
planner heartbeat (B1), GUARD (B2) and four GUARD ablations. In the snapshot in the
README Results section (CI run 43907070270, commit 6220b91), GUARD's outcome matched
its written hypothesis in every scenario (0 B2 mismatches), one repeat per scenario and
configuration. A separate 50-seed host run (CI run 43887605751, 3,150 runs, linked in
`progress/A.md`) also had 0 B2 mismatches. Seeds vary staff and planner timing within
small bounds; the report says this alone does not establish statistical independence.
Target runs (F0-F8 in QEMU) are shown only as behaviour: reason codes, modes, logs
and test results in the README Evidence Map, never measurements.

## Challenges

- **Publishing results under the QNX licence.** Organisers could not answer our
  question on §4.6(i), so we treated publication of QNX-measured results as denied
  (assumption A5, DECISIONS.md Q5, D11): every number is from the Linux host
  harness and target runs are behaviour only.
- **No QNX in CI.** The SDP cannot run in hosted CI, so target runs are captured on
  our own machines as evidence bundles tied to a commit and contract version.
- **Who may move the robot.** Baselines B0 and B1 command the gate directly, but only
  the supervisor may lease; the contracts needed a pass-through path so the
  comparison stays honest (`contracts/CHANGELOG.md`).
- **Three people, little overlap.** Hand-offs were replaced by ownership rules, a
  scope gate and required checks, so each stream merges on green alone (D26).

## Accomplishments

- One portable core: its smoke test passes on the host and on the QNX target
  (evidence `core-on-target`, 1 of 1 test case, behaviour only).
- A host harness that replays every fault scenario in virtual time and checks the
  outcome against a written hypothesis, with ablation differences reported even when
  they reject our own hypothesis.
- The README's results, evidence map, quick start and limits blocks are rebuilt
  from `main` and drift-checked (`tools/build_readme.py --check`).

## What we learned

- A lease with a deadline answers a different question from a heartbeat: "is this
  decision still valid?" rather than "is someone alive?".
- Putting the stop in its own process with its own timer is easy to say and fiddly
  to specify: lease delivery and the latch rules each needed a contract change
  (`contracts/CHANGELOG.md` v1.0, v1.1).
- Writing the hypotheses before the code made the ablations useful: a difference is
  a result, not a bug.

## What's next

- The LLM route advisor (Qwen2.5-1.5B, D23) is on the host today with a recorded
  default (A6); behind the target planner over C9: planned (B9).
- Signed proposals (HMAC), so a planner cannot misstate which observation it used:
  planned (REF-SECURITY P2; no SCOPE item).

## Limits

<!-- claims-ok: negated -->
From `docs/limits.md` (the README Limits section is generated from it): no certified
functional safety, no real actuators, no hard real-time guarantee on hardware (the
target is QNX 8.0 in QEMU, and QEMU timing is not hardware timing), no formal
verification, no defence against a compromised kernel, supervisor or gate, and no
QNX performance claims.

## AI-use disclosure

We built GUARD with AI coding assistants. Assistants wrote code, tests, checks and
documents, and opened and merged pull requests. After the initial bootstrap commit,
every change reached `main` through a pull request merged by its author once the
required CI checks passed; pull requests have no required human review, and a
fresh-context agent self-review takes its place (D12, D26). The humans set the rules
and own the design decisions in `DECISIONS.md`. Numbers come from the Linux host
harness, and target bundles are transcripts of runs on our own machines. By default
the LLM advisor replays a synthetic test fixture, not model output; live model runs
stay on our own machines (`host/ADVISOR.md`). The rule-based planner is the default
demo path. The demo video's narration is a synthetic voice (offline Windows
text-to-speech); the footage is screen recordings of the real tools at `v1.0-rc1`.
