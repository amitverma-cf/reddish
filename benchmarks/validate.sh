#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
REDDISH_BINARY=$(realpath "${REDDISH_BINARY:-$ROOT_DIR/build/output/reddish}")
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/validation-$(date +%Y%m%d-%H%M%S)"}
[[ ! -e "$RESULT_DIR/checks.csv" ]] || { echo 'Validation output already exists' >&2; exit 1; }
mkdir -p "$RESULT_DIR"
printf 'server,command,case,status\n' > "$RESULT_DIR/checks.csv"
git -C "$ROOT_DIR" rev-parse HEAD > "$RESULT_DIR/revision.txt"
uname -a > "$RESULT_DIR/environment.txt"
pid=
trap '[[ -z ${pid:-} ]] || { kill -TERM "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; }' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
expect_reply() {
  local label=$1 expression=$2; shift 2
  local output="$RESULT_DIR/$server-$label.json"
  if [[ ${1^^} == INFO ]]; then
    timeout 10 redis-cli -h 127.0.0.1 -p "$port" -2 -e --raw "$@" | jq -Rs . > "$output"
  else
    timeout 10 redis-cli -h 127.0.0.1 -p "$port" -2 -e --json "$@" > "$output"
  fi
  jq -e "$expression" "$output" >/dev/null || { echo "Unexpected $server reply: $label" >&2; cat "$output" >&2; exit 1; }
  printf '%s,%s,%s,pass\n' "$server" "$1" "$label" >> "$RESULT_DIR/checks.csv"
}
expect_error() {
  local label=$1; shift
  if timeout 10 redis-cli -h 127.0.0.1 -p "$port" -e --raw "$@" > "$RESULT_DIR/$server-$label.txt" 2>&1; then
    echo "Expected $server error: $label" >&2; exit 1
  else
    [[ $? == 1 ]] || { echo "Transport failure: $server $label" >&2; exit 1; }
  fi
  grep -Eq '^(ERR |WRONGTYPE )' "$RESULT_DIR/$server-$label.txt" || { echo "Not a server error: $server $label" >&2; exit 1; }
  printf '%s,%s,%s,pass\n' "$server" "$1" "$label" >> "$RESULT_DIR/checks.csv"
}
for server in reddish redis valkey; do
  port=6390
  [[ -z $(ss -H -ltn "sport = :$port") ]] || { echo "Port $port is occupied" >&2; exit 1; }
  if [[ $server == reddish ]]; then
    "$REDDISH_BINARY" "$port" --no-wal --max-keys 10000 > "$RESULT_DIR/$server.log" 2>&1 &
  else
    "$server-server" --bind 127.0.0.1 --port "$port" --save '' --appendonly no > "$RESULT_DIR/$server.log" 2>&1 &
  fi
  pid=$!; wait_server "$pid" "$port" "$server"
  expect_reply ping '. == "PONG"' PING
  expect_reply case_ping '. == "PONG"' pInG
  expect_reply info 'type == "string" and contains("# Server")' INFO
  expect_reply memory 'type == "string" and contains("used_memory")' INFO memory
  expect_reply missing '. == null' GET missing
  expect_reply set '. == "OK"' SET string first
  expect_reply get '. == "first"' GET string
  expect_reply overwrite '. == "OK"' SET string second
  expect_reply overwritten '. == "second"' get string
  expect_reply empty_set '. == "OK"' SET empty ''
  expect_reply empty_get '. == ""' GET empty
  printf 'A\0B\r\n' | timeout 10 redis-cli -h 127.0.0.1 -p "$port" -2 -e --json -x SET binary > "$RESULT_DIR/$server-binary-set.json"
  jq -e '. == "OK"' "$RESULT_DIR/$server-binary-set.json" >/dev/null
  expect_reply binary '. == "A\u0000B\r\n"' GET binary
  expect_reply incr_new '. == 1' INCR counter
  expect_reply incr_existing '. == 2' INCR counter
  expect_reply incr_get '. == "2"' GET counter
  expect_reply max_set '. == "OK"' SET number 9223372036854775807
  expect_error overflow INCR number
  expect_reply overflow_unchanged '. == "9223372036854775807"' GET number
  expect_error noninteger INCR string
  expect_reply lpush '. == 2' LPUSH list a b
  expect_reply rpush '. == 4' RPUSH list c d
  expect_reply llen '. == 4' LLEN list
  expect_reply lpop '. == "b"' LPOP list
  expect_reply rpop '. == "d"' RPOP list
  expect_reply list_remaining '. == 2' LLEN list
  expect_reply list_last_left '. == "a"' LPOP list
  expect_reply list_last_right '. == "c"' RPOP list
  expect_reply list_empty '. == 0' LLEN list
  expect_reply list_missing '. == null' LPOP list
  expect_reply hash_new '. == 2' HSET hash f1 one f2 two
  expect_reply hash_len '. == 2' HLEN hash
  expect_reply hash_update '. == 0' HSET hash f1 three
  expect_reply hash_get '. == "three"' HGET hash f1
  expect_reply hash_missing '. == null' HGET hash absent
  expect_reply hash_del_missing '. == 0' HDEL hash absent
  expect_reply hash_del '. == 1' HDEL hash f1
  expect_reply hash_remaining '. == 1' HLEN hash
  expect_reply hash_last '. == 1' HDEL hash f2
  expect_reply hash_empty '. == 0' HLEN hash
  for command in LPUSH RPUSH LPOP RPOP LLEN HSET HGET HDEL HLEN; do
    case "$command" in
      LPUSH|RPUSH) expect_error "wrong_$command" "$command" string value ;;
      HSET) expect_error "wrong_$command" "$command" string field value ;;
      HGET|HDEL) expect_error "wrong_$command" "$command" string field ;;
      *) expect_error "wrong_$command" "$command" string ;;
    esac
  done
  expect_reply expire_missing '. == 0' EXPIRE missing 1
  expect_reply ttl_missing '. == -2' TTL missing
  expect_reply ttl_set '. == "OK"' SET ttl value
  expect_reply ttl_persistent '. == -1' TTL ttl
  expect_reply expire '. == 1' EXPIRE ttl 3
  expect_reply ttl_live '. >= 0 and . <= 3' TTL ttl
  expect_reply ttl_reset '. == "OK"' SET ttl overwritten
  expect_reply ttl_cleared '. == -1' TTL ttl
  expect_reply expire_zero '. == 1' EXPIRE ttl 0
  expect_reply expired_zero '. == null' GET ttl
  expect_reply burst_set '. == "OK"' SET ttl short
  expect_reply expire_real '. == 1' EXPIRE ttl 1
  sleep 1.2
  expect_reply expired_real '. == null' GET ttl
  expect_reply expired_ttl '. == -2' TTL ttl
  expect_error invalid_expiry EXPIRE string nope
  expect_reply del '. == 3' DEL string empty binary absent
  expect_reply del_missing '. == 0' DEL string
  expect_error unknown __reddish_unknown__
  for command in SET GET INCR DEL LPUSH RPUSH LPOP RPOP LLEN HSET HGET HDEL HLEN EXPIRE TTL; do
    expect_error "arity_$command" "$command"
  done
  expect_reply alive '. == "PONG"' PING
  kill -INT "$pid"; timeout 15 tail --pid="$pid" -f /dev/null; wait "$pid"; pid=
done
awk -F, 'NR>1{seen[$1,$2]=1} END{split("reddish redis valkey",servers," ");split("PING INFO SET GET INCR DEL LPUSH RPUSH LPOP RPOP LLEN HSET HGET HDEL HLEN EXPIRE TTL",commands," ");for(s in servers)for(c in commands)if(!seen[servers[s],commands[c]])exit 1}' "$RESULT_DIR/checks.csv"
touch "$RESULT_DIR/complete"
printf 'All 17 supported commands passed validation on three servers: %s\n' "$RESULT_DIR"
