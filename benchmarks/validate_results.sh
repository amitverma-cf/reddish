#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
RESULTS_DIR=${1:?Pass one completed run}
for file in results.csv summary.csv memory.csv memory-summary.csv cpu.csv manifest.txt; do
  [[ -f "$RESULTS_DIR/$file" ]] || { echo "Missing $file" >&2; exit 1; }
done
[[ $(head -1 "$RESULTS_DIR/manifest.txt") =~ ^[0-9a-f]{40}$ ]] || { echo 'Missing measured revision' >&2; exit 1; }
manifest_value() { grep -oE "(^|[[:space:]])$1=[^[:space:]]+" "$RESULTS_DIR/manifest.txt" | tail -1 | cut -d= -f2 || true; }
trials=$(manifest_value TRIALS); memory_trials=$(manifest_value MEMORY_TRIALS)
extended=$(manifest_value RUN_EXTENDED); performance=$(manifest_value RUN_PERFORMANCE); memory=$(manifest_value RUN_MEMORY)
[[ -n $performance ]] || performance=$(( $(wc -l < "$RESULTS_DIR/results.csv") > 1 ))
[[ -n $memory ]] || memory=$(( $(wc -l < "$RESULTS_DIR/memory.csv") > 1 ))
servers='reddish redis valkey'
if grep -q '^memcached ' "$RESULTS_DIR/manifest.txt"; then servers+=' memcached'; fi
[[ $trials =~ ^[1-9][0-9]*$ && $memory_trials =~ ^[1-9][0-9]*$ && $extended =~ ^[01]$ && $performance =~ ^[01]$ && $memory =~ ^[01]$ ]] || { echo 'Missing run settings' >&2; exit 1; }
awk -F, -v servers="$servers" -v trials="$trials" -v enabled="$performance" -v extended="$extended" 'NR>1{n[$1,$2]++;if(!enabled)exit 1} END{if(enabled){split(servers,s," ");split("reads writes mixed ping incr set_del lpush_lpop rpush_rpop llen hget hlen hset_hdel expire_ttl",w," ");for(i in s)for(j=1;j<=(s[i]=="memcached"?2:extended?13:3);j++)if(n[s[i],w[j]]!=trials)exit 1}}' "$RESULTS_DIR/results.csv" || { echo 'Missing or incomplete workloads' >&2; exit 1; }
awk -F, -v servers="$servers" -v trials="$memory_trials" -v enabled="$memory" 'NR>1{n[$1,$2]++;if(!enabled)exit 1} END{if(enabled){split(servers,s," ");split("strings lists hashes",w," ");for(i in s)for(j=1;j<=(s[i]=="memcached"?1:3);j++)if(n[s[i],w[j]]!=trials)exit 1}}' "$RESULTS_DIR/memory.csv" || { echo 'Missing or incomplete memory shapes' >&2; exit 1; }
clients=$(manifest_value CLIENTS); threads=$(manifest_value THREADS); pipeline=$(manifest_value PIPELINE)
keys=$(manifest_value KEYS); bytes=$(manifest_value VALUE_BYTES); requests=$(manifest_value REQUESTS_PER_CLIENT)
for setting in clients threads pipeline keys bytes; do [[ ${!setting} =~ ^[1-9][0-9]*$ ]] || { echo "Invalid $setting" >&2; exit 1; }; done
[[ $requests =~ ^[0-9]+$ ]] || { echo 'Invalid request limit' >&2; exit 1; }
[[ $(wc -l < "$RESULTS_DIR/cpu.csv") == "$(wc -l < "$RESULTS_DIR/results.csv")" ]] || { echo 'CPU row count differs' >&2; exit 1; }
if [[ -f "$RESULTS_DIR/process.csv" ]]; then
  [[ $(wc -l < "$RESULTS_DIR/process.csv") == "$(wc -l < "$RESULTS_DIR/results.csv")" ]] || { echo 'Process row count differs' >&2; exit 1; }
