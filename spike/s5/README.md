# Spike S5 (#36): instrumented `procnto` for tracelogger on QSTI

Throwaway spike script. It stays on branch `spike/s5` and never goes to `main`.

## Run (target, as root)

The default QSTI image has `tracelogger` but not `traceprinter`. Copy it from the SDP:

```sh
scp $QNX_TARGET/x86_64/usr/bin/traceprinter spike/s5/run.sh root@<target-ip>:/tmp/
ssh root@<target-ip> sh /tmp/run.sh
```

The last result line is `S5 kernel events captured: yes/no`.
The script deletes the trace afterwards: `/tmp` is the small `/data` partition (64 MiB) and a 2 s trace is about 17 MB.
Do not commit the `.kev` file (binary). Put only the script output in the evidence bundle.

## Decision to record in DECISIONS.md (row S5)

- yes: B-12c tracelogger timeline (#48) is GO. The image must ship `traceprinter`, or the timeline must be decoded on the host.
- no: B-12c is NO-GO. Write why (tool missing, non-instrumented kernel, empty file).
