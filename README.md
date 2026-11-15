<!-- managed-by: guard-bootstrap v1 -->
# GUARD — Gated Unexpired Authorization for Robotic Drives

**Stale decisions can't move the robot.**

GUARD is an independent runtime supervisor for a *simulated* autonomous mobile robot on QNX 8.0.
- An untrusted planner (rule-based now, an LLM later) only *proposes* motion.
- GUARD checks each proposal for provenance freshness, stopping distance and restricted zones, then grants a short, epoch-fenced lease.
- A separate gate process uses its own kernel timer to bring the robot to a stop when the lease expires. It does this even if the supervisor process has died.

> **Status:** in development for the QNX Global Hackathon 2026 (Robotics & Medical). Submission due 15 Dec 2026.

## Limits — read first
<!-- claims-ok: negated -->
- No certified functional safety. GUARD claims no compliance with ISO 13849, IEC 61508, ISO 3691-4 or IEC 62304, and no SIL or PL rating.
- No real actuators, braking or vehicles. This is a simulation-only prototype.
- No hard real-time guarantee on hardware. QEMU timing is not hardware timing.
- No formal verification. Assurance comes from unit, scenario and property tests only.
- No defence against a compromised kernel, supervisor or gate. Only the planner is treated as untrusted.
- No QNX performance claims and no QNX-vs-Linux comparisons without BlackBerry's written approval.

## Repository map
| Path | Purpose |
|---|---|
| `contracts/` | Interfaces and schemas (frozen at v1.0) |
| `core/` | Portable C++17 logic. No QNX calls |
| `qnx/` | QNX adapters. No business logic |
| `host/` | Deterministic host harness |
| `scenarios/` | Acceptance scenarios F0–F8 |
| `eval/` | Evaluation |
| `dashboard/` | Read-only viewer |
| `image/` | `mkqnximage` scripts. No images are committed |
| `evidence/` | Target evidence bundles |
| `docs/` | Architecture, ADRs, setup |

## How we work
- [AGENTS.md](AGENTS.md): rules for humans and AI agents
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [DECISIONS.md](DECISIONS.md)
- [docs/architecture.md](docs/architecture.md)

## AI-assistance disclosure
Three people build this project with AI coding assistants. A human code owner reviews and merges every change. The final README will describe how assistants were used.

## Licence
MIT. The QNX SDP is not included or redistributed. Each developer installs it under their own licence.
