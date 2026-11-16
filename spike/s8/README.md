# Spike S8 (#35): `mkqnximage` snippet adding GUARD binaries and startup

Throwaway spike code. It stays on branch `spike/s8` and never goes to `main`.

## Run (QNX host, SDP 8.0 env sourced, bridge `br0` up)

```sh
bash spike/s8/run.sh     # builds ~/qsti-s8/img, boots it at 10.10.10.2, prints yes/no lines
```

Stop any other QSTI on `10.10.10.2` first (`mkqnximage --stop` in its directory).

## What works

| Candidate | Result |
|---|---|
| `--extra-dirs=<dir>` with `<dir>/snippets/system_files.<suffix>` | Works: files land in `/system` |
| `--extra-dirs=<dir>` with `<dir>/snippets/post_start.<suffix>` | Works: commands run at the end of `post_startup.sh` |
| The same files named `*.custom` in an extra dir | Lost: on the first build `mkqnximage` copies its placeholder `local/snippets/*.custom` over them |
| `local/snippets/system_files.custom` / `post_start.custom` | Works, but lives in the image dir, not the repo |
| `ifs_start` | Runs before `/system` is mounted; not needed for GUARD binaries |

- The template comment in `post_start.custom` names `START()` / `STARTU()`. SDP 8.0 defines `START_P(type)` and `STARTU_P(type,uid)` (`inputs/definitions.inc`). They add `on -T <type> -u <uid>:<uid>` only when `--secpol` is not `no`. Without secpol the process starts as root.
- `on -u 1101:1101 <cmd>` starts a process as a GUARD uid without secpol.
- `system_files` paths need the absolute host path of the binary; `run.sh` fills it in.

## Decision to record in DECISIONS.md (row S8)

B-9 (#43) uses `--extra-dirs=image/mkqnximage` with `snippets/system_files.guard` and `snippets/post_start.guard`. GUARD processes start as root and drop privileges themselves (docs/architecture.md), so `STARTU_P` is only needed once secpol is on.
