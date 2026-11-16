<!-- managed-by: guard-bootstrap v1 -->
# Contributing to GUARD

## Setup
Follow `docs/setup.md` (written in issue B-1).

1. Get your own free QNX non-commercial licence and install SDP 8.0.
2. Boot the QNX quick-start target image (QSTI) on QEMU.
3. Commit your setup evidence bundle.

## Workflow
1. Pick an issue that is assigned to you and labelled `status:ready`, then comment `START:` (AGENTS.md §4).
2. Branch `<stream>/<issue#>-<slug>`, e.g. `a/42-gate-fsm`.
3. Use small commits in Conventional Commit style (`feat(core): …`, `fix(qnx): …`, `docs: …`).
4. Open a PR of ≤ 400 changed lines (generated files excluded) using the template. It closes the issue (`Closes #N`).
5. CI must be green. It is host-only and never touches the QNX SDP.
6. **Evidence:** QNX-path PRs (`core/`, `qnx/`, `image/`, `cmake/`, `contracts/`) attach a bundle per `evidence/README.md`. Until then, add `status:needs-evidence`.
7. **Merge when finished:** reviews are optional (D12). When CI is green and every conversation is resolved, the author merges. If you want a second pair of eyes, ask on the PR with an @-mention.
8. Squash-merge. The branch is deleted automatically.

## Contract changes (after `contracts-v1.0`)
1. Open a `decision` issue.
2. Write the ADR.
3. Open a PR labelled `contract-change` that bumps the version in `contracts/CHANGELOG.md`.
4. @-mention **both** @phatdat220707 and @baofrovn on the PR before merging, so both streams see the change. No approval is required (D12).

## Decisions
ADRs live in `docs/adr/NNNN-slug.md` and are indexed in `DECISIONS.md`. The PO accepts or rejects them.

## Definition of done (every issue)
- [ ] Acceptance criteria met
- [ ] PR merged with green CI
- [ ] Target evidence attached (QNX path)
- [ ] Docs and DECISIONS.md updated if behaviour or a decision changed
- [ ] `HANDOFF:` comment posted

## Freezes (SGT)
| Freeze | Date |
|---|---|
| Contracts | Fri 20 Nov 23:59 |
| Scenarios | Sat 28 Nov 23:59 |
| P0 complete | Wed 9 Dec 23:59 |
| Code | Thu 10 Dec 23:59 |
| Video | Sat 12 Dec 23:59 |
| Submission filed | Sun 13 Dec 23:59 |
| Hard stop | Tue 15 Dec 18:00 |
| Deadline | Wed 16 Dec 03:00 |

## Emergency rule
Reviews are optional (D12), so nobody waits for an unreachable reviewer, and nobody needs admin rights to merge.
