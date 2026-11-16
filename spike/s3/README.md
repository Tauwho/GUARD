# Spike S3 (#33): resmgr devctl, late read reply, OCB close, mutex inheritance, pulse order

Throwaway spike code. It stays on branch `spike/s3` and never goes to `main`.
It has not been compiled yet: the agent that wrote it has no QNX SDP.

## Build (QNX host, SDP 8.0 env sourced)

```sh
qcc -Vgcc_ntox86_64 -Wall -o /tmp/s3_server spike/s3/s3_server.c
qcc -Vgcc_ntox86_64 -Wall -o /tmp/s3_client spike/s3/s3_client.c
scp /tmp/s3_server /tmp/s3_client spike/s3/run.sh root@<target-ip>:/tmp/
```

## Run (target, as root)

```sh
cd /tmp && sh run.sh
```

## What each line answers

| Question | Output line |
|---|---|
| 1. `__DIOTF` `devctl` round trip | `Q1 __DIOTF round trip works: yes/no` |
| 2. Late `MsgReply` on a saved `rcvid` for a blocked `read` | `Q2 ... yes/no` |
| 3. OCB close after `kill -9`, and how quickly | `Q3 resmgr saw OCB close ...` and `Q3 close seen within 100 ms ...` |
| 4. Mutex priority inheritance by default | `Q4 default mutexattr protocol` and `Q4 default mutex boosted holder ...` |
| 5. Pulse vs `devctl` order on one dispatch channel | the four `Q5 ... order=` lines |

Q3 reports only "within 100 ms: yes/no". It does not print a QNX timing number (AGENTS.md section 2).

## Decision to record in DECISIONS.md (row S3)

- All of Q1-Q3 yes: keep the resmgr design (ADR 0005/0006).
- Any of Q1-Q3 no: use the `name_attach` fallback and say which question failed.
- Q4 and Q5: write the observed answer, so `qnx/common` (#37) can rely on it.
