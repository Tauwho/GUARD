# Stream A decisions (@phatdat220707)

Decide, log one line, continue (D26). Never edit `DECISIONS.md`.

| Date | Decision | Reason |
|---|---|---|

| 2026-11-16 | Resume D26 against frozen v1.2; keep prior successful host results and reverify each milestone | No QNX setup/peer handoff needed; A owns host acceptance. |

| 2026-11-16 | CI gates execution/schema errors and B2 regressions; control/ablation differences remain visible in full reports | Controls intentionally differ; default matrix CLI retains strict mismatch exit status. |
| 2026-11-16 | Omit optional C8 min_margin_m when no geometry is measured; report adapter returns null | Frozen v1.2 permits number or absence, not null; never publish synthetic 1000 m sentinel. |
