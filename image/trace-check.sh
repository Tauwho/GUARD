#!/bin/sh
# image/trace-check.sh (B11, #48): tracelogger timeline of one lease, on the test image as root.
#   sh trace-check.sh
# Runs F0, records 5 s with tracelogger while F2 (guard-hog) makes the lease expire,
# decodes the .kev with traceprinter on the target and prints the GUARD user events in order:
# proposal -> decision -> lease -> expiry. The .kev stays in /tmp and is never committed.
# No timestamps are printed (D11).
KEV=/tmp/guard.kev
rm -f $KEV
guardctl scenario stop > /dev/null 2>&1
guardctl scenario load /system/guard/scenarios/F0.yaml --seed 1000 > /dev/null || exit 1
guardctl scenario start > /dev/null || exit 1
sleep 3
tracelogger -n 0 -s 5 -f $KEV > /tmp/tracelogger.log 2>&1 &
sleep 2
# F2: guard-hog at 32 on every CPU starves the supervisor (30); the gate (35) expires the lease.
guardctl inject F2 --duration-ms 400 > /dev/null
wait
guardctl scenario stop > /dev/null
[ -s $KEV ] || { echo "RESULT FAIL (no $KEV)"; exit 1; }
# Keep only GUARD user events; drop the time columns.
# traceprinter prints each CPU buffer in turn: sort on the (hex, padded) time stamp, then
# keep only the event text.
traceprinter -f $KEV 2>/dev/null | grep USREVENT \
    | awk '{ t = substr($1, 5); printf "%24s %s\n", t, $0 }' | sort | cut -c26- \
    | sed -n 's/.*STR:"\([^"]*\)".*/\1/p' > /tmp/guard-timeline.txt
for k in proposal decision lease expiry; do
    echo "events $k: $(grep -c "^$k" /tmp/guard-timeline.txt)"
done
echo "first steps of the timeline:"
head -8 /tmp/guard-timeline.txt | sed 's/^/  /'
echo "last steps of the timeline:"
tail -4 /tmp/guard-timeline.txt | sed 's/^/  /'
# One full chain: a proposal, its decision and lease (the gate installs the lease while the
# supervisor is still answering, so "lease" can come just before "decision"), then an expiry.
# The chain is followed by ids, not by time stamps: on the QEMU target the per-CPU stamps of
# two processes do not sort reliably. A lease the gate installed (lease L) comes from the
# decision that names it (decision N ... lease L), which answers proposal N.
L=$(sed -n 's/^lease \([0-9]*\)$/\1/p' /tmp/guard-timeline.txt | tail -1)
D=$(grep "^decision .* verdict 1 .* lease $L\$" /tmp/guard-timeline.txt | tail -1)
N=$(echo "$D" | sed -n 's/^decision \([0-9]*\) .*/\1/p')
P=$(grep "^proposal $N\$" /tmp/guard-timeline.txt | tail -1)
E=$(grep "^expiry" /tmp/guard-timeline.txt | head -1)
echo "one lease, step by step:"
for step in "$P" "$D" "lease $L" "$E"; do echo "  ${step:-<missing>}"; done
if [ -n "$L" ] && [ -n "$D" ] && [ -n "$P" ] && [ -n "$E" ]; then
    echo "RESULT PASS (timeline shows proposal -> decision -> lease -> expiry)"
else
    echo "RESULT FAIL (proposal $p decision $d lease $l expiry $e)"
    exit 1
fi