fi
awk -F, 'FNR==1{next} FILENAME==ARGV[1]{if(NF!=8||$3<1||$4<=0||$5<0||$6<0||$7<0||$8<0||seen[$1,$2,$3]++)exit 1;k=$1 FS $2;n[k]++;sum[k]+=$4;sq[k]+=$4*$4;p99[k]+=$7;if(!(k in lo)||$4<lo[k])lo[k]=$4;if(!(k in hi)||$4>hi[k])hi[k]=$4;next} {k=$1 FS $2;if(NF!=8||checked[k]++||!(k in n)||$3!=n[k])exit 1;m=sum[k]/n[k];v=n[k]>1?(sq[k]-n[k]*m*m)/(n[k]-1):0;if(v<0)v=0;if(($4-m)^2>.0005||($5-p99[k]/n[k])^2>.000000001||($6-lo[k])^2>.0005||($7-hi[k])^2>.0005||($8-sqrt(v))^2>.0005)exit 1} END{for(k in n)if(!(k in checked))exit 1}' "$RESULTS_DIR/results.csv" "$RESULTS_DIR/summary.csv" || { echo 'Inconsistent throughput summary' >&2; exit 1; }
while IFS=, read -r server workload trial ops latency p50 p99 p999; do
  [[ $server != server ]] || continue
  [[ $server =~ ^(reddish|redis|valkey|memcached)$ && $workload =~ ^[a-z_]+$ && $trial =~ ^[1-9][0-9]*$ ]] || { echo 'Invalid sample identity' >&2; exit 1; }
  raw="$RESULTS_DIR/temp/$server-$workload-$trial.txt.json"
  expected=0
  if [[ $workload == reads || $workload == writes || $workload == mixed ]]; then expected=$((requests*clients*threads)); fi
  values=$(read_stats "$raw" "$workload" "$expected")
  jq -e --argjson clients "$clients" --argjson threads "$threads" --argjson pipeline "$pipeline" --argjson keys "$keys" --argjson bytes "$bytes" '.configuration | .clients==$clients and .threads==$threads and .pipeline==$pipeline and .key_minimum==1 and .key_maximum==$keys and .data_size==$bytes' "$raw" >/dev/null || { echo "Workload settings differ: $raw" >&2; exit 1; }
  IFS=, read -r actual_ops actual_latency actual_p50 actual_p99 actual_p999 count <<< "$values"
  awk -v a="$ops" -v b="$actual_ops" -v c="$latency" -v d="$actual_latency" -v e="$p50" -v f="$actual_p50" -v g="$p99" -v h="$actual_p99" -v i="$p999" -v j="$actual_p999" 'BEGIN{exit ((a-b)^2>.0005||(c-d)^2>.000001||(e-f)^2>.000001||(g-h)^2>.000001||(i-j)^2>.000001)}' || { echo "Raw sample differs: $raw" >&2; exit 1; }
  elapsed=$(jq -er '.["ALL STATS"].Runtime["Total duration"] / 1000 | select(. > 0)' "$raw")
  if [[ -f "$RESULTS_DIR/process.csv" ]]; then
    awk -F, -v s="$server" -v w="$workload" -v t="$trial" -v count="$count" -v elapsed="$elapsed" 'NR>1&&$1==s&&$2==w&&$3==t{n++;if(($4-elapsed)^2>.000001||NF!=14||$4<=0||$5<0||$6<0||$7<0||$11<0||$12<0||$13!=count||$14+0.02<$4)exit 1;for(i=8;i<=10;i++)if($i!="NA"&&$i<0)exit 1} END{if(n!=1)exit 1}' "$RESULTS_DIR/process.csv" || { echo "Process metrics differ: $raw" >&2; exit 1; }
  fi
  awk -F, -v s="$server" -v w="$workload" -v t="$trial" -v count="$count" 'NR>1&&$1==s&&$2==w&&$3==t{n++;if($4<0||$5!=count)exit 1} END{if(n!=1)exit 1}' "$RESULTS_DIR/cpu.csv" || { echo "CPU sample differs: $raw" >&2; exit 1; }
done < "$RESULTS_DIR/results.csv"
awk -F, 'FNR==1{next} FILENAME==ARGV[1]{if(NF!=8||$3<1||$4<=0||$5<0||$6<0||seen[$1,$2,$3]++)exit 1;d=$6-$5;if(d<0)d=0;if(($7-d)^2>.001||($8-d*1024/$4)^2>.0005)exit 1;k=$1 FS $2;if(k in keys&&keys[k]!=$4)exit 1;keys[k]=$4;n[k]++;sum[k]+=$8;rss[k]+=$6;if(!(k in lo)||$8<lo[k])lo[k]=$8;if(!(k in hi)||$8>hi[k])hi[k]=$8;next} {k=$1 FS $2;if(NF!=7||checked[k]++||!(k in n)||$3!=n[k])exit 1;if(($4-sum[k]/n[k])^2>.0005||($5-lo[k])^2>.0005||($6-hi[k])^2>.0005||($7-rss[k]/n[k]/1024)^2>.000001)exit 1} END{for(k in n)if(!(k in checked))exit 1}' "$RESULTS_DIR/memory.csv" "$RESULTS_DIR/memory-summary.csv" || { echo 'Inconsistent memory samples' >&2; exit 1; }
printf 'Validated run: %s\n' "$RESULTS_DIR" >&2
