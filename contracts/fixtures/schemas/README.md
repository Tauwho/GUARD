# Schema fixtures

`<schema>.valid.json` must validate; `<schema>.invalid.json` must fail.

| Schema | Why the invalid fixture fails |
|---|---|
| `telemetry.schema.json` | gate mode PARKED does not exist |
| `config.schema.json` | ablations only apply to B2, and t_lease_ms 20 is below the 50 ms minimum |
| `evidence-manifest.schema.json` | short sha, bad date, bad topic, unknown role, empty host_os, SDP 7.1, disallowed file |
| `planner-remote.schema.json` | missing based_on and empty waypoints |
| `scenario.schema.json` | process scenario without host_abstraction, and expected lacks B0 and B1 |
