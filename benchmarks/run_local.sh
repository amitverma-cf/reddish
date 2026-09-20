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
AUXILIARY_DURATION_SECONDS=${AUXILIARY_DURATION_SECONDS:-5}
TRIALS=${TRIALS:-5}

mkdir -p "$RESULT_DIR/temp"
printf 'server,workload,trial,ops_per_sec,avg_latency_ms,p50_latency_ms,p99_latency_ms,p999_latency_ms\n' >"$RESULT_DIR/results.csv"
printf 'server,scenario,key_count,idle_rss_kib,rss_kib,delta_rss_kib,bytes_per_key\n' >"$RESULT_DIR/memory.csv"

for program in memtier_benchmark redis-server valkey-server memcached; do
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
    reddish) "$REDDISH_BINARY" "$port" --max-keys "$((KEYS * 4))" --dump-interval 3600 --dump-path "/tmp/reddish-$port.reddish" >"$log" 2>&1 & ;;
    redis) redis-server --port "$port" --save '' --appendonly no >"$log" 2>&1 & ;;
    valkey) valkey-server --port "$port" --save '' --appendonly no >"$log" 2>&1 & ;;
    memcached) memcached -l 127.0.0.1 -p "$port" -U 0 >"$log" 2>&1 & ;;
  esac
  server_pid=$!
  sleep 1
  kill -0 "$server_pid" 2>/dev/null || { cat "$log" >&2; exit 1; }
}

stop_server() { cleanup; server_pid=; }
rss_kib() { awk '/VmRSS:/ {print $2; exit}' "/proc/$server_pid/status"; }

record_memory() {
  local server=$1 scenario=$2 idle=$3 rss delta bytes
  rss=$(rss_kib); delta=$((rss - idle)); ((delta < 0)) && delta=0
  bytes=$(awk -v delta="$delta" -v keys="$KEYS" 'BEGIN {printf "%.2f", delta * 1024 / keys}')
  printf '%s,%s,%s,%s,%s,%s,%s\n' "$server" "$scenario" "$KEYS" "$idle" "$rss" "$delta" "$bytes" >>"$RESULT_DIR/memory.csv"
}

protocol_for() {
  [[ $1 == memcached ]] && printf '%s' memcache_binary || printf '%s' redis
}

memtier_base() {
  local server=$1 port=$2
  shift 2
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" --threads="$THREADS" --clients="$CLIENTS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-maximum="$KEYS" --hide-histogram "$@"
}

preload_strings() {
  local server=$1 port=$2 output=$3
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-maximum="$KEYS" --key-pattern=S:S --ratio=1:0 --hide-histogram >"$output"
}

preload_command() {
  local port=$1 raw=$2; shift 2
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol=redis --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-maximum="$KEYS" --hide-histogram "$@" >"$raw"
}

append_result() {
  local server=$1 workload=$2 trial=$3 raw=$4
  awk -v server="$server" -v workload="$workload" -v trial="$trial" '/^Totals/ {printf "%s,%s,%s,%s,%s,%s,%s,%s\n",server,workload,trial,$2,$5,$6,$7,$8}' "$raw" >>"$RESULT_DIR/results.csv"
}

run_ratio() {
  local server=$1 port=$2 trial=$3 workload=$4 ratio=$5
  local raw="$RESULT_DIR/temp/$server-$workload-$trial.txt"
  memtier_base "$server" "$port" --test-time "$DURATION_SECONDS" --key-pattern=S:S --ratio="$ratio" >"$raw"
  append_result "$server" "$workload" "$trial" "$raw"
}

run_command() {
  local server=$1 port=$2 trial=$3 workload=$4; shift 4
  local raw="$RESULT_DIR/temp/$server-$workload-$trial.txt"
  memtier_base "$server" "$port" --test-time "$AUXILIARY_DURATION_SECONDS" "$@" >"$raw"
  append_result "$server" "$workload" "$trial" "$raw"
}

