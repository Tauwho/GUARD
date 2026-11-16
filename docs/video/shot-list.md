# GUARD demo video: shot list

Recorded from tag `v1.0-rc1` following [demo/DEMO.md](../../demo/DEMO.md). Scene numbers
refer to [storyboard.md](storyboard.md). Two takes of each shot.

## Before recording

- `git checkout v1.0-rc1`; run the DEMO "Setup" block.
- Close every other window; hide desktop icons, notifications and personal folders.
- Screen 1920 x 1080 at 100 % scaling. Terminal: monospace 18 pt, dark background,
  prompt shortened to `$ `.
- Browser for the dashboard: no tabs, bookmarks or extensions visible.

## Shots

| Shot | Scene | DEMO step | Content | Status |
|---|---|---|---|---|
| S1 | 1, 3 | 2 | Dashboard replay of `out/F3-B2.jsonl`, full run, robot visible from Ward A | ready |
| S2 | 2 | 1 | Host terminal: both F3 replays, exit codes visible | ready |
| S3 | 4 | 3 | Dashboard replay of `out/F6-B2.jsonl` with `--map scenarios/maps/mri_suite.yaml` (robot waits, reason journal) | ready |
| S4 | 4 | 3 | Dashboard replay of `out/F6-B0.jsonl` with the same `--map` (robot crosses the zone) | ready |
| S5 | 5 | 4 | Target terminal: the `run-demo.sh` start log (QEMU runs headless), then the four `ssh ... guardctl` lines with their filtered output | planned (B5) |
| S6 | 5 | 4 | Live dashboard following the target bridge during S5; speed and lease-time readouts cropped (no QNX numbers) | planned (B5) |
| S7 | 6 | 5 | `build_readme.py --check` PASS, then README Results and Limits scrolled on GitHub (DEMO step 5) | ready |
| S8 | - | - | 30 s cut of S1 (kill, lease drains, stop, latch) for social posts | ready |

## Layout

- Host scenes: terminal on the left, dashboard on the right.
- Target scene: target ssh terminal on the left (QEMU has no window: `-display none`), dashboard on the right.
- Crop to these windows only; no host OS chrome.

## Recording tool

OBS Studio, canvas and output 1920 x 1080 at 30 fps, MKV (remux to MP4), desktop audio
off, microphone on its own track. Editing in Kdenlive (fallback Shotcut). Export 1080p
MP4 (H.264 + AAC), under 3:00.
