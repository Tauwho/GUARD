#!/usr/bin/env bash
# Fresh-clone demo test (P). Clones a ref into a temp dir and runs the README host quick start.
# Usage: bash tools/fresh_clone_demo.sh [ref]   (default: main)
# Passes only if a newcomer could run the host demo with the documented commands.
set -euo pipefail
REF="${1:-main}"
URL="${GUARD_REPO_URL:-https://github.com/Tauwho/GUARD.git}"
WORK="$(mktemp -d)"
trap 'kill ${RELAY_PID:-0} 2>/dev/null || true; rm -rf "$WORK"' EXIT
step() { echo "== $*"; }

step "clone $REF"
git clone -q "$URL" "$WORK/guard"
cd "$WORK/guard"
git checkout -q "$REF"

step "python deps"
python3 -m pip install -q PyYAML==6.0.3 jsonschema==4.26.0 websockets==17.3

step "host build and tests"
cmake --preset host-debug >/dev/null
cmake --build --preset host-debug --parallel 4 >/dev/null
ctest --preset host-debug

step "F0 and F3 replay (expectations must hold)"
mkdir -p out
python3 host/guard-replay --scenario scenarios/F0.yaml --mode B2 --out out/F0.jsonl
python3 host/guard-replay --scenario scenarios/F3.yaml --mode B2 --out out/F3.jsonl

step "one-seed B2 matrix (mismatches are reported, not fatal)"
python3 eval/run_matrix.py --seeds 1 --modes B2 --out out/matrix || echo "matrix reported mismatches (see out/matrix/results.json)"
test -s out/matrix/results.json

step "dashboard relay headless smoke"
python3 dashboard/relay.py --trace out/F3.jsonl --port 18765 &
RELAY_PID=$!
for _ in $(seq 1 20); do
  if python3 -c "import urllib.request,sys; sys.exit(0 if urllib.request.urlopen('http://127.0.0.1:18765/meta.json', timeout=1).status == 200 else 1)" 2>/dev/null; then
    echo "relay serves /meta.json"; break
  fi
  sleep 0.5
done
python3 -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:18765/meta.json', timeout=2)"

echo "FRESH-CLONE DEMO: PASS ($REF @ $(git rev-parse --short HEAD))"
