#!/usr/bin/env bash
# Fresh-clone demo test (P). Clones a ref into a temp dir and runs the README host quick start.
# Usage: bash tools/fresh_clone_demo.sh [ref]   (default: main)
# Passes only if a newcomer could run the host demo with the documented commands.
# Needs GNU coreutils `timeout`; each step is bounded so a hang fails instead of stalling CI.
set -euo pipefail
REF="${1:-main}"
URL="${GUARD_REPO_URL:-https://github.com/Tauwho/GUARD.git}"
WORK="$(mktemp -d)"
RELAY_PID=""
cleanup() {
  # Only the relay this script started; never `kill 0` (that is the caller's process group).
  if [[ -n "$RELAY_PID" ]] && kill -0 "$RELAY_PID" 2>/dev/null; then
    kill "$RELAY_PID" 2>/dev/null || true
    wait "$RELAY_PID" 2>/dev/null || true
  fi
  rm -rf -- "$WORK"
}
trap cleanup EXIT
step() { echo "== $*"; }

step "clone $REF"
timeout 300 git clone -q -- "$URL" "$WORK/guard"
cd "$WORK/guard"
timeout 60 git checkout -q --detach "$REF"

step "python deps (isolated venv)"
timeout 120 python3 -m venv "$WORK/venv"
PY="$WORK/venv/bin/python"
timeout 300 "$PY" -m pip install -q PyYAML==6.0.3 jsonschema==4.26.0 websockets==17.3
export PATH="$WORK/venv/bin:$PATH"

step "host build and tests"
timeout 300 cmake --preset host-debug >/dev/null
timeout 1200 cmake --build --preset host-debug --parallel 4 >/dev/null
timeout 1200 ctest --preset host-debug

step "F0 and F3 replay (expectations must hold)"
mkdir -p out
timeout 300 "$PY" host/guard-replay --scenario scenarios/F0.yaml --mode B2 --out out/F0.jsonl
timeout 300 "$PY" host/guard-replay --scenario scenarios/F3.yaml --mode B2 --out out/F3.jsonl

step "one-seed B2 matrix (mismatches are reported, not fatal)"
status=0
timeout 900 "$PY" eval/run_matrix.py --seeds 1 --modes B2 --out out/matrix || status=$?
if [[ $status -eq 124 ]]; then
  echo "matrix timed out" >&2
  exit 124
elif [[ $status -ne 0 ]]; then
  echo "matrix reported mismatches (see out/matrix/results.json)"
fi
test -s out/matrix/results.json

step "dashboard relay headless smoke"
# A free port pair (HTTP on PORT, WebSocket on PORT+1), so another listener cannot answer for us.
PORT="$("$PY" -c '
import socket
for _ in range(50):
    with socket.socket() as a, socket.socket() as b:
        a.bind(("127.0.0.1", 0))
        port = a.getsockname()[1]
        try:
            b.bind(("127.0.0.1", port + 1))
        except OSError:
            continue
        print(port)
        break
else:
    raise SystemExit("no free port pair")
')"
"$PY" dashboard/relay.py --trace out/F3.jsonl --port "$PORT" >out/relay-smoke.log 2>&1 &
RELAY_PID=$!
ready=0
for _ in $(seq 1 40); do
  kill -0 "$RELAY_PID" 2>/dev/null || break
  if "$PY" -c "import urllib.request,sys; sys.exit(0 if urllib.request.urlopen('http://127.0.0.1:$PORT/meta.json', timeout=1).status == 200 else 1)" 2>/dev/null; then
    ready=1
    break
  fi
  sleep 0.5
done
if [[ $ready -ne 1 ]] || ! kill -0 "$RELAY_PID" 2>/dev/null; then
  echo "relay did not serve /meta.json; last log lines:" >&2
  tail -n 40 out/relay-smoke.log >&2 || true
  exit 1
fi
echo "relay serves /meta.json on port $PORT"

echo "FRESH-CLONE DEMO: PASS ($REF @ $(git rev-parse --short HEAD))"