run_extended() {
  local server=$1 port=$2 trial=$3
  run_command "$server" "$port" "$trial" ping --command='PING'
  run_command "$server" "$port" "$trial" incr --command='INCR incr:__key__' --command-key-pattern=S
  run_command "$server" "$port" "$trial" set_del --transaction --command='SET del:__key__ __data__' --command-key-pattern=S --command='DEL del:__key__' --command-key-pattern=S
  run_command "$server" "$port" "$trial" lpush_lpop --transaction --command='LPUSH left:__key__ __data__' --command-key-pattern=S --command='LPOP left:__key__' --command-key-pattern=S
  run_command "$server" "$port" "$trial" rpush_rpop --transaction --command='RPUSH right:__key__ __data__' --command-key-pattern=S --command='RPOP right:__key__' --command-key-pattern=S
  run_command "$server" "$port" "$trial" llen --command='LLEN len:__key__' --command-key-pattern=S
  run_command "$server" "$port" "$trial" hget --command='HGET hash:__key__ field' --command-key-pattern=S
  run_command "$server" "$port" "$trial" hlen --command='HLEN hash:__key__' --command-key-pattern=S
  run_command "$server" "$port" "$trial" hset_hdel --transaction --command='HSET churn:__key__ field __data__' --command-key-pattern=S --command='HDEL churn:__key__ field' --command-key-pattern=S
  run_command "$server" "$port" "$trial" expire_ttl --transaction --command='SET ttl:__key__ __data__' --command-key-pattern=S --command='EXPIRE ttl:__key__ 60' --command-key-pattern=S --command='TTL ttl:__key__' --command-key-pattern=S
}

measure_memory() {
  local server=$1 port=$2 idle
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-memory-strings.log"; idle=$(rss_kib)
  preload_strings "$server" "$port" "$RESULT_DIR/temp/$server-memory-strings-preload.txt"; record_memory "$server" strings "$idle"; stop_server
  [[ $server == memcached ]] && return
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-memory-lists.log"; idle=$(rss_kib)
  preload_command "$port" "$RESULT_DIR/temp/$server-memory-lists-preload.txt" --command='LPUSH list:__key__ __data__'; record_memory "$server" lists "$idle"; stop_server
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-memory-hashes.log"; idle=$(rss_kib)
  preload_command "$port" "$RESULT_DIR/temp/$server-memory-hashes-preload.txt" --command='HSET hash:__key__ field __data__'; record_memory "$server" hashes "$idle"; stop_server
}

for entry in 'reddish:6380' 'redis:6381' 'valkey:6382' 'memcached:6383'; do
  server=${entry%%:*}; port=${entry##*:}
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-performance.log"
  preload_strings "$server" "$port" "$RESULT_DIR/temp/$server-strings-preload.txt"
  if [[ $server != memcached ]]; then
    preload_command "$port" "$RESULT_DIR/temp/$server-lists-preload.txt" --command='LPUSH len:__key__ __data__'
    preload_command "$port" "$RESULT_DIR/temp/$server-hashes-preload.txt" --command='HSET hash:__key__ field __data__'
  fi
  for trial in $(seq 1 "$TRIALS"); do
    run_ratio "$server" "$port" "$trial" reads 0:1
    run_ratio "$server" "$port" "$trial" writes 1:0
    if [[ $server != memcached ]]; then
      run_ratio "$server" "$port" "$trial" mixed 1:1
      run_extended "$server" "$port" "$trial"
    fi
  done
  stop_server
  measure_memory "$server" "$port"
done

{
  echo 'server,workload,samples,average_ops_per_sec,average_p99_latency_ms'
  awk -F, 'NR > 1 {k=$1 FS $2; ops[k]+=$4; p99[k]+=$7; count[k]++} END {for(k in count) {split(k,a,FS); printf "%s,%s,%d,%.2f,%.5f\n",a[1],a[2],count[k],ops[k]/count[k],p99[k]/count[k]}}' "$RESULT_DIR/results.csv" | sort
} >"$RESULT_DIR/summary.csv"
echo "Results: $RESULT_DIR/summary.csv"
echo "Memory: $RESULT_DIR/memory.csv"
