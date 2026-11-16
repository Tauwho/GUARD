#!/bin/sh
# Spike S3 (#33). Run on the QSTI target as root, from the directory holding the binaries.
# Prints yes/no answers only. Capture the whole session with script(1) for the evidence bundle.
set -u
./s3_server &
SERVER=$!
sleep 1
for q in echo read kill pi order; do
    echo "== $q"
    ./s3_client "$q" || echo "(exit $?)"
done
kill "$SERVER"
