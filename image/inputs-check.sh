#!/bin/sh
# image/inputs-check.sh (#141): on the target, check that every normalized input packed by
# image/build-image.sh under /system/guard/scenarios matches its SHA-256 in inputs.json.
#   sh inputs-check.sh [dir]        (default /system/guard/scenarios)
# Prints one CHECK line per file and a RESULT line. Exit 0 when all match.
DIR=${1:-/system/guard/scenarios}
failed=0
total=0
[ -f "$DIR/inputs.json" ] || { echo "RESULT FAIL (no $DIR/inputs.json)"; exit 1; }
# inputs.json lines look like:   "maps/hospital_corridor.yaml": "<64 hex>",
sed -n 's/^ *"\([^"]*\)": "\([0-9a-f]\{64\}\)",\{0,1\}$/\1 \2/p' "$DIR/inputs.json" > /tmp/inputs.list
while read -r path want; do
    total=$((total + 1))
    got=$(openssl dgst -sha256 "$DIR/$path" 2>/dev/null | sed 's/.*= *//')
    if [ "$got" = "$want" ]; then r=PASS; else r=FAIL; failed=$((failed + 1)); fi
    echo "CHECK sha256 $path $r"
done < /tmp/inputs.list
[ "$total" -gt 0 ] || failed=1
echo "RESULT $([ "$failed" = 0 ] && echo PASS || echo FAIL) ($total files, $failed failed)"
[ "$failed" = 0 ]
