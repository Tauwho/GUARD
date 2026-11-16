# GUARD demo video: captions plan

- Language: English. Burned-in captions are not used; the platform caption track is
  uploaded from `docs/video/captions.srt`, written in P4 after the voice-over is locked.
- Source text: the narration in [storyboard.md](storyboard.md), one cue per sentence,
  at most two lines of 42 characters, at least 1.2 s on screen.
- On-screen labels (not captions): scene titles from the storyboard "Caption text"
  column, white on a dark bar, bottom left, so they never cover the dashboard map.
- Reason codes appear exactly as the tools print them (`GATE_SUPERVISOR_LOST`,
  `REJ_MAP_VERSION`), in monospace.
- Every numeric caption must say "host (Linux) harness". No QNX numbers (D11).
- Music: none in the main cut. Any later track must be CC0 and named in the description.
- Check before export: total length under 3:00, captions in sync within 0.2 s, no
  word in `tools/claims-lint.txt` in any caption.
