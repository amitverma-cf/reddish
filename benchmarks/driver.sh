#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/driver-$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=$(realpath "${REDDISH_BINARY:-$ROOT_DIR/build/output/reddish}")
TRIALS=${TRIALS:-3}
DURATION_SECONDS=${DURATION_SECONDS:-5}
[[ $TRIALS =~ ^[1-9][0-9]*$ && $DURATION_SECONDS =~ ^[1-9][0-9]*$ && ! -e $RESULT_DIR/summary.csv ]]
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
if [[ -z ${SERVER_CPU:-} || -z ${CLIENT_CPUS:-} ]]; then choose_cpus; fi
git -C "$ROOT_DIR" rev-parse HEAD > "$RESULT_DIR/revision.txt"
printf 'SERVER_CPU=%s CLIENT_CPUS=%s TRIALS=%s DURATION_SECONDS=%s total_connections=16 keys=100000 value_bytes=32\n' "$SERVER_CPU" "$CLIENT_CPUS" "$TRIALS" "$DURATION_SECONDS" > "$RESULT_DIR/manifest.txt"
uname -a >> "$RESULT_DIR/manifest.txt"
printf 'server,trial,processes,threads_per_process,pipeline,operations,envelope_ops_per_sec,worst_process_p99_ms,start_spread_ms,client_cpu_pct\n' > "$RESULT_DIR/summary.csv"
pid=
trap '[[ -z ${pid:-} ]] || { kill -TERM "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }' EXIT
for server in reddish redis valkey; do
  case $server in
    reddish) command=("$REDDISH_BINARY" 6398 --no-wal --max-keys 200000);;
    redis) command=(redis-server --port 6398 --save '' --appendonly no);;
    valkey) command=(valkey-server --port 6398 --save '' --appendonly no);;
  esac
  taskset -c "$SERVER_CPU" "${command[@]}" > "$RESULT_DIR/$server.log" 2>&1 & pid=$!
  wait_server "$pid" 6398 "$server"
  base=(memtier_benchmark --server=127.0.0.1 --port=6398 --data-size=32 --key-minimum=1 --key-maximum=100000 --key-pattern=S:S --hide-histogram --ratio=0:1)
  taskset -c "$CLIENT_CPUS" "${base[@]}" --threads=1 --clients=1 --pipeline=64 --requests=100000 --ratio=1:0 > "$RESULT_DIR/$server-preload.log" 2>&1
  verify_preload "$server" 6398 100000 >/dev/null
  for ((trial=1; trial<=TRIALS; trial++)); do
    points=('1:1' '1:2' '1:4' '4:1')
    [[ $((trial%2)) == 1 ]] || points=('4:1' '1:4' '1:2' '1:1')
    for point in "${points[@]}"; do
      IFS=: read -r processes threads <<< "$point"
      for pipeline in 16 64; do
        stem="$RESULT_DIR/$server-$trial-p$processes-t$threads-q$pipeline"
        children=()
        for ((index=0; index<processes; index++)); do
          /usr/bin/time -f '%U,%S' -o "$stem-$index.time" taskset -c "$CLIENT_CPUS" "${base[@]}" --threads="$threads" --clients="$((16/processes/threads))" --pipeline="$pipeline" --test-time="$DURATION_SECONDS" --json-out-file="$stem-$index.json" --failed-keys-file="$stem-$index.failed.csv" > "$stem-$index.log" 2> "$stem-$index.stderr" & children+=("$!")
        done
        for child in "${children[@]}"; do wait "$child"; done
        for ((index=0; index<processes; index++)); do read_stats "$stem-$index.json" reads >/dev/null; done
        cpu=$(awk -F, '{seconds+=$1+$2} END{print seconds}' "$stem-"*.time)
        jq -sr --arg server "$server" --argjson trial "$trial" --argjson processes "$processes" --argjson threads "$threads" --argjson pipeline "$pipeline" --argjson cpu "$cpu" '[.[]|.["ALL STATS"]] as $s | (([$s[].Runtime."Finish time"]|max)-([$s[].Runtime."Start time"]|min)) as $ms | ([$s[].Totals.Count]|add) as $ops | [$server,$trial,$processes,$threads,$pipeline,$ops,$ops/($ms/1000),([$s[].Totals["Percentile Latencies"]."p99.00"]|max),([$s[].Runtime."Start time"]|max-min),$cpu/($ms/1000)*100]|@csv|gsub("\"";"")' "$stem-"*.json >> "$RESULT_DIR/summary.csv"
      done
    done
  done
  kill -TERM "$pid"; wait "$pid"; pid=
done
printf 'Throughput uses total operations divided by the measured envelope of all client processes. Tail latency is the worst process p99, not a pooled percentile. Assigned SMT siblings share physical resources; four workers do not imply four independent cores.\n' >> "$RESULT_DIR/manifest.txt"
touch "$RESULT_DIR/complete"
