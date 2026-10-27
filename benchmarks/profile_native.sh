#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/native-$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=$(realpath "${REDDISH_BINARY:-$ROOT_DIR/build/output/reddish}")
PERF_BIN=${PERF_BIN:-perf}
[[ ! -e $RESULT_DIR/manifest.txt ]]
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
if [[ -z ${SERVER_CPU:-} || -z ${CLIENT_CPUS:-} ]]; then choose_cpus; fi
{
  git -c safe.directory="$ROOT_DIR" -C "$ROOT_DIR" rev-parse HEAD
  uname -a
  sha256sum "$REDDISH_BINARY"
  printf 'SERVER_CPU=%s CLIENT_CPUS=%s keys=100000 clients=16 threads=2 pipeline=16 workload=GET\n' "$SERVER_CPU" "$CLIENT_CPUS"
} > "$RESULT_DIR/manifest.txt"
have_perf=0
if "$PERF_BIN" stat -e cycles,instructions true > "$RESULT_DIR/perf-probe.log" 2>&1; then have_perf=1; fi
printf 'hardware_counters=%s\n' "$have_perf" >> "$RESULT_DIR/manifest.txt"
pid=
trap '[[ -z ${pid:-} ]] || { kill -TERM "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }' EXIT
for bytes in 32 256 4096; do
  taskset -c "$SERVER_CPU" "$REDDISH_BINARY" 6397 --no-wal --max-keys 200000 > "$RESULT_DIR/server-$bytes.log" 2>&1 & pid=$!
  wait_server "$pid" 6397
  base=(memtier_benchmark --server=127.0.0.1 --port=6397 --data-size="$bytes" --key-minimum=1 --key-maximum=100000 --key-pattern=S:S --hide-histogram --ratio=0:1)
  taskset -c "$CLIENT_CPUS" "${base[@]}" --threads=1 --clients=1 --pipeline=64 --requests=100000 --ratio=1:0 > "$RESULT_DIR/preload-$bytes.log" 2>&1
  verify_preload reddish 6397 100000 >/dev/null
  if [[ $have_perf == 1 ]]; then
    "$PERF_BIN" stat -e task-clock,cycles,instructions,cache-misses,context-switches -p "$pid" -o "$RESULT_DIR/counters-$bytes.txt" -- sleep 6 & monitor=$!
    taskset -c "$CLIENT_CPUS" "${base[@]}" --threads=2 --clients=8 --pipeline=16 --test-time=5 --json-out-file="$RESULT_DIR/counters-$bytes.json" --failed-keys-file="$RESULT_DIR/counters-$bytes.failed.csv" > "$RESULT_DIR/counters-$bytes.log" 2>&1
    wait "$monitor"
    read_stats "$RESULT_DIR/counters-$bytes.json" reads > "$RESULT_DIR/counters-$bytes.validated"
    "$PERF_BIN" record -q -F 199 -e cycles -g --call-graph dwarf,8192 -p "$pid" -o "$RESULT_DIR/cycles-$bytes.data" -- sleep 6 & monitor=$!
    taskset -c "$CLIENT_CPUS" "${base[@]}" --threads=2 --clients=8 --pipeline=16 --test-time=5 > "$RESULT_DIR/sampling-$bytes.log" 2>&1
    wait "$monitor"
    "$PERF_BIN" report --stdio --no-children --call-graph none -i "$RESULT_DIR/cycles-$bytes.data" > "$RESULT_DIR/cycles-$bytes.txt" 2>&1
  fi
  if command -v strace >/dev/null; then
    timeout --signal=INT 6 strace -f -c -w -e trace=network,epoll_wait,epoll_ctl,futex -p "$pid" -o "$RESULT_DIR/syscalls-$bytes.txt" > "$RESULT_DIR/trace-$bytes.log" 2>&1 & monitor=$!
    taskset -c "$CLIENT_CPUS" "${base[@]}" --threads=2 --clients=8 --pipeline=16 --test-time=4 > "$RESULT_DIR/traced-$bytes.log" 2>&1
    status=0; wait "$monitor" || status=$?
    [[ $status == 0 || $status == 124 ]]
  fi
  if [[ $bytes == 32 ]] && "$PERF_BIN" stat -e sched:sched_switch true > "$RESULT_DIR/scheduler-probe.log" 2>&1; then
    "$PERF_BIN" record -q -a -e sched:sched_switch -e sched:sched_wakeup -g -o "$RESULT_DIR/scheduler.data" -- sleep 6 & monitor=$!
    taskset -c "$CLIENT_CPUS" "${base[@]}" --threads=2 --clients=8 --pipeline=16 --test-time=5 > "$RESULT_DIR/scheduler-load.log" 2>&1
    wait "$monitor"
    "$PERF_BIN" sched timehist -i "$RESULT_DIR/scheduler.data" -p "$pid" --summary > "$RESULT_DIR/scheduler-summary.txt" 2>&1
    "$PERF_BIN" sched timehist -i "$RESULT_DIR/scheduler.data" -p "$pid" > "$RESULT_DIR/scheduler-waits.txt" 2>&1
  fi
  kill -TERM "$pid"; wait "$pid"; pid=
done
printf 'Instrumented diagnostics only. Counter windows include idle edges; syscall totals include maintenance threads. Missing hardware counters are unavailable, not zero.\n' >> "$RESULT_DIR/manifest.txt"
