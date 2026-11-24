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


## Re-cut #222
The accompanying SRT is retimed to retained source intervals and individually
synthesised new sentences. Continuous on-screen disclosure identifies the remote
scene as recorded target trace replay. New captions attribute REJ_MAP_VERSION to
the planner, not the supervisor. Target numeric results are absent. Video duration
is 2:51.02; duration.txt rounds up to 2:52. P reviews claims before merge/upload.
