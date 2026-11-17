#!/bin/sh
# guardctl_check (B5, #42): exit codes and output lines of guardctl against contracts/guardctl.md.
# Run as root on the GUARD image (guard-sim, guard-gate, guard-supervisor running):
#   sh /tmp/guardctl_check.sh
# Prints CHECK lines (PASS/FAIL) and the guardctl output lines; no timing values.
failed=0
check() { # check <name> <expected exit> <command...>
    name=$1 want=$2
    shift 2
    out=$("$@" 2>/tmp/guardctl_err)
    got=$?
    if [ "$got" = "$want" ]; then r=PASS; else r=FAIL; failed=$((failed + 1)); fi
    echo "CHECK $name (exit $want, got $got) $r"
    # D11: no timing values in the transcript (t_ns, *_ns, tick counts).
    [ -n "$out" ] && echo "  out: $(echo "$out" | head -1 | sed -e 's/t_ns=[0-9]*/t_ns=<n>/' \
        -e 's/"[a-z_]*_ns":[0-9]*,//g' -e 's/"[a-z_]*ticks":[0-9]*,//g')"
    [ -s /tmp/guardctl_err ] && echo "  err: $(head -1 /tmp/guardctl_err)"
}
S=/system/guard/scenarios
check "usage error" 2 guardctl frobnicate
check "bad flag value" 2 guardctl watch --interval-ms 10
check "--version" 0 guardctl --version
check "--help" 0 guardctl --help
check "status (C4 text)" 0 guardctl status
check "status --json" 0 guardctl status --json
guardctl status --json | grep -q '"counts":{' && echo "CHECK status --json has counts PASS" || { echo "CHECK status --json has counts FAIL"; failed=$((failed + 1)); }
check "status as uid 1105 works for any uid" 0 on -u 1105 guardctl status
check "recover as uid 1104 -> EPERM" 3 on -u 1104 guardctl recover
check "scenario load missing file -> invalid" 6 guardctl scenario load $S/none.yaml
check "scenario load relative path -> usage" 2 guardctl scenario load F0.yaml
check "scenario load F0" 0 guardctl scenario load $S/F0.yaml --seed 1000 --mode B2
check "scenario load --json" 0 guardctl --json scenario load $S/F0.yaml --seed 1000
check "scenario start (recovers a latched gate)" 0 guardctl scenario start
sleep 1
check "recover while not latched -> refused" 1 guardctl recover
check "inject F4" 0 guardctl inject F4 --duration-ms 500
check "inject F9 -> usage" 2 guardctl inject F9
if [ -x /system/bin/guard-hog ]; then
    check "inject F2 (test image)" 0 guardctl inject F2 --duration-ms 100
else
    check "inject F2 without guard-hog -> missing" 5 guardctl inject F2
fi
sleep 1
check "recover after F4 latch" 0 guardctl recover
check "scenario stop" 0 guardctl scenario stop
check "scenario start after end -> EBUSY" 6 guardctl scenario start
slay -f -Q -s SIGSTOP guard-sim
check "sim stopped: scenario stop -> timeout" 4 guardctl scenario stop
slay -f -Q -s SIGCONT guard-sim
slay -f -Q guard-sim
sleep 1
check "sim gone: scenario stop -> device missing" 5 guardctl scenario stop
on -d /system/bin/guard-sim </dev/null >/dev/null 2>&1 # do not hold the ssh session open
waitfor /dev/sim/world 5
check "sim back: scenario load F0" 0 guardctl scenario load $S/F0.yaml
echo "RESULT $([ $failed = 0 ] && echo PASS || echo FAIL) ($failed failed)"
[ $failed = 0 ]
