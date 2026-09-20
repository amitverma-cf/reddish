#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULTS_DIR=${RESULTS_DIR:-"$ROOT_DIR/benchmarks/results"}
OUTPUT_FILE=${OUTPUT_FILE:-"$ROOT_DIR/benchmarks/benchmarks.md"}
README_FILE=${README_FILE:-"$ROOT_DIR/README.md"}
result_files=()
memory_files=()

if [[ -f "$RESULTS_DIR/results.csv" && -f "$RESULTS_DIR/memory.csv" ]]; then
  result_files=("$RESULTS_DIR/results.csv")
  memory_files=("$RESULTS_DIR/memory.csv")
else
  while IFS= read -r result_file; do
    run_dir=$(dirname "$result_file")
    memory_file="$run_dir/memory.csv"
    if [[ -f "$memory_file" ]]; then
      result_files+=("$result_file")
      memory_files+=("$memory_file")
    fi
  done < <(find "$RESULTS_DIR" -mindepth 2 -maxdepth 2 -name results.csv -print | sort)
fi

if ((${#result_files[@]} == 0)); then
  echo 'No complete benchmark runs found (each run needs results.csv and memory.csv)' >&2
  exit 1
fi

combined=$(mktemp); memory=$(mktemp); generated=$(mktemp); root_summary=$(mktemp)
trap 'rm -f "$combined" "$memory" "$generated" "$root_summary"' EXIT
head -n 1 "${result_files[0]}" >"$combined"; for file in "${result_files[@]}"; do tail -n +2 "$file" >>"$combined"; done
head -n 1 "${memory_files[0]}" >"$memory"; for file in "${memory_files[@]}"; do tail -n +2 "$file" >>"$memory"; done

summary=$(awk -F, 'NR>1 {k=$1 SUBSEP $2;n[k]++;ops[k]+=$4;p99[k]+=$7;if(!(k in lo)||$4<lo[k])lo[k]=$4;if(!(k in hi)||$4>hi[k])hi[k]=$4} END {for(k in n){split(k,a,SUBSEP);printf "%s,%s,%d,%.0f,%.0f,%.0f,%.3f\n",a[1],a[2],n[k],ops[k]/n[k],lo[k],hi[k],p99[k]/n[k]}}' "$combined" | sort)
memory_summary=$(awk -F, 'NR>1 {k=$1 SUBSEP $2;n[k]++;idle[k]+=$4;rss[k]+=$5;delta[k]+=$6;bytes[k]+=$7} END {for(k in n){split(k,a,SUBSEP);printf "%s,%s,%d,%.0f,%.0f,%.0f,%.2f\n",a[1],a[2],n[k],idle[k]/n[k],rss[k]/n[k],delta[k]/n[k],bytes[k]/n[k]}}' "$memory" | sort)

{
  echo '<!-- generated-results:start -->'; echo '## Published results'; echo
  echo "Aggregated from ${#result_files[@]} benchmark run directories."; echo
  echo '### System and toolchain'; echo
  echo "- OS: $(source /etc/os-release && printf '%s %s' "$NAME" "$VERSION_ID")"
  echo "- Kernel: $(uname -r)"; echo "- CPU: $(lscpu | awk -F: '/Model name:/ {sub(/^[[:space:]]+/,"",$2); print $2; exit}') ($(nproc) logical CPUs)"
  echo "- Memory: $(awk '/MemTotal:/ {printf "%.0f",$2/1024}' /proc/meminfo) MiB"; echo "- Compiler: $(c++ --version | head -n 1)"; echo
  echo '### Command throughput'; echo; echo '| Workload | Server | Samples | Average ops/s | Min ops/s | Max ops/s | Average p99 ms |'; echo '|---|---|---:|---:|---:|---:|---:|'
  while IFS=, read -r server workload samples average minimum maximum p99; do printf '| %s | %s | %s | %s | %s | %s | %s |\n' "$workload" "$server" "$samples" "$average" "$minimum" "$maximum" "$p99"; done <<<"$summary"
  echo; echo '### Resident memory after preload'; echo; echo 'RSS includes the process and allocator/runtime overhead. Delta RSS is after-preload RSS minus idle RSS.'; echo
  echo '| Value shape | Server | Samples | Idle RSS KiB | RSS KiB | Delta RSS KiB | Bytes/key |'; echo '|---|---|---:|---:|---:|---:|---:|'
  while IFS=, read -r server shape samples idle rss delta bytes; do printf '| %s | %s | %s | %s | %s | %s | %s |\n' "$shape" "$server" "$samples" "$idle" "$rss" "$delta" "$bytes"; done <<<"$memory_summary"
  echo; echo '<!-- generated-results:end -->'
} >"$generated"
awk -v generated="$generated" '/<!-- generated-results:start -->/ {while((getline line < generated)>0) print line;close(generated);skip=1;next} /<!-- generated-results:end -->/ {skip=0;next} !skip {print}' "$OUTPUT_FILE" >"$OUTPUT_FILE.tmp"; mv "$OUTPUT_FILE.tmp" "$OUTPUT_FILE"

{
  echo '<!-- benchmark-summary:start -->'; echo '### Latest benchmark summary'; echo
  echo 'Generated from [the full benchmark report](benchmarks/benchmarks.md). This is a loopback measurement, not a general server comparison. Memcached is a string-cache baseline and is measured only for GET and SET.'; echo
  echo '| Workload | Reddish ops/s | Redis ops/s | Valkey ops/s |'; echo '|---|---:|---:|---:|'
  printf '%s\n' "$summary" | awk -F, '{v[$2 SUBSEP $1]=$4} function f(x){return sprintf("%.4fm",x/1000000)} END {print "| Read (GET) | " f(v["reads" SUBSEP "reddish"]) " | " f(v["reads" SUBSEP "redis"]) " | " f(v["reads" SUBSEP "valkey"]) " |"; print "| Write (SET) | " f(v["writes" SUBSEP "reddish"]) " | " f(v["writes" SUBSEP "redis"]) " | " f(v["writes" SUBSEP "valkey"]) " |"; print "| Mixed (50/50) | " f(v["mixed" SUBSEP "reddish"]) " | " f(v["mixed" SUBSEP "redis"]) " | " f(v["mixed" SUBSEP "valkey"]) " |"}'
  echo; echo '| Simple-cache baseline | Memcached ops/s |'; echo '|---|---:|'
  printf '%s\n' "$summary" | awk -F, '{v[$2 SUBSEP $1]=$4} function f(x){return sprintf("%.4fm",x/1000000)} END {print "| Read (GET) | " f(v["reads" SUBSEP "memcached"]) " |"; print "| Write (SET) | " f(v["writes" SUBSEP "memcached"]) " |"}'
  echo; echo '<!-- benchmark-summary:end -->'
} >"$root_summary"
awk -v generated="$root_summary" '/<!-- benchmark-summary:start -->/ {while((getline line < generated)>0) print line;close(generated);skip=1;next} /<!-- benchmark-summary:end -->/ {skip=0;next} !skip {print}' "$README_FILE" >"$README_FILE.tmp"; mv "$README_FILE.tmp" "$README_FILE"
echo "Published aggregate results to $OUTPUT_FILE and $README_FILE"
