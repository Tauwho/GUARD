# GUARD demo video -- storyboard v1

Status: **v1 draft** (front-loaded, issue #7). v2 on Tue 8 Dec replaces every `TBD` with what the real runs show. Final capture Fri 11 Dec on tag `v1.0-rc1`.

**Rules for every frame** (#2, #62, DECISIONS.md D11 once merged):
- Nothing measured on QNX is shown as a number: no latency, jitter, CPU or memory figures, and no QNX-vs-Linux comparison.
- Numbers appear only in the results scene, from the **host (Linux) harness**, labelled as such.
- GUARD is called a "simulation-only research prototype". It is never shown as a medical device or as a device for real robots or patients.
- No QNX logo. "QNX" appears only as plain text and in normal terminal output (boot banner, `uname`).
- Every claim must also appear in the README (draft in #64). See the claims list below.

Total length **2:50** (hard limit: under 3:00).

## 1. Scene table

| # | Time | On screen | Commands (target unless marked host) | Scenario / seed | Dashboard state | On-screen text |
|---|---|---|---|---|---|---|
| 1 | 0:00-0:10 | Hospital corridor map; robot icon at Ward A | host: `python dashboard/relay.py --replay F6-B0.jsonl` | F6 under B0, seed 1006 (host replay) | Map only, zone outline visible | "AI planners can be right about a world that no longer exists." |
| 2 | 0:10-0:25 | QEMU window booting, then target terminal | `uname -a`, then `pidin` | -- | Not shown | "QNX 8.0 quick-start target image in QEMU (x86_64)" |
| 3 | 0:25-0:45 | Animated diagram from the README | -- | -- | Not shown | "Proposal -> 11 checks -> 250 ms lease -> gate -> motor" |
| 4 | 0:45-1:10 | Dashboard live from target; inset: host replay under B1 | `guardctl scenario load /guard/scenarios/F6.yaml --seed 1006 --mode B2`, `guardctl scenario start`; host inset: `guard-replay scenarios/F6.yaml --mode B1` | F6, seed 1006 (B2 live, B1 inset) | B2: zone turns active, reason `REJ_MAP_VERSION` (#137), robot waits. Inset: B1 robot enters the zone | "Stale map -> rejected" |
| 5 | 1:10-1:30 | Dashboard + target terminal side by side | `guardctl scenario load /guard/scenarios/F3.yaml --seed 1003 --mode B2`, `guardctl scenario start`, then `guardctl inject F3`, then `guardctl status` | F3, seed 1003 | Lease bar drains; mode STOPPING -> STOPPED -> LATCHED; reason `GATE_SUPERVISOR_LOST` | "Supervisor killed. The gate stops the robot on its own timer." |
| 6 | 1:30-1:45 | Target terminal + dashboard | `guardctl scenario load /guard/scenarios/F7.yaml --seed 1007 --mode B2`, `guardctl scenario start`, `guardctl inject F7` | F7, seed 1007 | `GATE_EPOCH_CHANGED`, then `GATE_EPOCH_STALE` for the old supervisor | "Zombie supervisor: old epoch refused" |
| 7 | 1:45-2:00 | Target terminal with `pidin` priorities, dashboard | `guardctl scenario load /guard/scenarios/F2.yaml --seed 1002 --mode B2`, `guardctl scenario start`, `guardctl inject F2` | F2, seed 1002 | Leases lapse, `GATE_LEASE_EXPIRED`, STOPPED, then moving again | "Overload above the supervisor: leases lapse, robot stops" |
| 8 | 2:00-2:15 | Dashboard with `d_req` overlay | `guardctl scenario load /guard/scenarios/F5.yaml --seed 1005 --mode B2`, `guardctl scenario start` | F5, seed 1005 | `REJ_STOPPING_DISTANCE`, then a lower speed authorized near the crossing | "Too fast for the space ahead -> slower lease" |
| 9 | 2:15-2:30 | Results table from the README | -- | Host matrix, all scenarios | Not shown | "Results: host (Linux) harness" + table `TBD (#23)` |
| 10 | 2:30-2:45 | Text slide: why QNX, then the Limits | -- | -- | Not shown | "Why QNX: message passing, pulses, resource managers, priorities" / "Simulation-only research prototype. Not for real robots or patients." |
| 11 | 2:45-2:50 | End card | -- | -- | Not shown | Repo URL, Devpost URL (TBD) |

`TBD (from #45 target evidence)`: exact reason-code sequence per scene, confirmed in v2.

## 2. Narration

317 words (limit 380), plain English, one block per scene.

1. **Hook.** "An AI planner can be right about a world that no longer exists. In a hospital, that world changes fast."
2. **On QNX.** "This is GUARD, running on QNX 8.0 in the quick-start target image under QEMU. Each part is its own process, with its own priority."
3. **How it works.** "The planner only proposes motion. The supervisor checks each proposal against its own view of the world: how old the data is, which map it used, the speed limits, restricted zones, and the distance needed to stop. If every check passes, it grants a short lease. A separate gate process holds that lease and stops the robot when it runs out."
4. **Stale map.** "Here the MRI zone switches on. The planner still cites the old map, so GUARD rejects it and the robot waits. In the inset, a heartbeat-only baseline drives straight into the zone, because its planner is still alive."
5. **Kill the supervisor.** "Now we kill the supervisor. Nothing renews the lease, so the gate's own kernel timer brings the robot to a stop and latches. It stays stopped until an operator recovers it."
6. **Zombie.** "If a paused supervisor wakes up after it was replaced, its lease carries an old epoch, and the gate refuses it."
7. **Overload.** "A busy process above the supervisor starves the checks. Leases lapse, so the robot stops, then carries on when the load ends. Starving GUARD can only stop motion, never extend it."
8. **Staff crossing.** "Near a crossing, a fast proposal fails the stopping-distance check. A slower one is authorized. A shorter lease allows more speed, because the gate reacts sooner."
9. **Results.** "Across every fault scenario in our host harness, these are the outcomes for no supervisor, a heartbeat, and GUARD."
10. **Why QNX and limits.** "QNX gives us message passing with priority inheritance, timer pulses and resource managers, so each layer can stop the robot by itself. GUARD is a simulation-only research prototype, not a device for real robots or patients."
11. **Close.** "Stale decisions can't move the robot. Find GUARD on GitHub."

## 3. Claims list

Every claim the video makes, with the README section that backs it (README draft, #64).

| # | Claim | README section |
|---|---|---|
| C1 | GUARD runs on QNX 8.0 in the quick-start target image under QEMU (x86_64) | Intro; Quick start (QNX path) |
| C2 | Each part is a separate process with its own priority | How it works (diagram); QNX Evidence Map |
| C3 | The planner only proposes; the supervisor checks each proposal against its own observation history | How it works, steps 1-2 |
| C4 | Leases are short (`T_lease` default 250 ms) and epoch-fenced | How it works, step 3 |
| C5 | The gate stops the robot with its own kernel timer, even if the supervisor has died | Intro; How it works, step 4 |
| C6 | A stale map is rejected; a heartbeat-only baseline does not catch it | The problem; Results (TBD) |
| C7 | An old-epoch lease is refused | How it works, step 3 |
| C8 | Starving the checks can only stop motion, never extend it | How it works, paragraph after step 5 |
| C9 | Shorter leases allow more speed near hazards | How it works, step 3 |
| C10 | Results come from the host (Linux) harness | Results |
| C11 | Simulation-only research prototype, not for real robots or patients | Intro; Limits |

C8 and C9 are now backed by README lines (#64, HO4-11), from `docs/architecture.md` (stopping distance; planner-path starvation rule).

## 4. Shot list and capture checklist (Fri 11 Dec, tag `v1.0-rc1`)

**Before recording:**
- [ ] `git checkout v1.0-rc1`; boot the test image with `image/run-demo.sh --test` (builds with `image/build-image.sh --test`, adds `guard-hog` for scene 7, F2). Stop it after with `image/run-demo.sh --stop`.
- [ ] Close every other window. Hide desktop icons, notifications and personal folders.
- [ ] Screen at 1920 × 1080; scaling 100 %.
- [ ] Terminal font: monospace, 18 pt, dark background, prompt shortened to `$ `.

**Window layout:**
- Left half: QEMU window (top) and target ssh terminal (bottom).
- Right half: dashboard in a browser window, no tabs or bookmarks visible.
- Crop the host OS interface to these windows only (A8).

**Shots** (record each as its own clip, two takes each):

| Shot | Content | Scene |
|---|---|---|
| S1 | QEMU boot from power-on to the login prompt, banner fully visible | 2 |
| S2 | `uname -a` on the target | 2 |
| S3 | `pidin` showing the GUARD processes and priorities | 2, 7 |
| S4 | F6 live under B2 + host B1 replay for the inset | 4 |
| S5 | F3 with `guardctl inject F3` and `guardctl status` | 5 |
| S6 | F7 with `guardctl inject F7` | 6 |
| S7 | F2 with `guardctl inject F2` | 7 |
| S8 | F5 with the `d_req` overlay on | 8 |
| S9 | 30 s "kill the supervisor" clip for Community Choice (from S5) | -- |

**Recording tool:** OBS Studio. Canvas and output 1920 × 1080, 30 fps, MKV container (remux to MP4 after), one scene per window layout, desktop audio off, microphone recorded on a separate track.

## 5. Editing plan

- **Editor:** Kdenlive (fallback: Shotcut).
- **Captions:** English, file `docs/video/captions.srt` (draft timings in this PR; retime after the voice-over).
- **Music:** none in the main cut. If music is added later, use only a CC0-licensed track and name it in the video description.
- **Voice-over:** record the narration above after the edit is locked; target 2:45 of speech.
- **Community Choice cut:** 30 s from S5 (F3): kill, lease drains, stop, latch. Own captions, no music.
- **Export:** 1080p, MP4 (H.264 + AAC), under 3:00. Upload publicly by Sat 12 Dec 23:59 SGT.

## 6. Thumbnail spec

- 1280 × 720, dark background.
- Left: dashboard frame with the robot stopped at the edge of the red zone.
- Right, large text: "Stale decisions can't move the robot."
- Small text bottom right: "GUARD · runtime supervisor on QNX 8.0 · simulation-only".
- No QNX logo (A8); "QNX" only as plain text.
