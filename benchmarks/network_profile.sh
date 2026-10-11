#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
BINARY=$(realpath "${1:?Pass the Reddish executable}")
RESULT_DIR=${2:?Pass a new output directory}
DURATION_SECONDS=${DURATION_SECONDS:-5}
TRIALS=${TRIALS:-3}
KEYS=${KEYS:-1000}
PORT=${PORT:-6394}
for setting in DURATION_SECONDS TRIALS KEYS PORT; do
  [[ ${!setting} =~ ^[1-9][0-9]*$ ]] || { echo "Invalid $setting" >&2; exit 1; }
done
[[ ! -e "$RESULT_DIR/network.csv" && -z $(ss -H -ltn "sport = :$PORT") ]] || { echo 'Output exists or port is occupied' >&2; exit 1; }
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
{
  git -C "$ROOT_DIR" rev-parse HEAD
  sha256sum "$BINARY"
  uname -a
  memtier_benchmark --version
  printf 'KEYS=%s CLIENTS=10 PIPELINE=16 DURATION_SECONDS=%s TRIALS=%s WARMUP_SECONDS=2\n' "$KEYS" "$DURATION_SECONDS" "$TRIALS"
} > "$RESULT_DIR/manifest.txt"
printf 'mode,trial,ops_per_sec,p99_ms,server_cpu_seconds,cpu_us_per_op,operation_count,wal_bytes\n' > "$RESULT_DIR/network.csv"
pid=
trap '[[ -z ${pid:-} ]] || { kill -KILL "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
hz=$(getconf CLK_TCK)
base=(memtier_benchmark --server=127.0.0.1 --port="$PORT" --threads=1 --clients=10 --pipeline=16 --data-size=32 --key-minimum=1 --key-maximum="$KEYS" --key-pattern=S:S --hide-histogram)
for ((trial=1; trial<=TRIALS; trial++)); do
  modes=(off everysec always)
  for ((offset=0; offset<3; offset++)); do
    mode=${modes[$(((trial-1+offset)%3))]}
    stem="$RESULT_DIR/$mode-$trial"
    [[ ! -e $stem.wal ]] || { echo "Existing WAL: $stem.wal" >&2; exit 1; }
    flags=(--no-wal)
    [[ $mode == off ]] || flags=(--wal-path "$stem.wal" --wal-sync "$mode")
    "$BINARY" "$PORT" --max-keys "$((KEYS*2))" "${flags[@]}" > "$stem.server.log" 2>&1 &
    pid=$!
    wait_server "$pid" "$PORT"
    "${base[@]}" --clients=1 --requests="$KEYS" --ratio=1:0 > "$stem.preload.txt" 2>&1
    verify_preload reddish "$PORT" "$KEYS" >/dev/null
    "${base[@]}" --test-time=2 --ratio=1:1 > "$stem.warmup.txt" 2>&1
    before=$(awk '{print $14+$15}' "/proc/$pid/stat")
    "${base[@]}" --test-time="$DURATION_SECONDS" --ratio=1:1 --json-out-file="$stem.json" --failed-keys-file="$stem.failed.csv" > "$stem.txt" 2> "$stem.stderr"
    after=$(awk '{print $14+$15}' "/proc/$pid/stat")
    values=$(read_stats "$stem.json" mixed)
    IFS=, read -r ops _ _ p99 _ count <<< "$values"
    kill -INT "$pid"
    timeout 30 tail --pid="$pid" -f /dev/null
    wait "$pid"; pid=
    bytes=0; [[ $mode == off ]] || bytes=$(stat -c %s "$stem.wal")
    awk -v mode="$mode" -v trial="$trial" -v ops="$ops" -v p99="$p99" -v before="$before" -v after="$after" -v hz="$hz" -v count="$count" -v bytes="$bytes" 'BEGIN{s=(after-before)/hz;printf "%s,%d,%s,%s,%.6f,%.6f,%d,%d\n",mode,trial,ops,p99,s,s*1000000/count,count,bytes}' >> "$RESULT_DIR/network.csv"
  done
done
printf 'WAL results: %s/network.csv\n' "$RESULT_DIR"
