#!/usr/bin/env bash
# Start the remote planner on an already booted develop image (host/ADVISOR.md).
set -eu
if [ "${1:-}" = "--help" ]; then
    printf '%s\n' 'Usage: bash demo/remote-advisor.sh' 'Requires: warm Ollama advisor on host, develop QNX image, ssh alias guard-target.'
    exit 0
fi
ssh guard-target 'slay -f guard-planner; on -d /system/bin/guard-planner --remote 10.0.2.2:7701 > /tmp/planner.log 2>&1'
