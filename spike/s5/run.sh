#!/bin/sh
# Spike S5 (#36): does tracelogger capture kernel events on QSTI?
# Run on the target as root. Prints yes/no and event-class counts only, no timing numbers.
# Options marked (verify) are unchecked against the SDP 8.0 docs.
set -u
OUT=/tmp/s5.kev
rm -f "$OUT"

if ! command -v tracelogger >/dev/null 2>&1; then
    echo "S5 tracelogger present on the image: no"
    exit 1
fi
echo "S5 tracelogger present on the image: yes"

# Capture about 2 s while the shell does some work. (verify) -s seconds, -f output file
tracelogger -s 2 -f "$OUT" &
TL=$!
i=0; while [ $i -lt 200 ]; do ls / >/dev/null; i=$((i+1)); done
wait "$TL"
echo "tracelogger exit: $?"

if [ ! -s "$OUT" ]; then
    echo "S5 kernel events captured: no (empty or missing $OUT)"
    echo "Hint: QSTI may boot a non-instrumented procnto. Check: ls /proc/boot | grep procnto"
    exit 1
fi

# (verify) traceprinter -f <file>
traceprinter -f "$OUT" > /tmp/s5.txt 2>&1
for class in KER_CALL KER_EXIT THREAD INT PROCESS; do
    printf '%-9s lines: %s\n' "$class" "$(grep -c "$class" /tmp/s5.txt)"
done
if grep -q KER_CALL /tmp/s5.txt; then
    echo "S5 kernel events captured: yes"
else
    echo "S5 kernel events captured: no (no KER_CALL lines)"
fi
