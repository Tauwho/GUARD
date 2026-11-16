#!/bin/sh
# Spike S5 (#36): does tracelogger capture kernel events on QSTI?
# Run on the target as root. Prints yes/no and event-class counts only, no timing numbers.
# The default QSTI image has tracelogger but not traceprinter: copy traceprinter from
# $QNX_TARGET/x86_64/usr/bin/ to /tmp first (see README).
set -u
OUT=/tmp/s5.kev
TXT=/tmp/s5.txt
rm -f "$OUT" "$TXT"

if ! command -v tracelogger >/dev/null 2>&1; then
    echo "S5 tracelogger present on the image: no"
    exit 1
fi
echo "S5 tracelogger present on the image: yes"
echo "S5 kernel: $(ls /proc/boot | grep procnto)"

TP=traceprinter
command -v traceprinter >/dev/null 2>&1 || TP=/tmp/traceprinter
[ -x "$TP" ] || { echo "S5 traceprinter missing: copy it to /tmp/traceprinter"; exit 1; }

# Capture about 2 s while the shell does some work.
tracelogger -s 2 -f "$OUT" &
TL=$!
i=0; while [ $i -lt 200 ]; do ls / >/dev/null; i=$((i+1)); done
wait "$TL"
echo "tracelogger exit: $?"

if [ ! -s "$OUT" ]; then
    echo "S5 kernel events captured: no (empty or missing $OUT)"
    exit 1
fi

# Count event classes from the stream: the decoded text does not fit in /tmp.
"$TP" -f "$OUT" 2>&1 | awk '
    /KER_CALL/ {kc++} /KER_EXIT/ {ke++} /THREAD/ {th++} / INT/ {it++} /PROCESS/ {pr++}
    END {printf "KER_CALL  lines: %d\nKER_EXIT  lines: %d\nTHREAD    lines: %d\nINT       lines: %d\nPROCESS   lines: %d\n", kc, ke, th, it, pr}
' > "$TXT"
cat "$TXT"
if ! grep -q '^KER_CALL  lines: 0$' "$TXT"; then
    echo "S5 kernel events captured: yes"
else
    echo "S5 kernel events captured: no (no KER_CALL lines)"
fi
# /tmp is the small /data partition: do not leave the trace behind.
rm -f "$OUT" "$TXT"
