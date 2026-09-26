#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/benchmarks/results/$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=${REDDISH_BINARY:-"$ROOT_DIR/build/wsl-release/output/reddish"}
KEYS=${KEYS:-100000}
[[ $KEYS =~ ^[1-9][0-9]*$ && ${#KEYS} -le 9 ]] || { echo 'KEYS must be a positive decimal integer below one billion' >&2; exit 1; }
# Explicit memtier minima must be positive: exactly KEYS keys are 1..KEYS.
KEY_MAX=$KEYS
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

for program in memtier_benchmark redis-server valkey-server; do
  command -v "$program" >/dev/null || { echo "Missing $program" >&2; exit 1; }
done

# Local runs never launch memcached unless explicitly opted in. GitHub requires it.
HAVE_MEMCACHED=0
if [[ ${INCLUDE_MEMCACHED:-0} == 1 ]]; then
  command -v memcached >/dev/null && HAVE_MEMCACHED=1
fi
if [[ ${REQUIRE_MEMCACHED:-0} == 1 && $HAVE_MEMCACHED == 0 ]]; then
  echo 'GitHub comparison requires memcached and INCLUDE_MEMCACHED=1' >&2; exit 1
fi

test -x "$REDDISH_BINARY" || { echo "Missing reddish Release binary: $REDDISH_BINARY" >&2; exit 1; }

{
  git -C "$ROOT_DIR" rev-parse HEAD
  git -C "$ROOT_DIR" status --short
  git -C "$ROOT_DIR" diff --binary | sha256sum
  sha256sum "$REDDISH_BINARY"
  uname -a
  lscpu
  memtier_benchmark --version
  redis-server --version
  valkey-server --version
  printf 'KEYS=%s VALUE_BYTES=%s THREADS=%s CLIENTS=%s PIPELINE=%s TRIALS=%s persistence=disabled\n' "$KEYS" "$VALUE_BYTES" "$THREADS" "$CLIENTS" "$PIPELINE" "$TRIALS"
} > "$RESULT_DIR/manifest.txt"
printf 'server,workload,trial,cpu_seconds,operation_count,cpu_us_per_op\n' > "$RESULT_DIR/cpu.csv"
cpu_ticks() { awk '{print $14 + $15}' "/proc/$server_pid/stat"; }
ticks_per_second=$(getconf CLK_TCK)
find "$ROOT_DIR/src" -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 | sort -z | xargs -0 sha256sum > "$RESULT_DIR/source-files.sha256"

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
    memcached) memcached -l 127.0.0.1 -p "$port" -U 0 -m 256 >"$log" 2>&1 & ;;
  esac
  server_pid=$!
  sleep 1
  kill -0 "$server_pid" 2>/dev/null || { cat "$log" >&2; exit 1; }
}

stop_server() { cleanup; server_pid=; }
rss_kib() { awk '/VmRSS:/ {print $2; exit}' "/proc/$server_pid/status"; }

record_memory() {
  local server=$1 scenario=$2 idle=$3 rss delta bytes actual
  actual=$(python3 "$ROOT_DIR/benchmarks/verify_preload.py" "$server" "$port" "$KEYS")
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
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" --threads="$THREADS" --clients="$CLIENTS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --hide-histogram "$@"
}

preload_strings() {
  local server=$1 port=$2 output=$3
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --key-pattern=S:S --ratio=1:0 --hide-histogram >"$output" 2>"$output.stderr"
}

preload_command() {
  local port=$1 raw=$2; shift 2
  # Sequential keys matter twice over: the extended workloads address keys with
  # --command-key-pattern=S, so a random preload leaves about 37% of their lookups
  # pointing at keys that were never created, and the memory measurement divides
  # by KEYS while a random preload only creates about 63% of them. The key-pattern
  # flag trails the --command it applies to, which is how memtier expects it.
  memtier_benchmark --server=127.0.0.1 --port="$port" --protocol=redis --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --hide-histogram "$@" --command-key-pattern=S >"$raw" 2>"$raw.stderr"
}

append_result() {
  local server=$1 workload=$2 trial=$3 raw=$4
  python3 "$ROOT_DIR/benchmarks/read_memtier.py" "$raw.json" "$server" "$workload" "$trial" >>"$RESULT_DIR/results.csv"
}

run_ratio() {
  local server=$1 port=$2 trial=$3 workload=$4 ratio=$5
  local raw="$RESULT_DIR/temp/$server-$workload-$trial.txt"
  local before; before=$(cpu_ticks)
  memtier_base "$server" "$port" --test-time "$DURATION_SECONDS" --key-pattern=S:S --ratio="$ratio" --json-out-file="$raw.json" --failed-keys-file="$raw.failed.csv" >"$raw" 2>"$raw.stderr"
  append_result "$server" "$workload" "$trial" "$raw"
  python3 "$ROOT_DIR/benchmarks/read_memtier.py" "$raw.json" --cpu "$server" "$workload" "$trial" "$before" "$(cpu_ticks)" "$ticks_per_second" >> "$RESULT_DIR/cpu.csv"
}

run_command() {
  local server=$1 port=$2 trial=$3 workload=$4; shift 4
  local raw="$RESULT_DIR/temp/$server-$workload-$trial.txt"
  local before; before=$(cpu_ticks)
  memtier_base "$server" "$port" --test-time "$AUXILIARY_DURATION_SECONDS" "$@" --json-out-file="$raw.json" --failed-keys-file="$raw.failed.csv" >"$raw" 2>"$raw.stderr"
  append_result "$server" "$workload" "$trial" "$raw"
  python3 "$ROOT_DIR/benchmarks/read_memtier.py" "$raw.json" --cpu "$server" "$workload" "$trial" "$before" "$(cpu_ticks)" "$ticks_per_second" >> "$RESULT_DIR/cpu.csv"
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

SERVERS=('reddish:6380' 'redis:6381' 'valkey:6382')
[[ $HAVE_MEMCACHED == 1 ]] && SERVERS+=('memcached:6383')

for entry in "${SERVERS[@]}"; do
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
  echo 'server,workload,samples,average_ops_per_sec,average_p99_latency_ms,min_ops_per_sec,max_ops_per_sec,stddev_ops_per_sec'
  awk -F, 'NR > 1 {k=$1 FS $2; ops[k]+=$4; squares[k]+=$4*$4; p99[k]+=$7; count[k]++; if(!(k in lo)||$4<lo[k])lo[k]=$4; if(!(k in hi)||$4>hi[k])hi[k]=$4} END {for(k in count) {split(k,a,FS); mean=ops[k]/count[k]; variance=count[k]>1?(squares[k]-count[k]*mean*mean)/(count[k]-1):0; if(variance<0)variance=0; printf "%s,%s,%d,%.2f,%.5f,%.2f,%.2f,%.2f\n",a[1],a[2],count[k],mean,p99[k]/count[k],lo[k],hi[k],sqrt(variance)}}' "$RESULT_DIR/results.csv" | sort
} >"$RESULT_DIR/summary.csv"
echo "Results: $RESULT_DIR/summary.csv"
echo "Memory: $RESULT_DIR/memory.csv"
