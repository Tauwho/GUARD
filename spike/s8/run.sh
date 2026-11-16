#!/bin/bash
# Spike S8 (#35), run on the QNX host (Linux/WSL2) with the SDP env sourced.
# Builds s8_hello, an --extra-dirs content dir with snippets/system_files.guard and
# snippets/post_start.guard, a fresh QSTI image in ~/qsti-s8/img, boots it on br0 (10.10.10.2),
# and checks that the binary is in /system and ran at boot.
# Stop any other QSTI on 10.10.10.2 first (mkqnximage --stop in its directory).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
W=${S8_WORK:-$HOME/qsti-s8}
mkdir -p "$W/bin"
qcc -Vgcc_ntox86_64 -Wall -Wextra -o "$W/bin/s8_hello" "$HERE/s8_hello.c"
rm -rf "$W/extra" && mkdir -p "$W/extra/snippets"
sed "s|__S8_BIN__|$W/bin|" "$HERE/extra/snippets/system_files.guard" > "$W/extra/snippets/system_files.guard"
cp "$HERE/extra/snippets/post_start.guard" "$W/extra/snippets/"
mkdir -p "$W/img" && cd "$W/img" # mkqnximage wants an empty or existing image directory
export OPT_NETWORK=bridge:br0
mkqnximage --type=qemu --arch=x86_64 --ip=10.10.10.2 --ssh-ident="$HOME/.ssh/id_ed25519.pub" \
    --extra-dirs="$W/extra" --noprompt --build > "$W/build.log" 2>&1 || { tail -30 "$W/build.log"; exit 1; }
echo "S8 image built with --extra-dirs: yes"
setsid nohup mkqnximage --run=-h --noprompt > "$W/run.log" 2>&1 < /dev/null &
SSHO=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)
for _ in $(seq 1 40); do
    ssh "${SSHO[@]}" -o ConnectTimeout=3 root@10.10.10.2 true 2>/dev/null && break
    sleep 3
done
T() { ssh "${SSHO[@]}" root@10.10.10.2 "$@"; }
T 'uname -a; ls -l /system/bin/s8_hello; cat /data/var/tmp/s8_hello.txt /data/var/tmp/s8_hello_uid.txt' || true
grep -E 'post_startup.sh.*(error|not found)' "$W/img/output/qemu.out" || echo "post_startup.sh errors: none"
yn() { if T "$1" >/dev/null 2>&1; then echo yes; else echo no; fi; }
echo "S8 binary added to /system via system_files.guard: $(yn 'test -x /system/bin/s8_hello')"
echo "S8 started at boot via post_start.guard (STARTU_P, no secpol, runs as root): $(yn 'grep -q "uid=0" /data/var/tmp/s8_hello.txt')"
echo "S8 started at boot as uid 1101 via 'on -u': $(yn 'grep -q "uid=1101" /data/var/tmp/s8_hello_uid.txt')"
