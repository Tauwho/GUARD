# contracts/fixtures

Golden examples for contract conformance tests (A-2) and the CI `schemas` check (A-1).

| Folder | Content | Rule |
|---|---|---|
| `messages/` | `<message>.valid.json` and `<message>.invalid.json` for all 13 binary messages in `messages.md` | Encoding `fields` must give exactly `hex` (little-endian). Decoding the invalid `hex` must fail with `expect_error` |
| `schemas/` | `<schema>.valid.json` and `<schema>.invalid.json` for each `*.schema.json` | The valid file must pass; the invalid file must fail. `schemas/README.md` says why |

Notes:
- `fields` in message fixtures use nested objects for `based_on`. Floats that JSON cannot hold are written as the strings `"NaN"` and `"Infinity"`.
- Unlisted bytes (reserved fields, unused obstacle slots, string padding) are zero.
- Scenario and map files under `scenarios/` are also checked against `scenario.schema.json`.
