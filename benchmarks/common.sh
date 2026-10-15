#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

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
  [[ -f ${raw%.json}.failed.csv ]] || { echo "Missing failed-request log: $raw" >&2; return 1; }
  if [[ -s ${raw%.json}.failed.csv ]] && awk 'NF && tolower($0) !~ /^timestamp,/{bad=1} END{exit !bad}' "${raw%.json}.failed.csv"; then
    echo "Failed commands in ${raw%.json}.failed.csv" >&2; return 1
  fi
  jq -er --arg workload "$workload" --argjson expected "$expected" '
    .["ALL STATS"] as $all | $all.Totals as $s |
    if (($s["Connection Errors"] // 0) != 0 or ($s["Ops/sec"] | isnan or isinfinite) or
        $s["Ops/sec"] <= 0 or $s.Count <= 0 or
        ($all.Runtime.Interrupted == true or $all.Runtime.Interrupted == "true") or
        ($expected > 0 and $s.Count != $expected) or
        (($workload == "reads" or $workload == "mixed" or $workload == "hget" or $workload == "hlen" or $workload == "llen") and ($s["Misses/sec"] // 0) > 0))
    then error("invalid sample: errors, misses, interruption, or operation count")
    else [$s["Ops/sec"], $s["Average Latency"], $s["Percentile Latencies"]["p50.00"],
          $s["Percentile Latencies"]["p99.00"], $s["Percentile Latencies"]["p99.90"], $s.Count] |
         if any(.[]; type != "number") then error("missing numeric statistics") else @csv end end' "$raw"
}

sample_process() {
  local pid=$1 ticks runtime=0 queued=0 voluntary involuntary
  ticks=$(awk '{print $14+$15}' "/proc/$pid/stat")
  if [[ -r /proc/$pid/schedstat && $(cat /proc/sys/kernel/sched_schedstats 2>/dev/null || true) == 1 ]]; then
    read -r runtime queued _ < "/proc/$pid/schedstat"
  fi
  read -r voluntary involuntary < <(awk '/^voluntary_ctxt_switches:/{v=$2} /^nonvoluntary_ctxt_switches:/{i=$2} END{print v+0,i+0}' "/proc/$pid/status")
  printf '%s %s %s %s %s\n' "$ticks" "$runtime" "$queued" "$voluntary" "$involuntary"
}

choose_cpus() {
  local program
  for program in taskset lscpu nproc; do command -v "$program" >/dev/null || { echo "Missing $program" >&2; return 1; }; done
  local cpu core socket first_core='' first_socket='' clients=''
  while IFS=, read -r cpu core socket; do
    [[ $cpu != \#* ]] || continue
    taskset -c "$cpu" true 2>/dev/null || continue
    if [[ -z $first_core ]]; then
      SERVER_CPU=$cpu; first_core=$core; first_socket=$socket
    elif [[ $core != "$first_core" || $socket != "$first_socket" ]]; then
      clients+=${clients:+,}$cpu
    fi
  done < <(lscpu -p=CPU,CORE,SOCKET)
  [[ -n $first_core && -n $clients ]] || { echo 'Two available physical cores are required' >&2; return 1; }
  CLIENT_CPUS=$clients
  export SERVER_CPU CLIENT_CPUS
}

send_pipeline() {
  local file=$1 count=$2 kind=$3 port=$4 socket input header line packet argc sent=0 batch i j
  exec {socket}<>"/dev/tcp/127.0.0.1/$port"
  exec {input}< "$file"
  while ((sent<count)); do
    batch=$((count-sent)); ((batch<=16)) || batch=16
    packet=
    for ((i=0; i<batch; i++)); do
      IFS= read -r header <&"$input"
      argc=${header%$'\r'}; argc=${argc#\*}
      [[ $argc =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid generated command' >&2; return 1; }
      packet+=$header$'\n'
      for ((j=0; j<argc*2; j++)); do
        IFS= read -r line <&"$input"
        packet+=$line$'\n'
      done
    done
    printf '%s' "$packet" >&"$socket"
    for ((i=0; i<batch; i++)); do
      IFS= read -r -t 30 line <&"$socket" || { echo 'Incomplete pipeline reply' >&2; return 1; }
      line=${line%$'\r'}
      if [[ $kind == ok && $line == +OK || $kind == one && $line == :1 || $kind == integer && $line =~ ^:[0-9]+$ ]]; then
        continue
      fi
      echo "Unexpected pipeline reply: $line" >&2; return 1
    done
    sent=$((sent+batch))
  done
  exec {input}<&-
  exec {socket}>&-
}

write_commands() {
  local mode=$1 count=$2 prefix=$3 bytes=${4:-32}
  awk -v mode="$mode" -v count="$count" -v prefix="$prefix" -v bytes="$bytes" '
    function bulk(s){return "$" length(s) "\r\n" s "\r\n"}
    BEGIN{value="x";while(length(value)<bytes)value=value value;value=substr(value,1,bytes);for(i=1;i<=count;i++){
      if(mode=="set")printf "*3\r\n%s%s%s",bulk("SET"),bulk(prefix i),bulk(value);
      else if(mode=="expire")printf "*3\r\n%s%s%s",bulk("EXPIRE"),bulk(prefix i),bulk("1");
      else if(mode=="del")printf "*2\r\n%s%s",bulk("DEL"),bulk(prefix i);
      else if(mode=="rpush")printf "*3\r\n%s%s%s",bulk("RPUSH"),bulk(prefix),bulk("v" i);
      else if(mode=="hset")printf "*4\r\n%s%s%s%s",bulk("HSET"),bulk(prefix),bulk("f" i),bulk("v" i);
      else exit 1}}'
}
