#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULTS_DIR=${RESULTS_DIR:?Set RESULTS_DIR to a completed benchmark run}
OUTPUT_FILE=${OUTPUT_FILE:-"$ROOT_DIR/benchmarks/benchmarks.md"}
README_FILE=${README_FILE-"$ROOT_DIR/README.md"}
[[ -s "$RESULTS_DIR/summary.csv" && -f "$RESULTS_DIR/memory.csv" && -s "$RESULTS_DIR/manifest.txt" ]] || { echo 'Incomplete benchmark run' >&2; exit 1; }
revision=$(head -1 "$RESULTS_DIR/manifest.txt")
report="$RESULTS_DIR/report.md"
{
  printf '# Benchmark results\n\n'
  printf 'Measured revision: [%s](https://github.com/amitverma-cf/reddish/commit/%s).\n\n' "${revision:0:7}" "$revision"
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
  printf '\n## Notes\n\n'
  printf -- '- This is a single-machine loopback test. WSL host scheduling and clock changes can cause large trial variance. Compare the ranges as well as the means.\n'
  printf -- '- GET and mixed samples must have zero misses. Preload key counts and fixed request counts are checked; failed requests invalidate the run.\n'
  printf -- '- Paired extended workloads report commands per second, not completed pairs. set_del runs SET/DEL, list pairs push/pop, hset_hdel inserts/deletes a field, and expire_ttl runs SET/EXPIRE/TTL.\n'
  if grep -q '^memcached,' "$RESULTS_DIR/results.csv"; then
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
  printf '\n</details>\n\n## Measurement manifest\n\n<details>\n<summary>Revision, executable hash, hardware, tools, and settings</summary>\n\n```text\n'
  cat "$RESULTS_DIR/manifest.txt"
  printf '```\n\n</details>\n'
} > "$report.tmp"
mv "$report.tmp" "$report"
mkdir -p "$(dirname "$OUTPUT_FILE")"
[[ $(realpath -m "$report") == "$(realpath -m "$OUTPUT_FILE")" ]] || cp "$report" "$OUTPUT_FILE"
if [[ -n $README_FILE ]]; then
  [[ $(grep -c '^<!-- benchmark-summary:start -->$' "$README_FILE") == 1 && $(grep -c '^<!-- benchmark-summary:end -->$' "$README_FILE") == 1 ]] || { echo 'README needs one benchmark marker pair' >&2; exit 1; }
  summary="$RESULTS_DIR/readme-summary.md"
  {
    printf '<!-- benchmark-summary:start -->\n### Benchmark results\n\n'
    printf 'Local loopback results; persistence disabled. [Full report](benchmarks/benchmarks.md) includes trial ranges, latency, CPU, memory, and setup.\n\n'
    awk -F, 'NR>1 && ($2=="reads"||$2=="writes"||$2=="mixed"){samples++;seen[$1]=1;value[$1,$2]=$4} END{if(!samples){print "No throughput samples in this run.";exit}split("reddish redis valkey memcached",servers," ");split("reads writes mixed",work," ");printf "| Workload |";for(i=1;i<=4;i++)if(seen[servers[i]])printf " %s ops/s |",servers[i];printf "\n|---|";for(i=1;i<=4;i++)if(seen[servers[i]])printf "---:|";printf "\n";for(j=1;j<=3;j++){printf "| %s |",work[j];for(i=1;i<=4;i++)if(seen[servers[i]]){if((servers[i],work[j]) in value)printf " %.0f |",value[servers[i],work[j]];else printf " — |"}printf "\n"}}' "$RESULTS_DIR/summary.csv"
    printf '\n| Shape | Server | RSS MiB | Delta bytes/key |\n|---|---|---:|---:|\n'
    tail -n +2 "$RESULTS_DIR/memory-summary.csv" | sort -t, -k2,2 -k1,1 | awk -F, '{printf "| %s | %s | %.2f | %.2f |\n",$2,$1,$7,$4}'
    printf '\n<!-- benchmark-summary:end -->\n'
  } > "$summary"
  awk -v summary="$summary" '/^<!-- benchmark-summary:start -->$/{while((getline line < summary)>0)print line;close(summary);inside=1;next} /^<!-- benchmark-summary:end -->$/{inside=0;next} !inside{print}' "$README_FILE" > "$RESULTS_DIR/readme.tmp"
  cp "$RESULTS_DIR/readme.tmp" "$README_FILE"
fi
printf 'Report: %s\n' "$OUTPUT_FILE"
