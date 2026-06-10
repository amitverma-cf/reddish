#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/benchmarks/results/$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=${REDDISH_BINARY:-"$ROOT_DIR/build/wsl-release/output/reddish"}
KEYS=${KEYS:-100000}
VALUE_BYTES=${VALUE_BYTES:-32}
THREADS=${THREADS:-1}
CLIENTS=${CLIENTS:-10}
PIPELINE=${PIPELINE:-16}
DURATION_SECONDS=${DURATION_SECONDS:-30}
TRIALS=${TRIALS:-5}

mkdir -p "$RESULT_DIR"
mkdir -p "$RESULT_DIR/temp"
printf 'server,workload,trial,ops_per_sec,avg_latency_ms,p50_latency_ms,p99_latency_ms,p999_latency_ms\n' >"$RESULT_DIR/results.csv"

for program in memtier_benchmark redis-server valkey-server; do
  command -v "$program" >/dev/null || {
    echo "Missing $program. Install it before running a fair comparison." >&2
    exit 1
  }
done
test -x "$REDDISH_BINARY" || {
  echo "Missing reddish Release binary: $REDDISH_BINARY" >&2
  exit 1
}

pids=()
cleanup() {
  for pid in "${pids[@]:-}"; do kill -INT "$pid" 2>/dev/null || true; done
  wait "${pids[@]:-}" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

start_server() {
  local server=$1 port=$2
  case "$server" in
    reddish) "$REDDISH_BINARY" "$port" --max-keys "$((KEYS * 2))" --dump-interval 3600 --dump-path "/tmp/reddish-$port.reddish" >"$RESULT_DIR/temp/$server.log" 2>&1 & ;;
    redis) redis-server --port "$port" --save '' --appendonly no >"$RESULT_DIR/temp/$server.log" 2>&1 & ;;
    valkey) valkey-server --port "$port" --save '' --appendonly no >"$RESULT_DIR/temp/$server.log" 2>&1 & ;;
  esac
  pids+=("$!")
  sleep 1
}

run_trial() {
  local server=$1 port=$2 trial=$3 workload=$4 label raw
  case "$workload" in
    0:1) label=reads ;;
    1:0) label=writes ;;
    1:1) label=mixed ;;
  esac
  raw="$RESULT_DIR/temp/$server-$label-$trial.txt"
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol=redis --threads="$THREADS" --clients="$CLIENTS" --test-time="$DURATION_SECONDS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-maximum="$KEYS" --ratio="$workload" --hide-histogram >"$raw"
  awk -v server="$server" -v workload="$label" -v trial="$trial" '/^Totals/ { printf "%s,%s,%s,%s,%s,%s,%s,%s\n", server, workload, trial, $2, $5, $6, $7, $8 }' "$raw" >>"$RESULT_DIR/results.csv"
}

for entry in "reddish:6380" "redis:6381" "valkey:6382"; do
  server=${entry%%:*}; port=${entry##*:}
  start_server "$server" "$port"
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol=redis --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-maximum="$KEYS" --key-pattern=S:S --ratio=1:0 --hide-histogram >"$RESULT_DIR/temp/$server-preload.txt"
  for workload in 0:1 1:0 1:1; do
    for trial in $(seq 1 "$TRIALS"); do run_trial "$server" "$port" "$trial" "$workload"; done
  done
done

awk -F, 'NR == 1 { next } { key = $1 FS $2; ops[key] += $4; p99[key] += $7; count[key]++ } END { print "server,workload,samples,average_ops_per_sec,average_p99_latency_ms"; for (key in count) { split(key, fields, FS); printf "%s,%s,%d,%.2f,%.5f\n", fields[1], fields[2], count[key], ops[key] / count[key], p99[key] / count[key] } }' "$RESULT_DIR/results.csv" | sort >"$RESULT_DIR/summary.csv"
echo "Results: $RESULT_DIR/summary.csv"
