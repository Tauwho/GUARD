<!-- managed-by: guard-bootstrap v1 -->
# evidence/

**When:** required on every PR touching `core/`, `qnx/`, `image/`, `cmake/` or `contracts/`.

**Bundle path:** `evidence/<yyyymmdd>-<sha7>-<topic>/`. Text only, ≤ 1 MiB.

| File | Content |
|---|---|
| `manifest.json` | sha, date, SDP version, `qcc -V`, image ID, host OS, runner role |
| `transcript.txt` | `script(1)` capture of the ssh session |
| `uname.txt` | `uname -a` on the target |
| `pidin.txt` | Process listing |
| `binaries.txt` | `file`, `readelf -h` and `sha256` for each binary |
| `slog2.txt` | `slog2info` output |
| `results.json` | Test or scenario results |

**CI checks:**
- The manifest validates against its schema.
- The sha is an ancestor of HEAD.
- The size limit holds.

**Redaction:** remove any licence identifiers before committing.
