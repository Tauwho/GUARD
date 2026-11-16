# Spike S4 (#34): secpol, denied connect, abilities across `setuid`, client uid

Throwaway spike code. It stays on branch `spike/s4` and never goes to `main`.
It has not been compiled yet: the agent that wrote it has no QNX SDP.
Steps marked **(verify)** are unchecked guesses about QNX tooling. Fix them from the QNX docs as you go.

## Build (QNX host, SDP 8.0 env sourced)

```sh
qcc -Vgcc_ntox86_64 -Wall -o /tmp/s4_ability spike/s4/s4_ability.c
qcc -Vgcc_ntox86_64 -Wall -o /tmp/s4_uid spike/s4/s4_uid.c
scp /tmp/s4_ability /tmp/s4_uid root@<target-ip>:/tmp/
```

## Q3 and Q4 (default image, as root)

```sh
cd /tmp && ./s4_ability && ./s4_uid
```

- Q3: every line of tests A, B and C should end in `as-expected`. Any `UNEXPECTED` line is the finding.
- Q4: the last line is `Q4 ConnectClientInfo uid matches every client: yes/no`. The `seteuid` line shows whether the server sees ruid or euid.

## Q1 and Q2 (secpol image)

1. Write a policy with two types, `s4srv_t` and `s4cli_t`. Give `s4srv_t` a channel rule, and give `s4cli_t` **no** connect rule to it. **(verify)** policy syntax in the QNX 8.0 "Security policies" guide.
2. Build the image: `mkqnximage --secpol=secure ...` with that policy **(verify)** the option that takes a custom policy file. Q1 is whether this builds and boots.
3. On the target, start the server as `s4srv_t`, and run `./s4_uid client` as `s4cli_t` **(verify)** how to launch a process as a type (`on -T` or `secpolexec`?).
4. Q2 is the errno on the `client uid=...: connect+send` line.

## Decision to record in DECISIONS.md (row S4)

- Q1 builds and boots, Q2 gives a clear errno: secpol can be P0.
- Otherwise: secpol stays P1. Relabel B-12a (#46) to match.
- Write the Q3 and Q4 answers too, since `qnx/common` (#37) depends on the privilege-drop order.
