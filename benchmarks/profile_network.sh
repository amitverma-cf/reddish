#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
BINARY=$(realpath "${1:?Pass the Reddish executable}")
RESULT_DIR=${2:?Pass a new output directory}
KEYS=${KEYS:-100000}
REQUESTS_PER_CLIENT=${REQUESTS_PER_CLIENT:-10000}
PORT=${PORT:-6395}
RATIO=${RATIO:-1:1}
[[ $KEYS =~ ^[1-9][0-9]*$ && $REQUESTS_PER_CLIENT =~ ^[1-9][0-9]*$ && $PORT =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid keys, requests, or port' >&2; exit 1; }
[[ $RATIO == 1:1 || $RATIO == 1:0 || $RATIO == 0:1 ]] || { echo 'Invalid ratio' >&2; exit 1; }
[[ ! -e "$RESULT_DIR/tcp.callgrind" && -z $(ss -H -ltn "sport = :$PORT") ]] || { echo 'Output exists or port is occupied' >&2; exit 1; }
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
valgrind --tool=callgrind --callgrind-out-file="$RESULT_DIR/tcp.callgrind" "$BINARY" "$PORT" --no-wal --max-keys "$((KEYS*2))" > "$RESULT_DIR/server.log" 2>&1 &
pid=$!
trap '[[ -z ${pid:-} ]] || { kill -KILL "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
wait_server "$pid" "$PORT"
base=(memtier_benchmark --server=127.0.0.1 --port="$PORT" --threads=1 --clients=10 --pipeline=16 --data-size=32 --key-minimum=1 --key-maximum="$KEYS" --key-pattern=S:S --hide-histogram)
"${base[@]}" --clients=1 --requests="$KEYS" --ratio=1:0 > "$RESULT_DIR/preload.log" 2>&1
verify_preload reddish "$PORT" "$KEYS" >/dev/null
callgrind_control --zero "$pid" >> "$RESULT_DIR/server.log" 2>&1
"${base[@]}" --requests="$REQUESTS_PER_CLIENT" --ratio="$RATIO" --json-out-file="$RESULT_DIR/tcp.json" --failed-keys-file="$RESULT_DIR/tcp.failed.csv" > "$RESULT_DIR/tcp.txt" 2> "$RESULT_DIR/tcp.stderr"
values=$(read_stats "$RESULT_DIR/tcp.json" mixed "$((REQUESTS_PER_CLIENT*10))")
callgrind_control --dump=measured-commands "$pid" >> "$RESULT_DIR/server.log" 2>&1
kill -INT "$pid"
timeout 30 tail --pid="$pid" -f /dev/null
wait "$pid"; pid=
measured=$(find "$RESULT_DIR" -maxdepth 1 -name 'tcp.callgrind.*' | sort -V | tail -1)
[[ -n $measured ]] || { echo 'No measured Callgrind dump' >&2; exit 1; }
callgrind_annotate --inclusive=no --auto=no --threshold=100 "$measured" > "$RESULT_DIR/self.txt"
callgrind_annotate --inclusive=yes --auto=no --threshold=100 "$measured" > "$RESULT_DIR/inclusive.txt"
{
  git -C "$ROOT_DIR" rev-parse HEAD
  sha256sum "$BINARY"
  printf 'KEYS=%s REQUESTS_PER_CLIENT=%s CLIENTS=10 PIPELINE=16 RATIO=%s\n' "$KEYS" "$REQUESTS_PER_CLIENT" "$RATIO"
  printf 'Instrumented TCP commands; throughput is not comparable to native measurements.\n%s\n' "$values"
} > "$RESULT_DIR/manifest.txt"
printf 'TCP profile: %s\n' "$RESULT_DIR"
