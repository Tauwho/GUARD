#!/bin/bash
# Boot the GUARD image and get the demo going (B-9, #43). Run on the QNX host, SDP env sourced.
#   image/run-demo.sh [--net user|bridge] [--rebuild] [--test]
# Builds the image if needed (image/build-image.sh), boots QEMU in the background, waits for
# ssh and the GUARD start log, then prints the live dashboard command (dashboard/relay.py).
# Stop the target: image/run-demo.sh --stop
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/image/out
NET=user REBUILD=0 BUILD_ARGS=()
while [ $# -gt 0 ]; do
    case $1 in
        --net) NET=$2; BUILD_ARGS+=(--net "$2"); shift 2 ;;
        --rebuild) REBUILD=1; shift ;;
        --test) BUILD_ARGS+=(--test); REBUILD=1; shift ;;
        --stop)
            [ -f "$OUT/qsti/output/qemu.pid" ] && kill "$(cat "$OUT/qsti/output/qemu.pid")" 2>/dev/null
            echo "target stopped"; exit 0 ;;
        *) echo "usage: $0 [--net user|bridge] [--rebuild] [--test] | --stop" >&2; exit 2 ;;
    esac
done
KEY=$HOME/.ssh/guard_target_ed25519
if [ "$REBUILD" = 1 ] || [ ! -f "$OUT/qsti/output/ifs.bin" ]; then
    "$ROOT/image/build-image.sh" "${BUILD_ARGS[@]}"
fi

cd "$OUT/qsti"
if [ "$NET" = bridge ]; then
    HOST=10.10.10.2 PORT=22
    OPT_NETWORK=bridge:br0 setsid nohup mkqnximage --run=-h --noprompt > "$OUT/run.log" 2>&1 < /dev/null &
else
    HOST=127.0.0.1 PORT=2222
    KVM=(); [ -w /dev/kvm ] && KVM=(-enable-kvm -cpu host)
    setsid qemu-system-x86_64 "${KVM[@]}" -smp 2 -m 1G \
        -drive file=output/disk-qemu.vmdk,if=ide,id=drv0 \
        -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22,hostfwd=tcp:127.0.0.1:7700-:7700 -device virtio-net-pci,netdev=net0 \
        -object rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-pci,rng=rng0 \
        -kernel output/ifs.bin -serial file:output/qemu.out -display none \
        -pidfile output/qemu.pid < /dev/null > /dev/null 2>&1 &
fi

SSH=(ssh -i "$KEY" -p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=no
    -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=3 "root@$HOST")
for _ in $(seq 1 60); do
    "${SSH[@]}" true 2>/dev/null && break
    sleep 3
done
"${SSH[@]}" true || { echo "target did not come up; see $OUT/qsti/output/qemu.out" >&2; exit 1; }
echo "target up: ${SSH[*]}"
"${SSH[@]}" cat /data/var/log/guard-start.log

echo "C8 telemetry: guard-bridge on 127.0.0.1:7700 (forwarded). Live dashboard:"
echo "  python3 dashboard/relay.py --bridge 127.0.0.1:7700   # then open http://127.0.0.1:8765"
