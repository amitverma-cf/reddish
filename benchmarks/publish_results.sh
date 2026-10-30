#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULTS_DIR=${RESULTS_DIR:?Set RESULTS_DIR to a completed benchmark run}
OUTPUT_FILE=${OUTPUT_FILE:-"$ROOT_DIR/benchmarks/benchmarks.md"}
README_FILE=${README_FILE-"$ROOT_DIR/README.md"}
[[ -s "$RESULTS_DIR/summary.csv" && -f "$RESULTS_DIR/memory.csv" && -s "$RESULTS_DIR/manifest.txt" ]] || { echo 'Incomplete benchmark run' >&2; exit 1; }
bash "$ROOT_DIR/benchmarks/validate_results.sh" "$RESULTS_DIR"
revision=$(head -1 "$RESULTS_DIR/manifest.txt")
measured=$(sed -n 's/^Measured at //p' "$RESULTS_DIR/manifest.txt")
platform=Linux
! grep -qi microsoft "$RESULTS_DIR/manifest.txt" || platform=WSL2
run_url=
worktree_dirty=0
if grep -q '^WORKTREE_STATE=dirty$' "$RESULTS_DIR/manifest.txt" || grep -Eq '^[ MADRCU?!]{2} ' "$RESULTS_DIR/manifest.txt"; then worktree_dirty=1; fi
if [[ -f "$RESULTS_DIR/provenance.json" ]]; then
  [[ $(jq -er '.revision' "$RESULTS_DIR/provenance.json") == "$revision" ]] || { echo 'Provenance revision mismatch' >&2; exit 1; }
  platform=$(jq -er '.environment' "$RESULTS_DIR/provenance.json")
  run_url=$(jq -r '.run_url // empty' "$RESULTS_DIR/provenance.json")
fi
base_host=$(grep -m1 '^Linux ' "$RESULTS_DIR/manifest.txt" || true)
for suite in VALIDATION_DIR CAPACITY_DIR PRESSURE_DIR WAL_RESULTS_DIR DRIVER_DIR SOAK_DIR; do
  directory=${!suite:-}
  [[ -n $directory ]] || continue
  marker="$directory/revision.txt"
  if [[ $suite == CAPACITY_DIR || $suite == WAL_RESULTS_DIR ]]; then marker="$directory/manifest.txt"; fi
  [[ $(head -1 "$marker") == "$revision" ]] || { echo "Different revision: $suite" >&2; exit 1; }
  if [[ $suite != WAL_RESULTS_DIR && ! -f $directory/complete ]]; then echo "Incomplete suite: $suite" >&2; exit 1; fi
  host_file="$directory/manifest.txt"
  [[ $suite != VALIDATION_DIR ]] || host_file="$directory/environment.txt"
  suite_host=$(grep -m1 '^Linux ' "$host_file" || true)
  [[ -n $base_host && $suite_host == "$base_host" ]] || { echo "Different or missing measurement host: $suite" >&2; exit 1; }
