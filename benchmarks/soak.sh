#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
REDDISH_BINARY=$(realpath "${REDDISH_BINARY:-$ROOT_DIR/build/output/reddish}")
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/soak-$(date +%Y%m%d-%H%M%S)"}
SOAK_SECONDS=${SOAK_SECONDS:-1800}
[[ $SOAK_SECONDS =~ ^[1-9][0-9]*$ && ! -e $RESULT_DIR/complete ]]
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
pid=; driver=; sampler=; child=
trap 'for child in ${driver:-} ${sampler:-} ${pid:-}; do kill -TERM "$child" 2>/dev/null || true; done; wait 2>/dev/null || true' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
git -C "$ROOT_DIR" rev-parse HEAD > "$RESULT_DIR/revision.txt"
sha256sum "$REDDISH_BINARY" > "$RESULT_DIR/binary.sha256"
printf 'seconds,keys,owned_bytes,rss_kib,expired_keys,evictions\n' > "$RESULT_DIR/memory.csv"
server=(); clients=()
[[ -z ${SERVER_CPU:-} ]] || server=(taskset -c "$SERVER_CPU")
[[ -z ${CLIENT_CPUS:-} ]] || clients=(taskset -c "$CLIENT_CPUS")
"${server[@]}" "$REDDISH_BINARY" 6399 --no-wal --max-keys 10000 --max-memory 8388608 > "$RESULT_DIR/server.log" 2>&1 & pid=$!
wait_server "$pid" 6399
base=(memtier_benchmark --server=127.0.0.1 --port=6399 --data-size=256 --key-minimum=1 --key-maximum=20000 --hide-histogram --threads=1 --clients=4 --pipeline=16)
"${clients[@]}" "${base[@]}" --clients=1 --requests=10000 --key-pattern=S:S --ratio=1:0 > "$RESULT_DIR/preload.log" 2>&1
verify_preload reddish 6399 10000 >/dev/null
write_commands expire 500 memtier- > "$RESULT_DIR/expiry.resp"
"${clients[@]}" "${base[@]}" --test-time="$SOAK_SECONDS" --key-pattern=R:R --ratio=1:1 --json-out-file="$RESULT_DIR/soak.json" --failed-keys-file="$RESULT_DIR/soak.failed.csv" > "$RESULT_DIR/soak.log" 2> "$RESULT_DIR/soak.stderr" & driver=$!
started=$SECONDS
(
  while kill -0 "$driver" 2>/dev/null; do
    stats=$(timeout 5 redis-cli -p 6399 -2 --raw INFO | tr -d '\r')
    keys=$(awk -F: '$1=="keys"{print $2}' <<< "$stats")
    owned=$(awk -F: '$1=="owned_memory_bytes"{print $2}' <<< "$stats")
    expired=$(awk -F: '$1=="expired_keys"{print $2}' <<< "$stats")
    evictions=$(awk -F: '$1=="evictions"{print $2}' <<< "$stats")
    rss=$(awk '/^VmRSS:/{print $2}' "/proc/$pid/status")
    [[ $keys =~ ^[0-9]+$ && $owned =~ ^[0-9]+$ ]] && ((keys<=10256 && owned<=8388608))
    printf '%s,%s,%s,%s,%s,%s\n' "$((SECONDS-started))" "$keys" "$owned" "$rss" "$expired" "$evictions" >> "$RESULT_DIR/memory.csv"
    send_pipeline "$RESULT_DIR/expiry.resp" 500 integer 6399
    sleep 5
  done
) > "$RESULT_DIR/sampler.log" 2>&1 & sampler=$!
wait "$driver"; driver=
wait "$sampler"; sampler=
read_stats "$RESULT_DIR/soak.json" eviction > "$RESULT_DIR/statistics.csv"
[[ $(timeout 5 redis-cli -p 6399 -2 --raw PING) == PONG ]]
kill -TERM "$pid"; wait "$pid"; pid=
printf 'Mixed traffic under eviction and expiry; misses are expected because 20000 possible keys exceed the 10000-key limit. No request/connection errors are accepted. RSS is process memory; owned bytes cover the database allocation budget.\n' > "$RESULT_DIR/manifest.txt"
printf 'SOAK_SECONDS=%s SERVER_CPU=%s CLIENT_CPUS=%s\n' "$SOAK_SECONDS" "${SERVER_CPU:-unbound}" "${CLIENT_CPUS:-unbound}" >> "$RESULT_DIR/manifest.txt"
uname -a >> "$RESULT_DIR/manifest.txt"
touch "$RESULT_DIR/complete"
