#!/bin/bash
# Spike S4 (#34) Q1/Q2, run on the QNX host (Linux/WSL2), bridge br0 up, SDP 8.0.
# Stage A: --secpol=develop image; secpolgenerate records a policy while
#   s4srv_t serves, s4ok_t connects to it, and s4cli_t runs without connecting.
# Stage B: rebuild with --secpol=secure --policy=<that policy>, boot (Q1), then
#   s4ok_t connects (control) and s4cli_t tries to connect (Q2: errno).
# Stop any other QSTI on 10.10.10.2 first. Image dir: ~/qsti-s4/img.
set -uo pipefail
HERE=$(cd "$(dirname "$0")/.." && pwd)
W=${S4_WORK:-$HOME/qsti-s4}
SSHO=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)
T() { ssh "${SSHO[@]}" root@10.10.10.2 "$@"; }
wait_ssh() {
    for _ in $(seq 1 60); do T true 2>/dev/null && return 0; sleep 3; done
    return 1
}
boot() { # $1 = log name, rest = extra mkqnximage options
    local log=$1; shift
    (cd "$W/img" && mkqnximage --stop >/dev/null 2>&1); sleep 2
    (cd "$W/img" && OPT_NETWORK=bridge:br0 mkqnximage --type=qemu --arch=x86_64 --ip=10.10.10.2 \
        --ssh-ident="$HOME/.ssh/id_ed25519.pub" --extra-dirs="$W/extra" --noprompt --build "$@" \
        > "$W/$log.build.log" 2>&1) || { tail -20 "$W/$log.build.log"; return 1; }
    (cd "$W/img" && OPT_NETWORK=bridge:br0 setsid nohup mkqnximage --run=-h --noprompt \
        > "$W/$log.run.log" 2>&1 < /dev/null &)
    wait_ssh
}

mkdir -p "$W/bin" "$W/img"
qcc -Vgcc_ntox86_64 -Wall -Wextra -o "$W/bin/s4_uid" "$HERE/s4_uid.c" || exit 1
rm -rf "$W/extra" && mkdir -p "$W/extra/snippets"
sed "s|__S4_BIN__|$W/bin|" "$HERE/secpol/snippets/system_files.guard" > "$W/extra/snippets/system_files.guard"
cp "$HERE/secpol/snippets/post_start.guard" "$W/extra/snippets/"

echo "######## stage A: --secpol=develop"
if boot A --secpol=develop --policy=none; then echo "S4 develop image boots: yes"; else echo "S4 develop image boots: no"; exit 1; fi
T 'sleep 2; on -T s4ok_t /system/bin/s4_uid client; on -T s4cli_t /system/bin/s4_uid idle'
T 'cat /dev/secpolgenerate/policy' > "$W/s4.pol"
echo "policy lines: $(wc -l < "$W/s4.pol")"
grep -n -A6 -E 'Rules for type s4(srv|ok|cli)_t' "$W/s4.pol"
if grep -A8 'Rules for type s4cli_t' "$W/s4.pol" | grep -q 'channel_connect:.*s4srv_t'; then
    echo "s4cli_t has a connect rule to s4srv_t: yes (test invalid)"; exit 1
fi
echo "s4cli_t has a connect rule to s4srv_t: no"

echo "######## stage B: --secpol=secure --policy=s4.pol"
if boot B --secpol=secure --policy="$W/s4.pol"; then
    echo "Q1 --secpol=secure with the custom policy builds and boots: yes"
else
    echo "Q1 --secpol=secure with the custom policy builds and boots: no"
    tail -20 "$W/img/output/qemu.out"; exit 1
fi
T 'secpol -Q; sleep 2; pidin -f an | grep s4_uid'
echo "== s4ok_t (has a connect rule)"; T 'on -T s4ok_t /system/bin/s4_uid client'
echo "== s4cli_t (no connect rule)"; T 'on -T s4cli_t /system/bin/s4_uid client'
echo "Q2 key: the s4cli_t line above shows the errno of a denied connect"
