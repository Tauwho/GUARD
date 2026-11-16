# Stream A decisions (@phatdat220707)

Decide, log one line, continue (D26). Never edit `DECISIONS.md`.

| Date | Decision | Reason |
|---|---|---|

| 2026-11-16 | Resume D26 against frozen v1.2; keep prior successful host results and reverify each milestone | No QNX setup/peer handoff needed; A owns host acceptance. |

| 2026-11-16 | CI gates execution/schema errors and B2 regressions; control/ablation differences remain visible in full reports | Controls intentionally differ; default matrix CLI retains strict mismatch exit status. |
| 2026-11-16 | Omit optional C8 min_margin_m when no geometry is measured; report adapter returns null | Frozen v1.2 permits number or absence, not null; never publish synthetic 1000 m sentinel. |
| 2026-11-16 | Runner injects only F2/F3/F7 externally; World retains model-fault timeline | Avoid duplicate model injections; mock uses identical virtual-time scenario. |
| 2026-11-16 | F7 resume_after_ms overrides duration_ms for C14 --duration-ms | C14 has one resume-duration flag; retain explicit resume semantics. |

| 2026-11-16 | Dashboard drops malformed physical records and reports count; parser rejects browser-infinite numbers | Preserve live viewing after bad bridge input without presenting corrupt telemetry. |
| 2026-11-16 | Use Node VM with DOM/canvas stubs for headless behavior smoke | No visible browser or extra runtime package; transport tests use real localhost HTTP/TCP. |
| 2026-11-16 | Scope-only Linux make adapter returns pending for recursive P package-check probe | P Makefile check -> scope -> package-check -> check recurses; all A checks still run unchanged. |

| 2026-11-16 | Remove temporary scope make adapter after P fixes f21aab4/2011b07 reach main | Use plain tools/check.py again; P-owned recursion is now fixed upstream. |

| 2026-11-16 | Recorded mode is the offline default; live fallback requires an exact request and dedicated model-availability error | Never refresh old provenance or mask recording I/O/invalid model output. |

| 2026-11-16 | A8 optional collection requires Hypothesis plus the production C++ probe; dedicated A CI requires both | P-owned baseline jobs do not install optional Hypothesis. Absence selects no tests (exit5), never fabricated property success. |
