#!/bin/bash
# image/secpol/generate.sh (#46, D22): record the GUARD secpol policy.
# Builds the test image with --secpol=develop, boots it, runs F0-F8 through
# eval/target_runner.py so secpolgenerate sees every connect and ability GUARD uses, then
# writes the recorded policy to image/secpol/guard.secpol. Run on the QNX host, SDP sourced.
#   image/secpol/generate.sh [--alias guard-target]
# Never run the denial demo (image/secpol/denial-check.sh) on a develop image: develop mode
# grants and records every request, so the denied edge would end up in the policy.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
ALIAS=${GUARD_TARGET:-guard-target}
[ "${1:-}" = --alias ] && ALIAS=$2
OUT=$ROOT/build/secpol-generate
mkdir -p "$OUT"
"$ROOT/image/run-demo.sh" --stop > /dev/null
"$ROOT/image/run-demo.sh" --test --secpol develop | grep -E 'secpol|started|missing'
# F7 and F3 last: they leave the supervisor stopped or killed.
for F in F0 F1 F2 F4 F5 F6 F8 F7 F3; do
    python3 "$ROOT/eval/target_runner.py" --target "ssh:$ALIAS" --scenario "$ROOT/scenarios/$F.yaml" \
        --out "$OUT/$F.jsonl" > /dev/null 2>&1 && echo "$F recorded" || echo "$F ran (exit $?)"
done
# The denial demo starts guardctl as planner_t and supervisor_t with uid 1104. Record only
# that start (guardctl --version touches no GUARD path), never the gate connect itself.
ssh -o BatchMode=yes "$ALIAS" 'on -T planner_t -u 1104 guardctl --version; on -T supervisor_t -u 1104 guardctl --version' > /dev/null
ssh -o BatchMode=yes "$ALIAS" 'guardctl status > /dev/null; cat /dev/secpolgenerate/policy' \
    > "$ROOT/image/secpol/guard.secpol"
echo "wrote image/secpol/guard.secpol ($(wc -l < "$ROOT/image/secpol/guard.secpol") lines)"
"$ROOT/image/run-demo.sh" --stop > /dev/null
