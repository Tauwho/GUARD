#!/bin/sh
# image/slm/slm-check.sh (B10, #47): on the SLM test image (run-demo.sh --test --slm), as root.
#   sh slm-check.sh
# Extended F3: kill -9 the supervisor while F0 runs. SLM restarts it, the gate stays LATCHED
# (SUPERVISOR_LOST) and the robot stays stopped until guardctl recover; then motion resumes.
# Then kill -9 the gate: SLM restarts it and the new gate comes up LATCHED (BOOT).
# Prints CHECK lines and a RESULT line; pids are compared, never printed with times.
failed=0
check() { if eval "$2"; then r=PASS; else r=FAIL; failed=$((failed + 1)); fi; echo "CHECK $1 $r"; }
st() { guardctl status | sed -n "s/^$1=//p"; }
pid() { slay -p "$1" 2>/dev/null | head -1; }
wait_for() { # wait_for <key> <value> <tenths of a second>
    n=0
    while [ "$(st "$1")" != "$2" ] && [ $n -lt "$3" ]; do sleep 0.1; n=$((n + 1)); done
    [ "$(st "$1")" = "$2" ]
}
guardctl scenario stop > /dev/null 2>&1
guardctl scenario load /system/guard/scenarios/F0.yaml --seed 1000 > /dev/null
guardctl scenario start > /dev/null
check "F0 running: gate MOVING" 'wait_for mode MOVING 50'
sup=$(pid guard-supervisor)
kill -9 "$sup"
check "kill -9 supervisor: gate LATCHED, SUPERVISOR_LOST" 'wait_for mode LATCHED 50 && [ "$(st latch_cause)" = SUPERVISOR_LOST ]'
n=0
while { [ -z "$(pid guard-supervisor)" ] || [ "$(pid guard-supervisor)" = "$sup" ]; } && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
check "SLM restarted guard-supervisor (new pid)" '[ -n "$(pid guard-supervisor)" ] && [ "$(pid guard-supervisor)" != "$sup" ]'
sleep 2
check "robot stays stopped until recover (LATCHED, v_cmd 0)" '[ "$(st mode)" = LATCHED ] && [ "$(st v_cmd)" = 0.000 ]'
echo "  recover: $(guardctl recover)"
check "guardctl recover with conditions met: READY" '[ "$(st mode)" = READY ] || [ "$(st mode)" = MOVING ]'
check "motion resumes under the restarted supervisor" 'wait_for mode MOVING 50'
gate=$(pid guard-gate)
kill -9 "$gate"
n=0
while { [ -z "$(pid guard-gate)" ] || [ "$(pid guard-gate)" = "$gate" ]; } && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
check "SLM restarted guard-gate (new pid)" '[ -n "$(pid guard-gate)" ] && [ "$(pid guard-gate)" != "$gate" ]'
check "restarted gate comes up LATCHED (BOOT)" 'wait_for mode LATCHED 30 && [ "$(st latch_cause)" = BOOT ]'
sleep 1
echo "  recover: $(guardctl recover)"
check "after recover the supervisor leases again: MOVING" 'wait_for mode MOVING 50'
guardctl scenario stop > /dev/null
echo "RESULT $([ $failed = 0 ] && echo PASS || echo FAIL) ($failed failed)"
[ $failed = 0 ]
