# Spike S5 (#36): instrumented `procnto` for tracelogger on QSTI

Throwaway spike script. It stays on branch `spike/s5` and never goes to `main`.
It has not been run yet: the agent that wrote it has no QNX target.

## Run (target, as root)

```sh
scp spike/s5/run.sh root@<target-ip>:/tmp/
ssh root@<target-ip> sh /tmp/run.sh
```

The last line is `S5 kernel events captured: yes/no`.
Do not commit `/tmp/s5.kev` (binary). Put only the script output in the evidence bundle.

## Decision to record in DECISIONS.md (row S5)

- yes: B-12c tracelogger timeline (#48) is GO.
- no: B-12c is NO-GO. Write why (tool missing, non-instrumented kernel, empty file).
