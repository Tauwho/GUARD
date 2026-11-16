# Read-only telemetry dashboard

No build step. Install `dashboard/requirements.txt`, then run from the repository root:

```sh
python dashboard/relay.py --trace out/runs/F3-B2-1003.jsonl
```

Open `http://127.0.0.1:8765`. Replay is delivered by WebSocket on localhost:8766.
The browser makes no external network calls. Both listeners bind only to loopback.
Use `--map scenarios/maps/mri_suite.yaml` for the MRI scenario or `--port` for another
pair of ports. Supply the map that actually belongs to the trace.

Play/pause, speed and seek are local visualization controls; nothing is sent to the
robot. The relay validates C8 records and serves only an explicit static-file list.
Shared scenario zone schedules are displayed when the trace identifies F0–F8 and
the selected map matches. Geometry/schedules are supplied scenario metadata, not
measurements inferred from the robot.

Current view: map, zone schedules, observed obstacles, robot footprint/trajectory,
gate mode/epoch, decision counts and reason journal. Live bridge input, lease bar,
recorded stopping arc and target-trace acceptance are still work in progress (#24).
No target numeric evaluation or QNX timings should be published (ADR0011/D11).
