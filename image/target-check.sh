#!/bin/bash
# B0 (Stream B brief): one target check, end to end.
#   image/target-check.sh [options] <topic>
# Steps:
#   1. build the qnx-x86_64 preset (or the --project standalone CMake project)
#   2. boot the target with image/run-demo.sh (skipped if the ssh alias already answers)
#   3. copy each test binary with  ssh <alias> 'cat > /tmp/<name>'
#   4. run it on the target
#   5. capture evidence/<yyyymmdd>-<sha7>-<topic>/ with tools/capture-evidence.py
#   6. stop the target (only if this script booted it, unless --keep)
# Options:
#   --project <dir>   build a standalone project (e.g. qnx/gate) instead of the qnx-x86_64 preset
#   --bin <name>      test binary inside the build dir (repeatable; default guard_core_tests)
#   --run <cmd>       extra command to run on the target after the binaries (repeatable)
#   --alias <host>    ssh alias (default $GUARD_TARGET or guard-target, see ENV.md)
#   --net user|bridge passed to run-demo.sh (default $GUARD_NET or user)
#   --keep            leave the target running
#   --dry-run         print each step, change nothing
#   -h, --help        this text
# Needs the SDP env sourced (qnxsdp-env.sh). Prints pass/fail and counts only, no timings.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PROJECT="" BINS=() RUNS=() KEEP=0 DRY=0 TOPIC=""
ALIAS=${GUARD_TARGET:-guard-target}
NET=${GUARD_NET:-user}

usage() { sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; }
while [ $# -gt 0 ]; do
    case $1 in
        --project) PROJECT=$2; shift 2 ;;
        --bin) BINS+=("$2"); shift 2 ;;
        --run) RUNS+=("$2"); shift 2 ;;
        --alias) ALIAS=$2; shift 2 ;;
        --net) NET=$2; shift 2 ;;
        --keep) KEEP=1; shift ;;
        --dry-run) DRY=1; shift ;;
        -h|--help) usage; exit 0 ;;
        -*) echo "unknown option $1" >&2; usage >&2; exit 2 ;;
        *) TOPIC=$1; shift ;;
    esac
done
[ -n "$TOPIC" ] || { echo "missing <topic>" >&2; usage >&2; exit 2; }
[ ${#BINS[@]} -gt 0 ] || BINS=(guard_core_tests)

step() { echo "== $*"; }
do_run() { if [ "$DRY" = 1 ]; then echo "   (dry-run) $*"; else "$@"; fi; }

SHA=$(git -C "$ROOT" rev-parse HEAD)
if [ -n "$PROJECT" ]; then
    BUILD=$ROOT/build/$(echo "$PROJECT" | tr / -)
    step "1/6 build $PROJECT at ${SHA:0:7}"
    do_run cmake -S "$ROOT/$PROJECT" -B "$BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/qnx-x86_64.cmake"
    do_run cmake --build "$BUILD"
else
    BUILD=$ROOT/build/qnx-x86_64
    step "1/6 build preset qnx-x86_64 at ${SHA:0:7}"
    do_run cmake --preset qnx-x86_64 -S "$ROOT"
    do_run cmake --build --preset qnx-x86_64
fi

SSH=(ssh -o BatchMode=yes -o ConnectTimeout=5 "$ALIAS")
BOOTED=0
step "2/6 target $ALIAS"
if [ "$DRY" = 0 ] && "${SSH[@]}" true 2>/dev/null; then
    echo "   already up"
else
    do_run "$ROOT/image/run-demo.sh" --net "$NET"
    BOOTED=1
fi

step "3/6 copy ${BINS[*]}"
for b in "${BINS[@]}"; do
    [ "$DRY" = 1 ] || [ -f "$BUILD/$b" ] || { echo "missing $BUILD/$b" >&2; exit 1; }
    if [ "$DRY" = 1 ]; then
        echo "   (dry-run) ${SSH[*]} 'cat > /tmp/$b && chmod +x /tmp/$b' < $BUILD/$b"
    else
        "${SSH[@]}" "cat > /tmp/$b && chmod +x /tmp/$b" < "$BUILD/$b"
    fi
done

step "4/6 run on target"
CAP=(--topic "$TOPIC" --target "$ALIAS" --sha "$SHA" --out "$ROOT/evidence")
for b in "${BINS[@]}"; do
    do_run "${SSH[@]}" "/tmp/$b 2>&1 | tail -3"
    CAP+=(--binary "$BUILD/$b" --run "/tmp/$b")
done
for r in "${RUNS[@]}"; do CAP+=(--run "$r"); done

step "5/6 capture evidence ($TOPIC)"
do_run python3 "$ROOT/tools/capture-evidence.py" "${CAP[@]}"

step "6/6 stop target"
if [ "$KEEP" = 1 ] || [ "$BOOTED" = 0 ]; then
    echo "   left running"
else
    do_run "$ROOT/image/run-demo.sh" --stop
fi