done
if [[ -n ${DRIVER_DIR:-} || -n ${SOAK_DIR:-} ]]; then
  source "$ROOT_DIR/benchmarks/common.sh"
  if [[ -n ${DRIVER_DIR:-} ]]; then
    while IFS=, read -r server trial processes threads pipeline count ops p99 spread cpu; do
      [[ $server != server ]] || continue
      [[ $server == reddish || $server == redis || $server == valkey ]] || exit 1
      stem="$DRIVER_DIR/$server-$trial-p$processes-t$threads-q$pipeline"
      for ((index=0; index<processes; index++)); do
        read_stats "$stem-$index.json" reads >/dev/null
        jq -e --argjson threads "$threads" --argjson clients "$((16/processes/threads))" --argjson pipeline "$pipeline" '.configuration|.threads==$threads and .clients==$clients and .pipeline==$pipeline and .data_size==32 and .key_maximum==100000 and .key_minimum==1' "$stem-$index.json" >/dev/null || exit 1
      done
      actual=$(jq -sr '[.[]|.["ALL STATS"]] as $s | ([$s[].Totals.Count]|add) as $n | [$n,$n/((([$s[].Runtime."Finish time"]|max)-([$s[].Runtime."Start time"]|min))/1000),([$s[].Totals["Percentile Latencies"]."p99.00"]|max),([$s[].Runtime."Start time"]|max-min)]|@csv' "$stem-"*.json)
      IFS=, read -r actual_count actual_ops actual_p99 actual_spread <<< "$actual"
      awk -v n="$count" -v an="$actual_count" -v ops="$ops" -v ao="$actual_ops" -v p="$p99" -v ap="$actual_p99" -v s="$spread" -v as="$actual_spread" -v cpu="$cpu" 'BEGIN{exit(n!=an||(ops-ao)^2>.01||(p-ap)^2>.000001||s!=as||cpu<0)}' || exit 1
    done < "$DRIVER_DIR/summary.csv"
    driver_trials=$(grep -oE 'TRIALS=[0-9]+' "$DRIVER_DIR/manifest.txt" | cut -d= -f2)
    [[ $driver_trials =~ ^[1-9][0-9]*$ ]] || exit 1
    awk -F, -v n="$driver_trials" 'NR>1{if($2<1||$2>n||!(($3==1&&($4==1||$4==2||$4==4))||($3==4&&$4==1))||($5!=16&&$5!=64)||seen[$1,$2,$3,$4,$5]++)exit 1}' "$DRIVER_DIR/summary.csv" || exit 1
    [[ $(($(wc -l < "$DRIVER_DIR/summary.csv")-1)) == "$((driver_trials*24))" ]] || { echo 'Incomplete driver sweep' >&2; exit 1; }
  fi
  if [[ -n ${SOAK_DIR:-} ]]; then
    read_stats "$SOAK_DIR/soak.json" eviction >/dev/null
    soak_seconds=$(grep -oE 'SOAK_SECONDS=[0-9]+' "$SOAK_DIR/manifest.txt" | cut -d= -f2)
    [[ $soak_seconds =~ ^[1-9][0-9]*$ ]] || exit 1
    jq -e --argjson seconds "$soak_seconds" '.["ALL STATS"].Runtime["Total duration"] >= $seconds*990' "$SOAK_DIR/soak.json" >/dev/null || { echo 'Short soak sample' >&2; exit 1; }
    awk -F, -v seconds="$soak_seconds" '
      function bad(reason){printf "Soak row %d: %s\n",NR,reason > "/dev/stderr";failed=1;exit 1}
      NR>1{
        if(NF!=6)bad("expected six columns")
        for(i=1;i<=6;i++)if($i!~/^[0-9]+$/)bad("expected nonnegative integer counters")
        if($2>10256)bad("key limit exceeded")
        if($3>8388608||$3<128)bad("owned allocation budget invalid")
        if($4<=0)bad("missing RSS sample")
        if(n&&($1<last||$5<expired||$6<evictions))bad("time or cumulative counters decreased")
        if(!n&&$1>15)bad("sampling started late")
        n++;last=$1;expired=$5;evictions=$6
      }
      END{
        if(failed)exit 1
        if(!n||n<seconds/15||last<seconds-15)bad("incomplete sampling interval")
        if(!evictions)bad("eviction pressure was not observed")
      }' "$SOAK_DIR/memory.csv" || { echo 'Invalid soak measurements' >&2; exit 1; }
  fi
fi
if [[ -n ${WAL_RESULTS_DIR:-} ]]; then
  source "$ROOT_DIR/benchmarks/common.sh"
  wal_trials=$(grep -oE '(^| )TRIALS=[0-9]+' "$WAL_RESULTS_DIR/manifest.txt" | tail -1 | cut -d= -f2)
  [[ $wal_trials =~ ^[1-9][0-9]*$ ]] || { echo 'Missing WAL trial count' >&2; exit 1; }
  awk -F, -v n="$wal_trials" 'NR>1{if(NF!=8||$2<1||$2>n||$3<=0||$4<0||$5<0||$7<=0||$8<0||seen[$1,$2]++)exit 1;counts[$1]++} END{if(counts["off"]!=n||counts["everysec"]!=n||counts["always"]!=n)exit 1}' "$WAL_RESULTS_DIR/network.csv" || { echo 'Incomplete WAL modes' >&2; exit 1; }
  while IFS=, read -r mode trial ops p99 seconds cpu count _; do
    [[ $mode != mode ]] || continue
    [[ $mode == off || $mode == everysec || $mode == always ]] || { echo 'Unknown WAL mode' >&2; exit 1; }
    stats=$(read_stats "$WAL_RESULTS_DIR/$mode-$trial.json" mixed)
    IFS=, read -r actual_ops _ _ actual_p99 _ actual_count <<< "$stats"
    awk -v a="$ops" -v b="$actual_ops" -v c="$p99" -v d="$actual_p99" -v n="$count" -v actual="$actual_count" -v seconds="$seconds" -v cpu="$cpu" 'BEGIN{exit ((a-b)^2>.0005||(c-d)^2>.000001||n!=actual||(cpu-seconds*1e6/n)^2>.000001)}' || { echo 'WAL sample differs from raw data' >&2; exit 1; }
  done < "$WAL_RESULTS_DIR/network.csv"
