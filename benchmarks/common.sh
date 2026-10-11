#!/usr/bin/env bash
set -euo pipefail

wait_server() {
  local pid=$1 port=$2 server=${3:-reddish} answer socket
  for ((attempt=0; attempt<600; attempt++)); do
    kill -0 "$pid" 2>/dev/null || { echo "$server exited before readiness" >&2; return 1; }
    if [[ $server == memcached ]]; then
      if { exec {socket}<>"/dev/tcp/127.0.0.1/$port"; } 2>/dev/null; then
        printf 'version\r\n' >&"$socket"
        answer=; IFS= read -r -t 1 answer <&"$socket" || true
        exec {socket}>&-
        [[ $answer == VERSION* ]] && return 0
      fi
    elif answer=$(timeout 1 redis-cli -h 127.0.0.1 -p "$port" --raw PING 2>/dev/null) && [[ $answer == PONG ]]; then
      return 0
    fi
    sleep .05
  done
  echo "$server on $port did not become ready" >&2
  return 1
}

verify_preload() {
  local server=$1 port=$2 expected=$3 count=0 evictions=0 socket line name value
  if [[ $server == memcached ]]; then
    exec {socket}<>"/dev/tcp/127.0.0.1/$port"
    printf 'stats\r\n' >&"$socket"
    while IFS= read -r -t 5 line <&"$socket"; do
      line=${line%$'\r'}
      [[ $line == END ]] && break
      read -r _ name value <<< "$line"
      [[ $name != curr_items ]] || count=$value
      [[ $name != evictions ]] || evictions=$value
    done
    exec {socket}>&-
    [[ $line == END && $evictions == 0 ]] || { echo 'Incomplete or evicting Memcached preload' >&2; return 1; }
  else
    count=$(timeout 5 redis-cli -h 127.0.0.1 -p "$port" --raw INFO | tr -d '\r' | awk -F '[:=,]' '/^keys:/{print $2} /^db0:keys=/{print $3}')
    count=${count:-0}
  fi
  [[ $count == "$expected" ]] || { echo "$server: expected $expected keys, observed $count" >&2; return 1; }
  printf '%s\n' "$count"
}

read_stats() {
  local raw=$1 workload=${2:-} expected=${3:-0}
  if [[ -s ${raw%.json}.failed.csv ]] && awk 'NF && tolower($0) !~ /^timestamp,/{bad=1} END{exit !bad}' "${raw%.json}.failed.csv"; then
    echo "Failed commands in ${raw%.json}.failed.csv" >&2; return 1
  fi
  jq -er --arg workload "$workload" --argjson expected "$expected" '
    .["ALL STATS"] as $all | $all.Totals as $s |
    if (($s["Connection Errors"] // 0) != 0 or ($s["Ops/sec"] | isnan or isinfinite) or
        $s["Ops/sec"] <= 0 or $s.Count <= 0 or
        ($all.Runtime.Interrupted == true or $all.Runtime.Interrupted == "true") or
        ($expected > 0 and $s.Count != $expected) or
        (($workload == "reads" or $workload == "mixed") and ($s["Misses/sec"] // 0) > 0))
    then error("invalid sample: errors, misses, interruption, or operation count")
    else [$s["Ops/sec"], $s["Average Latency"], $s["Percentile Latencies"]["p50.00"],
          $s["Percentile Latencies"]["p99.00"], $s["Percentile Latencies"]["p99.90"], $s.Count] |
         if any(.[]; type != "number") then error("missing numeric statistics") else @csv end end' "$raw"
}
