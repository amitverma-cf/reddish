#!/usr/bin/env bash
# Measures resident memory per key after equivalent string, list and hash preloads.
# Uses sequential preloads and validates actual live key count.
#
# Usage:
#   bash benchmarks/measure_memory.sh
#   KEYS=200000 VALUE_BYTES=64 bash benchmarks/measure_memory.sh
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/benchmarks/results/memory-$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=${REDDISH_BINARY:-"$ROOT_DIR/build/wsl-release/output/reddish"}
KEYS=${KEYS:-100000}
[[ $KEYS =~ ^[1-9][0-9]*$ && ${#KEYS} -le 9 ]] || { echo 'KEYS must be a positive decimal integer below one billion' >&2; exit 1; }
# Explicit memtier minima must be positive: exactly KEYS keys are 1..KEYS.
KEY_MAX=$KEYS
VALUE_BYTES=${VALUE_BYTES:-32}
PIPELINE=${PIPELINE:-16}

mkdir -p "$RESULT_DIR/temp"
printf 'server,shape,key_count,idle_rss_kib,rss_kib,delta_rss_kib,bytes_per_key\n' >"$RESULT_DIR/memory.csv"

for program in memtier_benchmark redis-server valkey-server; do
  command -v "$program" >/dev/null || { echo "Missing $program" >&2; exit 1; }
done
test -x "$REDDISH_BINARY" || { echo "Missing reddish Release binary: $REDDISH_BINARY" >&2; exit 1; }

server_pid=
cleanup() {
  if [[ -n "${server_pid:-}" ]]; then
    kill -INT "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

start_server() {
  local server=$1 port=$2 log=$3
  case "$server" in
    reddish) "$REDDISH_BINARY" "$port" --max-keys "$((KEYS * 16))" --no-wal >"$log" 2>&1 & ;;
    redis) redis-server --port "$port" --save '' --appendonly no >"$log" 2>&1 & ;;
    valkey) valkey-server --port "$port" --save '' --appendonly no >"$log" 2>&1 & ;;
  esac
  server_pid=$!
  sleep 1
  kill -0 "$server_pid" 2>/dev/null || { cat "$log" >&2; exit 1; }
}

stop_server() { cleanup; server_pid=; }
rss_kib() { awk '/VmRSS:/ {print $2; exit}' "/proc/$server_pid/status"; }

protocol_for() {
  [[ $1 == memcached ]] && printf '%s' memcache_binary || printf '%s' redis
}

# Sequential key generation removes the birthday-collision bias: KEYS requests
# create exactly KEYS distinct keys, which is what bytes_per_key assumes.
#
# Key-pattern flags differ per shape and are supplied by the caller: memtier
# rejects --key-pattern together with --command (the latter is configured with
# --command-key-pattern instead).
preload() {
  local server=$1 port=$2 raw=$3
  shift 3
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" \
    --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" \
    --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --hide-histogram \
    "$@" >"$raw"
}

measure_one() {
  local server=$1 port=$2 shape=$3
  shift 3
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-$shape.log"
  local idle
  idle=$(rss_kib)
  preload "$server" "$port" "$RESULT_DIR/temp/$server-$shape-preload.txt" "$@"
  local rss delta bytes
  python3 "$ROOT_DIR/benchmarks/verify_preload.py" "$server" "$port" "$KEYS" >/dev/null
  rss=$(rss_kib)
  delta=$((rss - idle))
  ((delta < 0)) && delta=0
  bytes=$(awk -v delta="$delta" -v keys="$KEYS" 'BEGIN {printf "%.2f", delta * 1024 / keys}')
  printf '%s\n' "$server,$shape,$KEYS,$idle,$rss,$delta,$bytes" >>"$RESULT_DIR/memory.csv"
  printf '%-8s %-8s delta=%6s KiB  bytes/key=%s\n' "$server" "$shape" "$delta" "$bytes"
  stop_server
}

port_for() {
  case "$1" in
    reddish) printf '%s' 6480 ;;
    redis) printf '%s' 6481 ;;
    valkey) printf '%s' 6482 ;;
  esac
}

for server in reddish redis valkey; do
  port=$(port_for "$server")
  measure_one "$server" "$port" strings --ratio=1:0 --key-pattern=S:S
  # With --command, memtier drives the rotation itself: --ratio must not be given,
  # and --command has to precede the modifiers that apply to it.
  measure_one "$server" "$port" lists \
    --command='LPUSH list:__key__ __data__' --command-key-pattern=S
  measure_one "$server" "$port" hashes \
    --command='HSET hash:__key__ field __data__' --command-key-pattern=S
done

echo "Memory: $RESULT_DIR/memory.csv"
