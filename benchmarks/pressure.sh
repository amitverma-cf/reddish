#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
REDDISH_BINARY=$(realpath "${REDDISH_BINARY:-$ROOT_DIR/build/output/reddish}")
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/pressure-$(date +%Y%m%d-%H%M%S)"}
PRESSURE_KEYS=${PRESSURE_KEYS:-10000}
COLLECTION_ITEMS=${COLLECTION_ITEMS:-4096}
CHURN_KEYS=${CHURN_KEYS:-1024}
for setting in PRESSURE_KEYS COLLECTION_ITEMS CHURN_KEYS; do
  value=${!setting}
  [[ $value =~ ^[1-9][0-9]*$ && ${#value} -le 7 ]] || { echo "Invalid $setting" >&2; exit 1; }
done
[[ ! -e "$RESULT_DIR/pressure.csv" ]] || { echo 'Pressure output exists' >&2; exit 1; }
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
git -C "$ROOT_DIR" rev-parse HEAD > "$RESULT_DIR/revision.txt"
sha256sum "$REDDISH_BINARY" > "$RESULT_DIR/binary.sha256"
{
  git -C "$ROOT_DIR" rev-parse HEAD
  uname -a
  date -u '+Measured at %Y-%m-%d %H:%M:%S UTC'
  printf 'PRESSURE_KEYS=%s COLLECTION_ITEMS=%s CHURN_KEYS=%s\n' "$PRESSURE_KEYS" "$COLLECTION_ITEMS" "$CHURN_KEYS"
  printf 'RSS_PEAK=VmHWM\n'
} > "$RESULT_DIR/manifest.txt"
printf 'scenario,status,assertions,elapsed_ms,peak_rss_kib,final_keys,expired_keys,evictions\n' > "$RESULT_DIR/pressure.csv"
pid=; sampler=; port=6393
trap '[[ -z ${pid:-} ]] || { kill -TERM "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }; [[ -z ${sampler:-} ]] || wait "$sampler" 2>/dev/null || true' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
start_pressure() {
  [[ -z $(ss -H -ltn "sport = :$port") ]] || { echo "Port $port is occupied" >&2; exit 1; }
  "$REDDISH_BINARY" "$port" "$@" >> "$RESULT_DIR/$scenario.server.log" 2>&1 & pid=$!
  wait_server "$pid" "$port"
  (while kill -0 "$pid" 2>/dev/null; do awk '/^VmRSS:/{print $2}' "/proc/$pid/status" 2>/dev/null || true; sleep .05; done) >> "$RESULT_DIR/$scenario.rss.txt" & sampler=$!
}
stop_pressure() {
  local signal=${1:-INT}
  awk '/^VmHWM:/{print $2}' "/proc/$pid/status" >> "$RESULT_DIR/$scenario.rss.txt"
  kill -"$signal" "$pid"; timeout 20 tail --pid="$pid" -f /dev/null
  if [[ $signal == KILL ]]; then wait "$pid" 2>/dev/null || [[ $? == 137 ]]; else wait "$pid"; fi
  pid=; wait "$sampler" || true; sampler=
}
check() {
  local expression=$1; shift
  timeout 10 redis-cli -h 127.0.0.1 -p "$port" -2 -e --json "$@" > "$RESULT_DIR/$scenario-reply.json"
  jq -e "$expression" "$RESULT_DIR/$scenario-reply.json" >/dev/null || { echo "Pressure assertion failed: $scenario $*" >&2; exit 1; }
  ((++assertions))
}
for scenario in expiry eviction large_list promoted_hash churn backpressure durable_recovery; do
  assertions=0; start=$(date +%s%3N)
  case "$scenario" in
    expiry)
      start_pressure --no-wal --max-keys "$((PRESSURE_KEYS*2))"
      write_commands set "$PRESSURE_KEYS" exp: > "$RESULT_DIR/$scenario.resp"
      send_pipeline "$RESULT_DIR/$scenario.resp" "$PRESSURE_KEYS" ok "$port"
      write_commands expire "$PRESSURE_KEYS" exp: > "$RESULT_DIR/$scenario.resp"
      send_pipeline "$RESULT_DIR/$scenario.resp" "$PRESSURE_KEYS" one "$port"
      for ((attempt=0; attempt<200; attempt++)); do
        stats=$(redis-cli -p "$port" -2 --raw INFO | tr -d '\r')
        keys=$(awk -F: '$1=="keys"{print $2}' <<< "$stats")
        [[ $keys != 0 ]] || break
        sleep .05
      done
      [[ $keys == 0 ]] || { echo 'Expiry cleanup did not finish' >&2; exit 1; }
      expired=$(awk -F: '$1=="expired_keys"{print $2}' <<< "$stats")
      ((expired>=PRESSURE_KEYS)); ((++assertions))
      check '. == -2' TTL exp:1
      check '. == null' GET "exp:$PRESSURE_KEYS"
      ;;
    eviction)
      start_pressure --no-wal --max-keys "$PRESSURE_KEYS"
      write_commands set "$((PRESSURE_KEYS*2))" evict: > "$RESULT_DIR/$scenario.resp"
      send_pipeline "$RESULT_DIR/$scenario.resp" "$((PRESSURE_KEYS*2))" ok "$port"
      for ((attempt=0; attempt<200; attempt++)); do
        stats=$(redis-cli -p "$port" -2 --raw INFO | tr -d '\r')
        keys=$(awk -F: '$1=="keys"{print $2}' <<< "$stats")
        ((keys>PRESSURE_KEYS)) || break
        sleep .05
      done
      ((keys<=PRESSURE_KEYS)); ((++assertions))
      evictions=$(awk -F: '$1=="evictions"{print $2}' <<< "$stats")
      ((evictions>=PRESSURE_KEYS)); ((++assertions))
      check '. == null' GET evict:1
      check 'length == 32' GET "evict:$((PRESSURE_KEYS*2))"
      ;;
    large_list)
      start_pressure --no-wal --max-keys 10000
      write_commands rpush "$COLLECTION_ITEMS" list > "$RESULT_DIR/$scenario.resp"
      send_pipeline "$RESULT_DIR/$scenario.resp" "$COLLECTION_ITEMS" integer "$port"
      check ". == $COLLECTION_ITEMS" LLEN list
      check '. == "v1"' LPOP list
      check ". == \"v$COLLECTION_ITEMS\"" RPOP list
      check ". == $((COLLECTION_ITEMS-2))" LLEN list
      check '. == 1' DEL list
      check '. == 0' LLEN list
      ;;
    promoted_hash)
      start_pressure --no-wal --max-keys 10000
      write_commands hset "$COLLECTION_ITEMS" hash > "$RESULT_DIR/$scenario.resp"
      send_pipeline "$RESULT_DIR/$scenario.resp" "$COLLECTION_ITEMS" one "$port"
      check ". == $COLLECTION_ITEMS" HLEN hash
      check '. == "v1"' HGET hash f1
      check ". == \"v$COLLECTION_ITEMS\"" HGET hash "f$COLLECTION_ITEMS"
      check '. == 0' HSET hash f1 updated
      check '. == "updated"' HGET hash f1
      check '. == 1' HDEL hash "f$COLLECTION_ITEMS"
      check ". == $((COLLECTION_ITEMS-1))" HLEN hash
      check '. == 1' DEL hash
      ;;
    churn)
      start_pressure --no-wal --max-keys "$((CHURN_KEYS*2))"
      for bytes in 32 4096 64 16384 32 4096 64 16384 32; do
        write_commands set "$CHURN_KEYS" churn: "$bytes" > "$RESULT_DIR/$scenario.resp"
        send_pipeline "$RESULT_DIR/$scenario.resp" "$CHURN_KEYS" ok "$port"
        check "length == $bytes" GET churn:1
        check "length == $bytes" GET "churn:$CHURN_KEYS"
        printf '%s,%s\n' "$bytes" "$(awk '/^VmRSS:/{print $2}' "/proc/$pid/status")" >> "$RESULT_DIR/churn-phases.csv"
      done
      write_commands del "$CHURN_KEYS" churn: > "$RESULT_DIR/$scenario.resp"
      send_pipeline "$RESULT_DIR/$scenario.resp" "$CHURN_KEYS" one "$port"
      check '. == null' GET churn:1
      ;;
    backpressure)
      start_pressure --no-wal --max-keys 10000
      head -c 65536 /dev/zero | tr '\0' x | redis-cli -p "$port" -2 -e -x SET blob > "$RESULT_DIR/blob-set.txt"
      exec {socket}<>"/dev/tcp/127.0.0.1/$port"
      printf -v request '*2\r\n%s\r\nGET\r\n%s\r\nblob\r\n' "\$3" "\$4"
      packet=
      for ((i=0; i<64; i++)); do packet+=$request; done
      printf '%s' "$packet" >&"$socket"
      sleep .2
      check '. == "PONG"' PING
      if timeout 5 cat <&"$socket" > "$RESULT_DIR/backpressure-drained.bin" 2> "$RESULT_DIR/backpressure-read.log"; then
        :
      else
        status=$?
        if [[ $status != 1 ]] || ! grep -q 'Connection reset by peer' "$RESULT_DIR/backpressure-read.log"; then echo 'Slow reader was not disconnected' >&2; exit 1; fi
      fi
      exec {socket}>&-
      ((++assertions))
      check 'length == 65536' GET blob
      ;;
    durable_recovery)
      wal="$RESULT_DIR/recovery.wal"
      start_pressure --wal-path "$wal" --wal-sync always --max-keys 10000
      check '. == "OK"' SET durable value
      check '. == 2' RPUSH list first second
      check '. == 1' HSET hash field value
      check '. == "OK"' SET ttl gone
      check '. == 1' EXPIRE ttl 1
      stop_pressure KILL
      sleep 1.2
      printf 'bad' >> "$wal"
      start_pressure --wal-path "$wal" --wal-sync always --max-keys 10000
      check '. == "value"' GET durable
      check '. == "first"' LPOP list
      check '. == "second"' RPOP list
      check '. == "value"' HGET hash field
      check '. == null' GET ttl
      ;;
  esac
  stats=$(redis-cli -p "$port" -2 --raw INFO | tr -d '\r')
  keys=$(awk -F: '$1=="keys"{print $2}' <<< "$stats")
  expired=$(awk -F: '$1=="expired_keys"{print $2}' <<< "$stats")
  evictions=$(awk -F: '$1=="evictions"{print $2}' <<< "$stats")
  stop_pressure
  peak=$(awk '$1>m{m=$1} END{print m+0}' "$RESULT_DIR/$scenario.rss.txt")
  end=$(date +%s%3N)
  printf '%s,pass,%d,%d,%d,%d,%d,%d\n' "$scenario" "$assertions" "$((end-start))" "$peak" "$keys" "$expired" "$evictions" >> "$RESULT_DIR/pressure.csv"
done
touch "$RESULT_DIR/complete"
printf 'All pressure scenarios passed: %s\n' "$RESULT_DIR"
