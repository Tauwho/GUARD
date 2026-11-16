# Spike S6 (#32): host to target TCP stream on port 7700

Throwaway spike code. It stays on branch `spike/s6` and never goes to `main`.
`s6.c` builds with SDP 8.0 `qcc` (needs `-lsocket`). It has not run on a target yet.

## Run

1. Host (same machine as QEMU): `python3 spike/s6/s6_host.py 1000`
2. Target, first try the built-in client (host IP is the bridge address, setup.md assumes `10.10.10.1`):

   ```sh
   i=0; while [ $i -lt 1000 ]; do echo "{\"seq\":$i}"; i=$((i+1)); done | nc 10.10.10.1 7700
   ```

3. If `nc` is missing: on the host, `qcc -Vgcc_ntox86_64 -Wall -o /tmp/s6 spike/s6/s6.c -lsocket`,
   `scp /tmp/s6 root@<target-ip>:/tmp/`, then on the target `/tmp/s6 10.10.10.1 1000`.

The host prints `S6 host received the target stream intact: yes/no`.

## Decision to record in DECISIONS.md (row S6)

- yes: the bridge streams to the host on TCP 7700 (C8).
- no: use the fallback. Log to a file on the target, `scp` it, and replay it in the dashboard.
  Write which step failed (no route, connection refused, lines lost).
