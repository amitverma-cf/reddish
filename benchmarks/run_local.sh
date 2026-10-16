#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=${REDDISH_BINARY:-"$ROOT_DIR/build/output/reddish"}
KEYS=${KEYS:-100000}
[[ $KEYS =~ ^[1-9][0-9]*$ && ${#KEYS} -le 9 ]] || { echo 'KEYS must be a positive decimal integer below one billion' >&2; exit 1; }
KEY_MAX=$KEYS
VALUE_BYTES=${VALUE_BYTES:-32}
THREADS=${THREADS:-1}
CLIENTS=${CLIENTS:-10}
PIPELINE=${PIPELINE:-16}
DURATION_SECONDS=${DURATION_SECONDS:-30}
AUXILIARY_DURATION_SECONDS=${AUXILIARY_DURATION_SECONDS:-5}
TRIALS=${TRIALS:-5}
WARMUP_SECONDS=${WARMUP_SECONDS:-2}
MEMORY_TRIALS=${MEMORY_TRIALS:-3}
RUN_EXTENDED=${RUN_EXTENDED:-1}
RUN_VALIDATION=${RUN_VALIDATION:-1}
RUN_PERFORMANCE=${RUN_PERFORMANCE:-1}
RUN_MEMORY=${RUN_MEMORY:-1}
REQUESTS_PER_CLIENT=${REQUESTS_PER_CLIENT:-0}
KEY_PATTERN=${KEY_PATTERN:-S:S}
[[ $KEY_PATTERN == S:S || $KEY_PATTERN == R:R ]] || { echo "Invalid key pattern" >&2; exit 1; }
SERVER_COMMAND=()
CLIENT_COMMAND=()
[[ -z ${SERVER_CPU:-} ]] || SERVER_COMMAND=(taskset -c "$SERVER_CPU")
[[ -z ${CLIENT_CPUS:-} ]] || CLIENT_COMMAND=(taskset -c "$CLIENT_CPUS")
for setting in VALUE_BYTES THREADS CLIENTS PIPELINE DURATION_SECONDS AUXILIARY_DURATION_SECONDS TRIALS MEMORY_TRIALS; do
  value=${!setting}
  [[ $value =~ ^[1-9][0-9]*$ && ${#value} -le 9 ]] || { echo "$setting must be a positive integer" >&2; exit 1; }
done
[[ $WARMUP_SECONDS =~ ^[0-9]+$ ]] || { echo 'Invalid WARMUP_SECONDS' >&2; exit 1; }
[[ $REQUESTS_PER_CLIENT =~ ^(0|[1-9][0-9]*)$ && ${#REQUESTS_PER_CLIENT} -le 9 ]] || { echo 'Invalid REQUESTS_PER_CLIENT' >&2; exit 1; }
for setting in RUN_EXTENDED RUN_PERFORMANCE RUN_MEMORY RUN_VALIDATION; do
  [[ ${!setting} == 0 || ${!setting} == 1 ]] || { echo "$setting must be 0 or 1" >&2; exit 1; }
done
[[ ! -e "$RESULT_DIR/results.csv" ]] || { echo "Refusing to overwrite $RESULT_DIR" >&2; exit 1; }

mkdir -p "$RESULT_DIR/temp"
printf 'server,workload,trial,ops_per_sec,avg_latency_ms,p50_latency_ms,p99_latency_ms,p999_latency_ms\n' >"$RESULT_DIR/results.csv"
printf 'server,scenario,trial,key_count,idle_rss_kib,rss_kib,delta_rss_kib,bytes_per_key\n' >"$RESULT_DIR/memory.csv"

for program in memtier_benchmark redis-server valkey-server redis-cli jq timeout ss; do
  command -v "$program" >/dev/null || { echo "Missing $program" >&2; exit 1; }
done

HAVE_MEMCACHED=0
if [[ ${INCLUDE_MEMCACHED:-0} == 1 ]]; then
  command -v memcached >/dev/null && HAVE_MEMCACHED=1
fi
if [[ ${REQUIRE_MEMCACHED:-0} == 1 && $HAVE_MEMCACHED == 0 ]]; then
  echo 'GitHub comparison requires memcached and INCLUDE_MEMCACHED=1' >&2; exit 1
fi

test -x /usr/bin/time || { echo "Missing /usr/bin/time" >&2; exit 1; }
test -x "$REDDISH_BINARY" || { echo "Missing reddish Release binary: $REDDISH_BINARY" >&2; exit 1; }

if [[ $RUN_VALIDATION == 1 ]]; then
  RESULT_DIR="$RESULT_DIR/validation" bash "$ROOT_DIR/benchmarks/validate.sh"
fi
worktree_state=clean
[[ -z $(git -C "$ROOT_DIR" status --porcelain) ]] || worktree_state=dirty
measured_at=$(date -u '+%Y-%m-%d %H:%M:%S UTC')
platform=Linux
! uname -r | grep -qi microsoft || platform=WSL2
run_url=
if [[ -n ${GITHUB_RUN_ID:-} ]]; then
  platform="${BENCHMARK_ENVIRONMENT:-GitHub Actions / ${RUNNER_OS:-Linux}}"
  run_url="${GITHUB_SERVER_URL:-https://github.com}/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID"
fi
jq -n --arg revision "$(git -C "$ROOT_DIR" rev-parse HEAD)" --arg environment "$platform" --arg run_url "$run_url" --arg measured_at "$measured_at" '{revision:$revision,environment:$environment,run_url:$run_url,measured_at:$measured_at}' > "$RESULT_DIR/provenance.json"
{
  git -C "$ROOT_DIR" rev-parse HEAD
  git -C "$ROOT_DIR" status --short
  git -C "$ROOT_DIR" diff --binary | sha256sum
  sha256sum "$REDDISH_BINARY"
  printf 'Measured at %s\n' "$measured_at"
  printf 'WORKTREE_STATE=%s\n' "$worktree_state"
  uname -a
  lscpu
  memtier_benchmark --version
  redis-server --version
  valkey-server --version
  [[ $HAVE_MEMCACHED == 0 ]] || memcached --version
  printf 'SCHEDSTATS_MODE=%s\n' "$(cat /proc/sys/kernel/sched_schedstats 2>/dev/null || echo unavailable)"
  printf 'SERVER_CPU=%s CLIENT_CPUS=%s\n' "${SERVER_CPU:-unbound}" "${CLIENT_CPUS:-unbound}"
  printf 'REQUESTS_PER_CLIENT=%s KEY_PATTERN=%s\n' "$REQUESTS_PER_CLIENT" "$KEY_PATTERN"
  printf 'RUN_PERFORMANCE=%s RUN_MEMORY=%s RUN_VALIDATION=%s\n' "$RUN_PERFORMANCE" "$RUN_MEMORY" "$RUN_VALIDATION"
  printf 'KEYS=%s VALUE_BYTES=%s THREADS=%s CLIENTS=%s PIPELINE=%s TRIALS=%s DURATION_SECONDS=%s WARMUP_SECONDS=%s MEMORY_TRIALS=%s RUN_EXTENDED=%s AUXILIARY_DURATION_SECONDS=%s persistence=disabled\n' "$KEYS" "$VALUE_BYTES" "$THREADS" "$CLIENTS" "$PIPELINE" "$TRIALS" "$DURATION_SECONDS" "$WARMUP_SECONDS" "$MEMORY_TRIALS" "$RUN_EXTENDED" "$AUXILIARY_DURATION_SECONDS"
} > "$RESULT_DIR/manifest.txt"
printf 'server,workload,trial,workload_seconds,server_cpu_seconds,client_cpu_seconds,client_max_rss_kib,main_runtime_seconds,main_runqueue_seconds,main_blocked_seconds,main_voluntary_switches,main_involuntary_switches,operation_count,client_elapsed_seconds\n' > "$RESULT_DIR/process.csv"
printf 'server,workload,trial,cpu_seconds,operation_count,cpu_us_per_op\n' > "$RESULT_DIR/cpu.csv"
ticks_per_second=$(getconf CLK_TCK)
find "$ROOT_DIR/src" -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 | sort -z | xargs -0 sha256sum > "$RESULT_DIR/source-files.sha256"
find "$ROOT_DIR/benchmarks" -maxdepth 1 -type f -name '*.sh' -print0 | sort -z | xargs -0 sha256sum > "$RESULT_DIR/benchmark-files.sha256"

server_pid=
cleanup() {
  if [[ -n "${server_pid:-}" ]]; then
    kill -INT "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

start_server() {
  local server=$1 port=$2 log=$3
  [[ -z $(ss -H -ltn "sport = :$port") ]] || { echo "Port $port is occupied" >&2; exit 1; }
  case "$server" in
    reddish) "${SERVER_COMMAND[@]}" "$REDDISH_BINARY" "$port" --max-keys "$((KEYS * 16))" --no-wal >"$log" 2>&1 & ;;
    redis) "${SERVER_COMMAND[@]}" redis-server --bind 127.0.0.1 --port "$port" --save '' --appendonly no >"$log" 2>&1 & ;;
    valkey) "${SERVER_COMMAND[@]}" valkey-server --bind 127.0.0.1 --port "$port" --save '' --appendonly no >"$log" 2>&1 & ;;
    memcached) "${SERVER_COMMAND[@]}" memcached -l 127.0.0.1 -p "$port" -U 0 -m 256 -t 1 >"$log" 2>&1 & ;;
  esac
  server_pid=$!
  wait_server "$server_pid" "$port" "$server"
  kill -0 "$server_pid" 2>/dev/null || { cat "$log" >&2; exit 1; }
}

stop_server() {
  kill -INT "$server_pid"
  timeout 20 tail --pid="$server_pid" -f /dev/null
  wait "$server_pid"
  server_pid=
}
rss_kib() {
  for ((sample=0; sample<5; sample++)); do
    awk '/^VmRSS:/{print $2}' "/proc/$server_pid/status"
    sleep .05
  done | sort -n | awk 'NR==3 {print; found=1} END{if(!found)exit 1}'
}

record_memory() {
  local server=$1 scenario=$2 idle=$3 rss delta bytes actual
  actual=$(verify_preload "$server" "$port" "$KEYS")
  rss=$(rss_kib); delta=$((rss - idle)); ((delta < 0)) && delta=0
  bytes=$(awk -v delta="$delta" -v keys="$KEYS" 'BEGIN {printf "%.2f", delta * 1024 / keys}')
  printf '%s,%s,%s,%s,%s,%s,%s,%s\n' "$server" "$scenario" "$memory_trial" "$actual" "$idle" "$rss" "$delta" "$bytes" >>"$RESULT_DIR/memory.csv"
}

protocol_for() {
  [[ $1 == memcached ]] && printf '%s' memcache_binary || printf '%s' redis
}

memtier_base() {
  local server=$1 port=$2
  shift 2
  local -a accounting=()
  [[ -z ${CLIENT_TIME_FILE:-} ]] || accounting=(/usr/bin/time -f '%U,%S,%e,%M' -o "$CLIENT_TIME_FILE")
  "${CLIENT_COMMAND[@]}" "${accounting[@]}" memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" --threads="$THREADS" --clients="$CLIENTS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --hide-histogram "$@"
}

preload_strings() {
  local server=$1 port=$2 output=$3
  "${CLIENT_COMMAND[@]}" memtier_benchmark --server=127.0.0.1 --port="$port" --protocol="$(protocol_for "$server")" --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --key-pattern=S:S --ratio=1:0 --hide-histogram >"$output" 2>"$output.stderr"
}

preload_command() {
  local port=$1 raw=$2; shift 2
  "${CLIENT_COMMAND[@]}" memtier_benchmark --server=127.0.0.1 --port="$port" --protocol=redis --threads=1 --clients=1 --requests="$KEYS" --pipeline="$PIPELINE" --data-size="$VALUE_BYTES" --key-minimum=1 --key-maximum="$KEY_MAX" --hide-histogram "$@" --command-key-pattern=S >"$raw" 2>"$raw.stderr"
}

append_result() {
  local server=$1 workload=$2 trial=$3 raw=$4 expected=${5:-0} before=$6 after=$7
  local values count client_user client_system elapsed max_rss client_elapsed
  count=$(jq -er '.["ALL STATS"].Totals.Count' "$raw.json")
  IFS=, read -r client_user client_system client_elapsed max_rss < "$raw.client-time.csv"
  elapsed=$(jq -er '.["ALL STATS"].Runtime["Total duration"] / 1000' "$raw.json")
  awk -v s="$server" -v w="$workload" -v t="$trial" -v before="$before" -v after="$after" -v hz="$ticks_per_second" -v n="$count" 'BEGIN{split(before,b," ");split(after,a," ");cpu=(a[1]-b[1])/hz;printf "%s,%s,%d,%.6f,%d,%.6f\n",s,w,t,cpu,n,cpu*1000000/n}' >> "$RESULT_DIR/cpu.csv"
  awk -v s="$server" -v w="$workload" -v t="$trial" -v before="$before" -v after="$after" -v hz="$ticks_per_second" -v n="$count" -v cu="$client_user" -v cs="$client_system" -v elapsed="$elapsed" -v rss="$max_rss" -v client_elapsed="$client_elapsed" 'BEGIN{split(before,b," ");split(after,a," ");available=(b[2]>0&&a[2]>=b[2]);runtime=(a[2]-b[2])/1e9;queue=(a[3]-b[3])/1e9;blocked=elapsed-runtime-queue;if(blocked<0)blocked=0;printf "%s,%s,%d,%.3f,%.6f,%.6f,%d,%s,%s,%s,%d,%d,%d,%.3f\n",s,w,t,elapsed,(a[1]-b[1])/hz,cu+cs,rss,(available?sprintf("%.6f",runtime):"NA"),(available?sprintf("%.6f",queue):"NA"),(available?sprintf("%.6f",blocked):"NA"),a[4]-b[4],a[5]-b[5],n,client_elapsed}' >> "$RESULT_DIR/process.csv"
  values=$(read_stats "$raw.json" "$workload" "$expected")
  printf '%s,%s,%s,%s\n' "$server" "$workload" "$trial" "${values%,*}" >> "$RESULT_DIR/results.csv"
}

run_ratio() {
  local server=$1 port=$2 trial=$3 workload=$4 ratio=$5
  local raw="$RESULT_DIR/temp/$server-$workload-$trial.txt"
  local -a limit=(--test-time "$DURATION_SECONDS")
  ((REQUESTS_PER_CLIENT == 0)) || limit=(--requests "$REQUESTS_PER_CLIENT")
  local before after; before=$(sample_process "$server_pid")
  echo "Timing $server $workload trial=$trial" >&2
  CLIENT_TIME_FILE="$raw.client-time.csv" memtier_base "$server" "$port" "${limit[@]}" --key-pattern="$KEY_PATTERN" --ratio="$ratio" --json-out-file="$raw.json" --failed-keys-file="$raw.failed.csv" >"$raw" 2>"$raw.stderr"
  after=$(sample_process "$server_pid")
  append_result "$server" "$workload" "$trial" "$raw" "$((REQUESTS_PER_CLIENT * THREADS * CLIENTS))" "$before" "$after"
}

run_command() {
  local server=$1 port=$2 trial=$3 workload=$4; shift 4
  local raw="$RESULT_DIR/temp/$server-$workload-$trial.txt"
  local before after; before=$(sample_process "$server_pid")
  CLIENT_TIME_FILE="$raw.client-time.csv" memtier_base "$server" "$port" --test-time "$AUXILIARY_DURATION_SECONDS" "$@" --json-out-file="$raw.json" --failed-keys-file="$raw.failed.csv" >"$raw" 2>"$raw.stderr"
  after=$(sample_process "$server_pid")
  append_result "$server" "$workload" "$trial" "$raw" 0 "$before" "$after"
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
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-memory-strings-$memory_trial.log"; idle=$(rss_kib)
  preload_strings "$server" "$port" "$RESULT_DIR/temp/$server-memory-strings-$memory_trial-preload.txt"; record_memory "$server" strings "$idle"; stop_server
  [[ $server == memcached ]] && return
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-memory-lists-$memory_trial.log"; idle=$(rss_kib)
  preload_command "$port" "$RESULT_DIR/temp/$server-memory-lists-$memory_trial-preload.txt" --command='LPUSH list:__key__ __data__'; record_memory "$server" lists "$idle"; stop_server
  start_server "$server" "$port" "$RESULT_DIR/temp/$server-memory-hashes-$memory_trial.log"; idle=$(rss_kib)
  preload_command "$port" "$RESULT_DIR/temp/$server-memory-hashes-$memory_trial-preload.txt" --command='HSET hash:__key__ field __data__'; record_memory "$server" hashes "$idle"; stop_server
}

SERVERS=('reddish:6380' 'redis:6381' 'valkey:6382')
[[ $HAVE_MEMCACHED == 1 ]] && SERVERS+=('memcached:6383')

if [[ $RUN_PERFORMANCE == 1 ]]; then
 for trial in $(seq 1 "$TRIALS"); do
  for offset in "${!SERVERS[@]}"; do
    entry=${SERVERS[$(((trial - 1 + offset) % ${#SERVERS[@]}))]}
    server=${entry%%:*}; port=${entry##*:}
    start_server "$server" "$port" "$RESULT_DIR/temp/$server-performance-$trial.log"
    preload_strings "$server" "$port" "$RESULT_DIR/temp/$server-strings-preload-$trial.txt"
    verify_preload "$server" "$port" "$KEYS" >/dev/null
    if ((WARMUP_SECONDS > 0)); then
      memtier_base "$server" "$port" --test-time "$WARMUP_SECONDS" --key-pattern="$KEY_PATTERN" --ratio=1:1 >"$RESULT_DIR/temp/$server-warmup-$trial.txt" 2>&1
    fi
    if ((trial % 2)); then
      run_ratio "$server" "$port" "$trial" reads 0:1
      run_ratio "$server" "$port" "$trial" writes 1:0
    else
      run_ratio "$server" "$port" "$trial" writes 1:0
      run_ratio "$server" "$port" "$trial" reads 0:1
    fi
    if [[ $server != memcached ]]; then
      run_ratio "$server" "$port" "$trial" mixed 1:1
      if [[ $RUN_EXTENDED == 1 ]]; then
        preload_command "$port" "$RESULT_DIR/temp/$server-lists-preload-$trial.txt" --command='LPUSH len:__key__ __data__'
        preload_command "$port" "$RESULT_DIR/temp/$server-hashes-preload-$trial.txt" --command='HSET hash:__key__ field __data__'
        verify_preload "$server" "$port" "$((KEYS * 3))" >/dev/null
        run_extended "$server" "$port" "$trial"
      fi
    fi
    stop_server
  done
 done
fi
if [[ $RUN_MEMORY == 1 ]]; then
 for memory_trial in $(seq 1 "$MEMORY_TRIALS"); do
  for entry in "${SERVERS[@]}"; do
    server=${entry%%:*}; port=${entry##*:}
    echo "Memory $server trial=$memory_trial" >&2
    measure_memory "$server" "$port"
  done
 done
fi

{
  echo 'server,workload,samples,average_ops_per_sec,average_p99_latency_ms,min_ops_per_sec,max_ops_per_sec,stddev_ops_per_sec'
  awk -F, 'NR > 1 {k=$1 FS $2; ops[k]+=$4; squares[k]+=$4*$4; p99[k]+=$7; count[k]++; if(!(k in lo)||$4<lo[k])lo[k]=$4; if(!(k in hi)||$4>hi[k])hi[k]=$4} END {for(k in count) {split(k,a,FS); mean=ops[k]/count[k]; variance=count[k]>1?(squares[k]-count[k]*mean*mean)/(count[k]-1):0; if(variance<0)variance=0; printf "%s,%s,%d,%.2f,%.5f,%.2f,%.2f,%.2f\n",a[1],a[2],count[k],mean,p99[k]/count[k],lo[k],hi[k],sqrt(variance)}}' "$RESULT_DIR/results.csv" | sort
} >"$RESULT_DIR/summary.csv"
echo "Results: $RESULT_DIR/summary.csv"
echo "Memory: $RESULT_DIR/memory.csv"
{
  echo 'server,scenario,samples,mean_bytes_per_key,min_bytes_per_key,max_bytes_per_key,mean_total_rss_mib'
  awk -F, 'NR>1{k=$1 FS $2;n[k]++;bytes[k]+=$8;rss[k]+=$6;if(!(k in lo)||$8<lo[k])lo[k]=$8;if(!(k in hi)||$8>hi[k])hi[k]=$8} END{for(k in n)printf "%s,%d,%.2f,%.2f,%.2f,%.3f\n",k,n[k],bytes[k]/n[k],lo[k],hi[k],rss[k]/n[k]/1024}' "$RESULT_DIR/memory.csv" | sort
} > "$RESULT_DIR/memory-summary.csv"
RESULTS_DIR="$RESULT_DIR" bash "$ROOT_DIR/benchmarks/publish_results.sh"
