# Stream A progress

Brief: `docs/streams/BRIEF-A.md`; frozen contracts-v1.2; D26/D27. Resume here.

## Now
- A1-A8 executable host outputs complete; A9 accepted evidence documented below under the updated PO decision.
- Full local check GREEN (out/d26-a9-close-check.txt); finish close-out CI/merge, then retain evidence. No more feature work without a concrete uncovered need.
- A9 close-out: document the accepted actual-inference plus Linux offline/recorded evidence; another laptop is optional (PO decision #29).

## Done with executable evidence

A1: done dcc8d36 (#158; lease/recheck fixes also #152 aab4d95).
- `ctest --preset host-debug`: 2/2 passed, 111 core cases.
- `python tools/check.py`: RESULT: GREEN. Refused passthrough no longer consumes cmd_seq.
- Self-review: 0 blocker, 0 should; sequence/refusal regressions reproduced before fixes.

A2: done 25fc74b (#153).
- `python host/guard-replay --scenario scenarios/F5.yaml --mode B2 --out out/F5.jsonl`: exit0, GOAL, mismatches[].
- F6 same command: exit0, READY, REJ_MAP_VERSION, mismatches[]. No B2 expectations weakened.

A3: done 82ff265 (#160).
- `python -m pytest eval/tests/test_target_runner.py -q`: 11 passed including real core F0/F3.
- Local C14 mock + bounded SSH transport; validate seed/run identity, always clean up owned run.
- Self-review: 0 blocker/should after duration, trial identity, deadline and refused-load fixes.
- Local/mock evidence only; real QNX transport execution belongs to B.

A4: done (manual remote evidence) https://github.com/Tauwho/GUARD/actions/runs/43856863508
- Main f48946b: host-evaluation GREEN; 90-day artifact; every subsequent main push reruns it.
- Main Linux full check/fresh-clone GREEN: https://github.com/Tauwho/GUARD/actions/runs/43856863542
- Optional min_margin_m omitted if unmeasured; report summary null/unavailable; frozen C8 unchanged.
- Temporary A-only recursion adapter removed after upstream P fix. Plain tools/check.py used again.

A5: done 82ba25a (#163).
- `python -m pytest eval/tests/test_dashboard_relay.py -q`: 7 passed.
- Headless Node smoke PASS: seek, lease, source arc, latch/reason feed; real localhost HTTP/TCP tests.
- Bad JSON/schema/non-finite/chunked oversized physical lines dropped/counted; later valid lines survive.
- Self-review: 0 blocker/should after physical framing and browser-infinite integer fixes.

A6: done a73a371 (#174).
- `python -m pytest eval/tests/test_recorded_advisor.py -q`: 11 passed incl offline TCP -> real core F6 map rejection.
- --recorded default, --trace compatible, explicit --ollama; exact recorded fallback only for model availability.
- Recording I/O failures and invalid model output do not fall back; original provenance/delay unchanged.
- Full check GREEN; self-review 0 blocker/should/nit after dedicated availability-error fix.

A7: done b016a04 (#177), manual Linux evidence https://github.com/Tauwho/GUARD/actions/runs/43887605751
- Artifact 14235253466, host-evaluation-338bf3cb2e1ae97bd4cf9e8bd845c7afd17507fe, retained 90 days.
- Actual 3150 executions, 0 execution/schema errors; 450/450 B2 pass. F1 B2 has 50 distinct trace bodies.
- 386 control/ablation reference differences remain visible; no statistical-independence claim.
- Same seed byte-identical; seeded staff +/-100ms and proposal cadence +0..5ms; reference/flood schedules unchanged.
- C8 exact schema dispatch remains jsonschema-based, full fallback for unknown reference shapes; 4 differential tests pass.
- `python tools/check.py`: RESULT: GREEN (out/d26-a7-final.txt); fresh-context reviews 0 outstanding blocker/should.
- Local copies: out/a7-linux-full/linux-matrix/results.json and linux-report/report.md.

## A8 acceptance
A8: done 6ac05ec (#179; actual C++ property CI https://github.com/Tauwho/GUARD/actions/runs/43890921100)
- Hypothesis 6.170.15 + actual guard-core-probe: 5 tests, 100 generated examples per property + explicit regressions.
- Valid wire bytes, nonfinite fields, arbitrary bytes, valid-header wrong lengths, ACK/refusal fencing, MOVING/expiry/braking/READY.
- Found and fixed test-driver subnormal stod issue; finite strtod regression retained.
- Self-review: 0 blocker/should/nit after removing wrong-length prefilter and requiring real movement.
- `python tools/check.py`: RESULT: GREEN before integration (out/d26-a8-check.txt); final integrated rerun RESULT: GREEN (out/d26-a8-integrated.txt).
- Optional suite only collects with real backend + built probe. Without it, -k hypothesis exits5; no property success fabricated.
- Dedicated stream-a CI pins backend, builds probe and explicitly requires -k hypothesis. Existing tests unchanged.

## Next
- Verify A9 close-out main CI and newest Linux artifact; retain evidence and stop feature churn. No additional laptop is required.

## Known limits
- A4/A7 remain MANUAL in scope_gate by design: remote evidence linked above, never inferred from progress text.
- A9 remains MANUAL in frozen scope: PO acceptance uses real inference on Dat laptop plus Linux offline/recorded CI; no second-device inference is claimed.
- Target evidence is B-owned. No QNX SDK/licences/weights or QNX performance numbers committed.
- Host reports use simulation-only functional results. Operator recovery trials still unavailable.
- No task-owned Ollama/advisor process left running; tests close their sockets/processes.


## Live-view burst follow-up
- Queue bounded live arrivals, coalesce rendering per frame, preserve pause/seek and flush on end.
- `python -m pytest eval/tests/test_dashboard_relay.py -q`: 7 passed, including burst and newer-error ordering regression.
- Headless: 1000 arrivals -> one redraw per frame; pending/history capped at 20000; paused view preserved.
- self-review: 0 blocker, 0 should, 0 nit after connection-status ordering and UTF-8 fixes.
- Full integrated `python tools/check.py`: RESULT: GREEN (out/d26-live-bursts-final.txt); no measured rendering-rate claim.


## A9 preparation (historical)
- Benchmark defaults to a committed validated task; custom C9 --request remains compatible. No private input handoff needed.
- `python -m pytest eval/tests/test_advisor_benchmark.py -q`: 2 passed.
- `python tools/check.py`: RESULT: GREEN (out/d26-a9-guide-check.txt); self-review 0 blocker/should/nit.
- host/ADVISOR.md gives second-laptop commands. The synthetic task input is not an inference result; new results still come from actual local model calls.
- At preparation time A9 still required another laptop; the PO decision below supersedes that requirement. No second-device or timing evidence fabricated.


## A9 verification close-out
- [PO acceptance](https://github.com/Tauwho/GUARD/issues/29#issuecomment-6921570197): another physical laptop is optional; frozen scope remains MANUAL.
- Actual Qwen2.5-1.5B inference on Dat laptop: one warmup + 20 schema/goal-valid responses, CPU two threads, GPU zero; live C9 TCP + replay and World/F6 changed-map/expired-source rejection. Internal evidence: out/s7-1p5b/; no timings published.
- Main Linux [eval job: success](https://github.com/Tauwho/GUARD/actions/runs/43895106219/job/133396405455) at b3c10273c1e95b0c8371ed28116bd77dba63b072: recorded advisor, timeout fallback, provenance tests. This is offline/recorded verification, not second-machine inference.
- `python tools/check.py`: all checks PASS, including host configure/build/test and pytest; `RESULT: GREEN` (out/d26-a9-close-check.txt).
- Fresh-context adversarial self-review: 0 blocker, 0 should, 0 nit; PO decision and linked Linux eval job independently verified.


## QNX SDP 8.0 decimal compatibility follow-up
- B reports main f37ece8 cannot compile BufferJson::number: SDP 8.0 lacks floating-point std::from_chars. Target evidence for the new fix remains B-owned.
- Replace floating conversion with bounded integer-significand/exponent normalization then strtod; no decimal separator, process locale mutation, allocation, or QNX APIs. Preserve integer from_chars.
- Add range, minimum-subnormal, signed-zero, huge-exponent, output-preservation and existing comma-locale regression coverage.
- Local build attempts blocked by filesystem write denial from CMake/Ninja even with escalation; no local test success claimed. Linux CI must run the complete check before merge.
- Linux validation at f8916fc: GCC/Clang debug + ASan build/test PASS; Stream A acceptance (tools/check.py, real Hypothesis and fresh-clone demo) PASS: https://github.com/Tauwho/GUARD/actions/runs/44005030555 . Local filesystem-denied results remain separate.
- Fresh-context adversarial self-review: 0 blocker, 0 should; one skipped nit requesting another comma-locale sample, since the existing locale regression already parses a fractional decimal under available comma locales.
- Next: B rebuilds merged main with SDP 8.0 and captures core-on-target evidence; the scratch strtod result does not validate this final implementation.