fi
if [[ -n $README_FILE ]]; then
  [[ $(realpath -m "$OUTPUT_FILE") != "$(realpath -m "$README_FILE")" ]] || { echo 'Report and README destinations must differ' >&2; exit 1; }
  awk '/^<!-- benchmark-summary:start -->$/{if(start||end)exit 1;start=1} /^<!-- benchmark-summary:end -->$/{if(!start||end)exit 1;end=1} END{if(!start||!end)exit 1}' "$README_FILE" || { echo 'README needs one ordered benchmark marker pair' >&2; exit 1; }
fi
report="$RESULTS_DIR/report.md"
{
  printf '# Benchmark results\n\n'
  printf 'Measured revision: [%s](https://github.com/amitverma-cf/reddish/commit/%s).\n\n' "${revision:0:7}" "$revision"
  printf 'Environment: **%s**. Measured: **%s**.\n\n' "$platform" "${measured:-not recorded}"
  [[ -z $run_url ]] || printf '[Source workflow run](%s). Raw data and profiles are attached to that run.\n\n' "$run_url"
  if [[ $worktree_dirty == 1 ]]; then printf '**This run included uncommitted changes.** The revision identifies HEAD; the manifest and recorded source, script, and binary hashes describe the measured files.\n\n'; fi
  printf 'These are results for this machine and workload. They do not establish a general ranking or maximum server capacity.\n\n'
  printf 'Loopback TCP on the machine below. Persistence is disabled for the server comparison. Each server receives the same preload, warmup, values, clients, and pipeline. Server order rotates between trials; GET and SET order alternates.\n\n'
  printf '## Setup\n\n```text\n'
  grep -E '^Measured at |^Linux |^Model name:|^CPU\(s\):|^memtier|^Redis server|^Valkey server|^memcached |^SERVER_CPU=|^REQUESTS_PER_CLIENT=|^KEYS=' "$RESULTS_DIR/manifest.txt"
  printf '```\n\n'
  printf '## Throughput\n\nOperations per second. Range and standard deviation describe the trial means, not individual requests.\n\n'
  printf '| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |\n|---|---|---:|---:|---:|---:|---:|\n'
  LC_ALL=C sort -t, -k2,2 -k1,1 "$RESULTS_DIR/summary.csv" | awk -F, '$2=="reads"||$2=="writes"||$2=="mixed"{printf "| %s | %s | %d | %.0f | %.0f | %.0f | %.0f |\n",$2,$1,$3,$4,$6,$7,$8}'
  if awk -F, 'NR>1 && $2!="reads" && $2!="writes" && $2!="mixed"{found=1} END{exit !found}' "$RESULTS_DIR/summary.csv"; then
    printf '\n## Other commands\n\nEach paired workload reports individual commands per second.\n\n'
    printf '| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |\n|---|---|---:|---:|---:|---:|---:|\n'
    LC_ALL=C sort -t, -k2,2 -k1,1 "$RESULTS_DIR/summary.csv" | awk -F, '$1!="server" && $2!="reads" && $2!="writes" && $2!="mixed"{printf "| %s | %s | %d | %.0f | %.0f | %.0f | %.0f |\n",$2,$1,$3,$4,$6,$7,$8}'
  fi
  [[ $(wc -l < "$RESULTS_DIR/summary.csv") -gt 1 ]] || printf '\nNo throughput samples in this run.\n'
  printf '\n## Latency and CPU\n\nLatency is in milliseconds. Percentiles are averages of each trial percentile; they are not pooled percentiles. CPU is server user plus system time per operation, excluding the load generator.\n\n'
  printf '| Workload | Server | Mean latency ms | p50 ms | p99 ms | p99.9 ms | CPU µs/op |\n|---|---|---:|---:|---:|---:|---:|\n'
  awk -F, 'FNR==1{next} FILENAME==ARGV[1]{k=$1 FS $2;seconds[k]+=$4;ops[k]+=$5;next} {k=$1 FS $2;n[k]++;lat[k]+=$5;p50[k]+=$6;p99[k]+=$7;p999[k]+=$8} END{for(k in n){split(k,a,FS);printf "%s,%s,%.3f,%.3f,%.3f,%.3f,%s\n",a[2],a[1],lat[k]/n[k],p50[k]/n[k],p99[k]/n[k],p999[k]/n[k],ops[k]?sprintf("%.3f",seconds[k]*1000000/ops[k]):"—"}}' "$RESULTS_DIR/cpu.csv" "$RESULTS_DIR/results.csv" | sort | awk -F, '{printf "| %s | %s | %s | %s | %s | %s | %s |\n",$1,$2,$3,$4,$5,$6,$7}'
  printf '\n## Memory\n\nFresh server processes are measured before and after each preload. Each key holds one string, one list element, or one hash field. RSS includes process and allocator overhead. Delta bytes/key is (loaded RSS − idle RSS) × 1024 / verified key count. It is not the exact allocation size of a value.\n\n'
  printf '| Shape | Server | Trials | Keys | Idle RSS MiB | Loaded RSS MiB | Delta bytes/key | Min bytes/key | Max bytes/key |\n|---|---|---:|---:|---:|---:|---:|---:|---:|\n'
  awk -F, 'NR>1{k=$2 FS $1;if(k in keys && keys[k]!=$4)exit 1;keys[k]=$4;n[k]++;idle[k]+=$5;rss[k]+=$6;bytes[k]+=$8;if(!(k in lo)||$8<lo[k])lo[k]=$8;if(!(k in hi)||$8>hi[k])hi[k]=$8} END{for(k in n){split(k,a,FS);printf "| %s | %s | %d | %d | %.2f | %.2f | %.2f | %.2f | %.2f |\n",a[1],a[2],n[k],keys[k],idle[k]/n[k]/1024,rss[k]/n[k]/1024,bytes[k]/n[k],lo[k],hi[k]}}' "$RESULTS_DIR/memory.csv" | sort
  [[ $(wc -l < "$RESULTS_DIR/memory.csv") -gt 1 ]] || printf '\nNo memory samples in this run.\n'
  if [[ -n ${WAL_RESULTS_DIR:-} ]]; then
    printf '\n## WAL modes\n\nReddish only. These samples include WAL cost and use a separate workload; they are not part of the persistence-disabled comparison.\n\n'
    printf 'These samples use 32-byte values and a 50/50 SET/GET mix without CPU binding.\n\n```text\n'
    grep '^KEYS=' "$WAL_RESULTS_DIR/manifest.txt"
    printf '```\n\n'
    printf '| Mode | Trials | Mean ops/s | Min | Max | Mean p99 ms | CPU µs/op |\n|---|---:|---:|---:|---:|---:|---:|\n'
    awk -F, 'NR>1{k=$1;n[k]++;ops[k]+=$3;p99[k]+=$4;cpu[k]+=$5;count[k]+=$7;if(!(k in lo)||$3<lo[k])lo[k]=$3;if(!(k in hi)||$3>hi[k])hi[k]=$3} END{for(k in n)printf "| %s | %d | %.0f | %.0f | %.0f | %.3f | %.3f |\n",k,n[k],ops[k]/n[k],lo[k],hi[k],p99[k]/n[k],cpu[k]*1000000/count[k]}' "$WAL_RESULTS_DIR/network.csv" | sort
  fi
  printf '\n## Validation\n\n'
  if [[ -n ${VALIDATION_DIR:-} ]]; then
    printf '| Server | Checks passed | Commands covered |\n|---|---:|---:|\n'
    awk -F, 'BEGIN{split("PING INFO SET GET INCR DEL LPUSH RPUSH LPOP RPOP LLEN HSET HGET HDEL HLEN EXPIRE TTL",a," ");for(i in a)supported[a[i]]=1} NR>1{if($4!="pass")exit 1;n[$1]++;command=toupper($2);if(command in supported&&!seen[$1,command]++)commands[$1]++} END{for(s in n)printf "| %s | %d | %d |\n",s,n[s],commands[s]}' "$VALIDATION_DIR/checks.csv" | sort
    printf '\nThe preflight checks supported command forms, payloads, list ordering, hash mutations, errors, overflow, missing keys and real TTL expiry. This is command coverage, not a proof of every input or execution path.\n'
  else
    printf 'No command-preflight artifact was recorded for this run. Original preload counts, miss/error checks and unit tests are separate evidence.\n'
  fi
  printf '\n## Client CPU and server scheduling\n\n'
  if [[ -f "$RESULTS_DIR/process.csv" && $(wc -l < "$RESULTS_DIR/process.csv") -gt 1 ]]; then
    printf 'CPU is normalized by the timed workload duration. Client process CPU includes startup and teardown. Main-thread runtime and queue counters bracket the client call; blocked time is an estimate after subtracting runtime and queue time from workload duration. It does not identify the wait stack. NA means scheduler counters were unavailable. A Memcached main thread is its dispatcher.\n\n'
    printf '| Workload | Server | Server CPU/core %% | Client CPU %% | Main run queue %% | Estimated main blocked %% | Context switches/s |\n|---|---|---:|---:|---:|---:|---:|\n'
    awk -F, 'NR>1&&($2=="reads"||$2=="writes"||$2=="mixed"){k=$2 FS $1;elapsed[k]+=$4;server[k]+=$5;client[k]+=$6;switches[k]+=$11+$12;if($9!="NA"){queue[k]+=$9;blocked[k]+=$10;measured[k]+=$4}} END{for(k in elapsed){split(k,a,FS);printf "| %s | %s | %.2f | %.2f | %s | %s | %.0f |\n",a[1],a[2],server[k]/elapsed[k]*100,client[k]/elapsed[k]*100,(measured[k]?sprintf("%.2f",queue[k]/measured[k]*100):"NA"),(measured[k]?sprintf("%.2f",blocked[k]/measured[k]*100):"NA"),switches[k]/elapsed[k]}}' "$RESULTS_DIR/process.csv" | sort
  else
    printf 'Client CPU and main-thread scheduling were not recorded in this run. Server CPU/op above does not establish the limiting resource.\n'
  fi
  printf '\n## Capacity measurements\n\n'
  if [[ -n ${CAPACITY_DIR:-} ]]; then
    printf 'Best observed points are grouped by server, workload, key count, value size and access pattern. Only points whose every trial meets the configured p99 limit qualify. These closed-loop measurements are not proof of absolute maximum capacity or an open-loop latency guarantee. Client CPU near a worker or assigned-CPU budget can limit the result. A one-trial row is a smoke measurement; repeated rows are needed for capacity conclusions.\n\n```text\n'
    grep '^SERVER_CPU=' "$CAPACITY_DIR/manifest.txt"
    printf '```\n\n| Server | Workload | Keys | Bytes | Clients | Threads | Pipeline | Pattern | Trials | Mean ops/s | Worst trial p99 ms |\n|---|---|---:|---:|---:|---:|---:|---|---:|---:|---:|\n'
    awk -F, 'NR>1{printf "| %s | %s | %d | %d | %d | %d | %d | %s | %d | %.0f | %.3f |\n",$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$14}' "$CAPACITY_DIR/best-observed.csv"
    printf '\n<details>\n<summary>Every tested operating point</summary>\n\n| Case | Server | Workload | Trials | Mean ops/s | Worst p99 ms | Server CPU %% | Client CPU/worker %% | Client CPU/budget %% | Meets p99 limit |\n|---|---|---|---:|---:|---:|---:|---:|---:|---|\n'
    awk -F, 'NR>1{printf "| %s | %s | %s | %d | %.0f | %.3f | %.2f | %.2f | %s | %s |\n",$1,$2,$3,$10,$11,$14,$15,$16,($20?sprintf("%.2f",$19/$20):"NA"),$18}' "$CAPACITY_DIR/summary.csv"
    printf '\nCase names record total clients (c), client threads (t), pipeline (p), bytes (v), keys (k), and sequential/random pattern (S/R). Client budget uses the available CPU count reported by nproc under the assigned affinity; SMT threads share physical resources. Older data without that count is NA.\n\n</details>\n'
  else
    printf 'No capacity sweep was recorded for this run. The baseline comparison is not a capacity claim.\n'
  fi
  printf '\n## Pressure scenarios\n\n'
  if [[ -n ${PRESSURE_DIR:-} ]]; then
    printf 'Reddish only. These are bounded correctness scenarios, not comparative throughput measurements or a long soak test. Churn RSS can remain reserved after keys are freed.\n\n'
    if grep -q '^RSS_PEAK=VmHWM$' "$PRESSURE_DIR/manifest.txt"; then printf 'Peak RSS includes the kernel high-water mark captured before each process stops.\n\n'; else printf 'Peak RSS is the largest periodic sample recorded.\n\n'; fi
    printf '```text\n'
    grep '^PRESSURE_KEYS=' "$PRESSURE_DIR/manifest.txt"
    printf '```\n\n| Scenario | Result | Assertions | Elapsed ms | Peak RSS KiB | Final keys | Expired keys | Evictions |\n|---|---|---:|---:|---:|---:|---:|---:|\n'
    awk -F, 'NR>1{if($2!="pass")exit 1;printf "| %s | %s | %d | %d | %d | %d | %d | %d |\n",$1,$2,$3,$4,$5,$6,$7,$8}' "$PRESSURE_DIR/pressure.csv"
  else
    printf 'No pressure-scenario artifact was recorded for this run. Baseline TTL bookkeeping does not measure expiry storms, and the baseline does not induce eviction.\n'
  fi
  printf '\n## Notes\n\n'
  printf -- '- This is a single-machine loopback test. WSL host scheduling and clock changes can cause large trial variance. Compare the ranges as well as the means.\n'
  printf -- '- GET and mixed samples must have zero misses. Preload key counts and fixed request counts are checked; failed requests invalidate the run.\n'
  printf -- '- Paired extended workloads report commands per second, not completed pairs. set_del runs SET/DEL, list pairs push/pop, hset_hdel inserts/deletes a field, and expire_ttl runs SET/EXPIRE/TTL.\n'
  if grep -q '^memcached,' "$RESULTS_DIR/results.csv" "$RESULTS_DIR/memory.csv"; then
    printf -- '- Memcached is measured only for GET, SET, and string RSS. It does not implement the other workloads.\n'
  else
    printf -- '- Memcached was not included in this run.\n'
  fi
  printf '\n## Individual throughput trials\n\n<details>\n<summary>All measured samples</summary>\n\n'
  printf '| Workload | Server | Trial | Ops/s | Mean latency ms | p99 ms |\n|---|---|---:|---:|---:|---:|\n'
  tail -n +2 "$RESULTS_DIR/results.csv" | sort -t, -k2,2 -k1,1 -k3,3n | awk -F, '{printf "| %s | %s | %d | %.0f | %.3f | %.3f |\n",$2,$1,$3,$4,$5,$7}'
  printf '\n</details>\n\n## Individual memory trials\n\n<details>\n<summary>All measured samples</summary>\n\n'
  printf '| Shape | Server | Trial | Keys | Idle RSS KiB | Loaded RSS KiB | Delta bytes/key |\n|---|---|---:|---:|---:|---:|---:|\n'
  tail -n +2 "$RESULTS_DIR/memory.csv" | sort -t, -k2,2 -k1,1 -k3,3n | awk -F, '{printf "| %s | %s | %d | %d | %d | %d | %.2f |\n",$2,$1,$3,$4,$5,$6,$8}'
  printf '\n</details>\n'
  if [[ -n ${DRIVER_DIR:-} ]]; then
    printf '\n## Client driver checks\n\nFixed 16 connections, 100000 keys, 32-byte values, GET, and the recorded CPU affinity. Throughput is total operations divided by the envelope of all process start/finish times. Worst p99 is the worst process percentile, not a pooled percentile. Extra workers sharing assigned cores are not extra physical capacity.\n\n'
    printf '| Server | Processes | Threads/process | Pipeline | Trials | Mean ops/s | Worst process p99 ms |\n|---|---:|---:|---:|---:|---:|---:|\n'
    awk -F, 'NR>1{k=$1 FS $3 FS $4 FS $5;n[k]++;ops[k]+=$7;if($8>p[k])p[k]=$8}END{for(k in n){split(k,a,FS);printf "| %s | %s | %s | %s | %s | %.0f | %.3f |\n",a[1],a[2],a[3],a[4],n[k],ops[k]/n[k],p[k]}}' "$DRIVER_DIR/summary.csv" | sort
  fi
  if [[ -n ${SOAK_DIR:-} ]]; then
    printf '\n## Sustained eviction and expiry\n\nMixed SET/GET with 20000 possible keys, a 10000-key limit and an 8 MiB owned-allocation budget. Misses are expected under eviction; request and connection errors are rejected. Concurrent EXPIRE calls may produce zero expirations when SET clears TTLs or eviction removes keys first. The separate expiry pressure scenario verifies actual expiration. RSS includes allocator and process memory outside the database budget.\n\n'
    awk -F, 'NR>1{n++;seconds=$1;if($3>owned)owned=$3;if($4>rss)rss=$4;expired=$5;evictions=$6}END{printf "Observed samples: %d; last sample: %d seconds; peak sampled owned memory: %.2f MiB; peak sampled RSS: %.2f MiB; expired keys: %.0f; evictions: %.0f.\n",n,seconds,owned/1048576,rss/1024,expired,evictions}' "$SOAK_DIR/memory.csv"
  fi
  printf '\n## Measurement manifest\n\n<details>\n<summary>Revision, executable hash, hardware, tools, and settings</summary>\n\n```text\n'
  cat "$RESULTS_DIR/manifest.txt"
  printf '```\n\n</details>\n'
} > "$report.tmp"
mv "$report.tmp" "$report"
mkdir -p "$(dirname "$OUTPUT_FILE")"
[[ $(realpath -m "$report") == "$(realpath -m "$OUTPUT_FILE")" ]] || cp "$report" "$OUTPUT_FILE"
if [[ -n $README_FILE ]]; then
  summary="$RESULTS_DIR/readme-summary.md"
  {
    printf '<!-- benchmark-summary:start -->\n### Benchmark results\n\n'
    printf '%s, measured %s; source revision [%s](https://github.com/amitverma-cf/reddish/commit/%s). Persistence disabled. [Full report](benchmarks/benchmarks.md) contains the same dataset, trial ranges, latency, CPU, memory, and setup.\n\n' "$platform" "${measured:-not recorded}" "${revision:0:7}" "$revision"
    awk '/^KEYS=/{for(i=1;i<=NF;i++){split($i,a,"=");v[a[1]]=a[2]}printf "Baseline: %s keys, %s-byte values; client threads: %s, total connections: %d, pipeline: %s; %s trials of %s seconds.\n",v["KEYS"],v["VALUE_BYTES"],v["THREADS"],v["THREADS"]*v["CLIENTS"],v["PIPELINE"],v["TRIALS"],v["DURATION_SECONDS"]}' "$RESULTS_DIR/manifest.txt"
    printf '\nThese rows use the fixed baseline configuration. The full report lists capacity sweeps separately; changing client threads or pipeline depth changes the result.\n\n'
    [[ -z $run_url ]] || printf '[Source workflow run](%s).\n\n' "$run_url"
    if [[ $worktree_dirty == 1 ]]; then printf 'This run included uncommitted changes; see the recorded hashes in the full report.\n\n'; fi
    awk -F, 'NR>1 && ($2=="reads"||$2=="writes"||$2=="mixed"){samples++;seen[$1]=1;value[$1,$2]=$4} END{if(!samples){print "No throughput samples in this run.";exit}split("reddish redis valkey memcached",servers," ");split("reads writes mixed",work," ");printf "| Workload |";for(i=1;i<=4;i++)if(seen[servers[i]])printf " %s ops/s |",servers[i];printf "\n|---|";for(i=1;i<=4;i++)if(seen[servers[i]])printf "---:|";printf "\n";for(j=1;j<=3;j++){printf "| %s |",work[j];for(i=1;i<=4;i++)if(seen[servers[i]]){if((servers[i],work[j]) in value)printf " %.0f |",value[servers[i],work[j]];else printf " — |"}printf "\n"}}' "$RESULTS_DIR/summary.csv"
    printf '\n| Shape | Server | RSS MiB | Delta bytes/key |\n|---|---|---:|---:|\n'
    tail -n +2 "$RESULTS_DIR/memory-summary.csv" | sort -t, -k2,2 -k1,1 | awk -F, '{printf "| %s | %s | %.2f | %.2f |\n",$2,$1,$7,$4}'
    printf '\n<!-- benchmark-summary:end -->\n'
  } > "$summary"
  awk -v summary="$summary" '/^<!-- benchmark-summary:start -->$/{while((getline line < summary)>0)print line;close(summary);inside=1;next} /^<!-- benchmark-summary:end -->$/{inside=0;next} !inside{print}' "$README_FILE" > "$RESULTS_DIR/readme.tmp"
  cp "$RESULTS_DIR/readme.tmp" "$README_FILE"
fi
printf 'Report: %s\n' "$OUTPUT_FILE"
