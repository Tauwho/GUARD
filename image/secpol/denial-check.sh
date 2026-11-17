#!/bin/sh
# image/secpol/denial-check.sh (#46, D22): on the secure image, as root.
#   sh denial-check.sh
# 1. planner_t (uid 1104) opens /dev/guard/gate: secpol has no planner_t -> gate_t connect rule
#    (REF-SECURITY), so the connect is denied with EPERM.
# 2. supervisor_t may connect to the gate, but uid 1104 may not recover: the gate's uid check
#    refuses with EPERM and counts GATE_UNAUTHORIZED_CLIENT.
# 3. Control: the operator (root) reaches the gate.
# guardctl names which check refused. Prints CHECK lines and a RESULT line.
failed=0
expect() { # expect <name> <exit> <stderr text> <command...>
    name=$1 want=$2 text=$3
    shift 3
    "$@" > /tmp/denial_out 2> /tmp/denial_err
    got=$?
    if [ "$got" = "$want" ] && grep -q "$text" /tmp/denial_err /tmp/denial_out; then r=PASS; else r=FAIL; failed=$((failed + 1)); fi
    echo "CHECK $name (exit $got) $r"
    { sed -n 1p /tmp/denial_err; sed -n 1p /tmp/denial_out; } | sed 's/^/  /'
}
count() { guardctl status | sed -n 's/^count.GATE_UNAUTHORIZED_CLIENT=//p'; }
echo "secpol mode: $(cat /system/etc/guard-secpol 2>/dev/null || echo none)"
c0=$(count)
expect "planner_t -> /dev/guard/gate: secpol denies the connect" 3 "connect denied" \
    on -T planner_t -u 1104 guardctl recover
c1=$(count)
expect "supervisor_t, uid 1104 -> recover: gate uid check" 3 "gate uid check" \
    on -T supervisor_t -u 1104 guardctl recover
c2=$(count)
# The secpol denial never reaches the gate (+0); the uid denial is counted (+1).
if [ "${c1:-0}" = "${c0:-0}" ] && [ "${c2:-0}" = "$((${c1:-0} + 1))" ]; then r=PASS; else r=FAIL; failed=$((failed + 1)); fi
echo "CHECK GATE_UNAUTHORIZED_CLIENT: +0 for the secpol denial, +1 for the uid denial $r"
guardctl recover > /tmp/denial_out 2>&1
rc=$?
if [ "$rc" -le 1 ]; then r=PASS; else r=FAIL; failed=$((failed + 1)); fi
echo "CHECK operator (root) reaches the gate (exit $rc) $r"
sed -n 1p /tmp/denial_out | sed "s/^/  /"
echo "RESULT $([ $failed = 0 ] && echo PASS || echo FAIL) ($failed failed)"
[ $failed = 0 ]
