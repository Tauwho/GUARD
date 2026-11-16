#!/bin/sh
# Spike S3 (#33). Run on the QSTI target as root, from the directory holding the binaries.
# Prints yes/no answers only. Capture the whole session with script(1) for the evidence bundle.
# Pass 1: default resmgr unblock handling. Pass 2: server answers unblock pulses (-u).
set -u
for opt in "" "-u"; do
    echo "######## s3_server $opt"
    ./s3_server $opt &
    SERVER=$!
    sleep 1
    for q in echo read kill pi order slow; do
        echo "== $q"
        ./s3_client "$q" || echo "(exit $?)"
    done
    kill "$SERVER"
    sleep 1
done
