# Stream A decisions (@phatdat220707)

Decide, log one line, continue (D26). Never edit `DECISIONS.md`.

| Date | Decision | Reason |
|---|---|---|

| 2026-11-16 | Resume D26 against frozen v1.2; keep prior successful host results and reverify each milestone | No QNX setup/peer handoff needed; A owns host acceptance. |

| 2026-11-16 | Runner injects only F2/F3/F7 externally; World retains model-fault timeline | Avoid duplicate model injections; mock uses identical virtual-time scenario. |
| 2026-11-16 | F7 resume_after_ms overrides duration_ms for C14 --duration-ms | C14 has one resume-duration flag; retain explicit resume semantics. |

| 2026-11-16 | Dashboard drops malformed physical records and reports count; parser rejects browser-infinite numbers | Preserve live viewing after bad bridge input without presenting corrupt telemetry. |
| 2026-11-16 | Use Node VM with DOM/canvas stubs for headless behavior smoke | No visible browser or extra runtime package; transport tests use real localhost HTTP/TCP. |
