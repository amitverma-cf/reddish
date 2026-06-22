#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULTS_DIR=${RESULTS_DIR:-"$ROOT_DIR/benchmarks/results"}
OUTPUT_FILE=${OUTPUT_FILE:-"$ROOT_DIR/benchmarks/benchmarks.md"}
README_FILE=${README_FILE:-"$ROOT_DIR/README.md"}
if [[ -f "$RESULTS_DIR/results.csv" ]]; then
  files=("$RESULTS_DIR/results.csv")
else
  mapfile -t files < <(find "$RESULTS_DIR" -mindepth 2 -maxdepth 2 -name results.csv -print | sort)
fi
if ((${#files[@]} == 0)); then echo "No results.csv files found" >&2; exit 1; fi

combined=$(mktemp)
combined_md=$(mktemp)
readme_md=$(mktemp)
trap 'rm -f "$combined" "$combined_md" "$readme_md"' EXIT
head -n 1 "${files[0]}" >"$combined"
for file in "${files[@]}"; do tail -n +2 "$file" >>"$combined"; done

summary=$(awk -F, 'NR > 1 { w=$2; if(w=="0:1")w="reads"; if(w=="1:0")w="writes"; if(w=="1:1")w="mixed"; k=$1 SUBSEP w; n[k]++; ops[k]+=$4; p99[k]+=$7; if(!(k in min)||$4<min[k])min[k]=$4; if(!(k in max)||$4>max[k])max[k]=$4 } END { for(k in n) {split(k,a,SUBSEP); printf "%s,%s,%d,%.0f,%.0f,%.0f,%.3f\n",a[1],a[2],n[k],ops[k]/n[k],min[k],max[k],p99[k]/n[k]} }' "$combined" | sort)

{
  echo '<!-- generated-results:start -->'
  echo '## Published results'
  echo
  echo "Aggregated from ${#files[@]} local run directories."
  echo
  echo '### System and toolchain'
  echo
  echo "- OS: $(source /etc/os-release && printf '%s %s' "$NAME" "$VERSION_ID")"
  echo "- Kernel: $(uname -r)"
  echo "- CPU: $(lscpu | awk -F: '/Model name:/ {sub(/^[[:space:]]+/,"",$2); print $2; exit}') ($(nproc) logical CPUs)"
  echo "- Memory: $(awk '/MemTotal:/ {printf "%.0f",$2/1024}' /proc/meminfo) MiB"
  echo "- Compiler: $(c++ --version | head -n 1)"
  echo
  echo '| Workload | Server | Samples | Average ops/s | Min ops/s | Max ops/s | Average p99 ms |'
  echo '|---|---|---:|---:|---:|---:|---:|'
  while IFS=, read -r server workload samples average minimum maximum p99; do printf '| %s | %s | %s | %s | %s | %s | %s |\n' "$workload" "$server" "$samples" "$average" "$minimum" "$maximum" "$p99"; done <<<"$summary"
  echo
  echo '<!-- generated-results:end -->'
} >"$combined_md"

awk -v generated="$combined_md" '/<!-- generated-results:start -->/ {while((getline line < generated)>0) print line; close(generated); skip=1; next} /<!-- generated-results:end -->/ {skip=0; next} !skip {print}' "$OUTPUT_FILE" >"$OUTPUT_FILE.tmp"
mv "$OUTPUT_FILE.tmp" "$OUTPUT_FILE"

{
  echo '<!-- benchmark-summary:start -->'
  echo '### Latest benchmark summary'
  echo
  echo 'Generated from the aggregate in [the full benchmark report](benchmarks/benchmarks.md). This is a loopback GET/SET measurement, not a general Redis or Valkey comparison.'
  echo
  echo '| Workload | Reddish ops/s | Redis ops/s | Valkey ops/s |'
  echo '|---|---:|---:|---:|'
  printf '%s\n' "$summary" | awk -F, '
    { values[$2 SUBSEP $1] = $4 }
    function throughput(value) { return sprintf("%.0fk", value / 1000) }
    END {
      print "| Read (GET) | " throughput(values["reads" SUBSEP "reddish"]) " | " throughput(values["reads" SUBSEP "redis"]) " | " throughput(values["reads" SUBSEP "valkey"]) " |"
      print "| Write (SET) | " throughput(values["writes" SUBSEP "reddish"]) " | " throughput(values["writes" SUBSEP "redis"]) " | " throughput(values["writes" SUBSEP "valkey"]) " |"
      print "| Mixed (50/50) | " throughput(values["mixed" SUBSEP "reddish"]) " | " throughput(values["mixed" SUBSEP "redis"]) " | " throughput(values["mixed" SUBSEP "valkey"]) " |"
    }'
  echo
  echo '<!-- benchmark-summary:end -->'
} >"$readme_md"

awk -v generated="$readme_md" '/<!-- benchmark-summary:start -->/ {while((getline line < generated)>0) print line; close(generated); skip=1; next} /<!-- benchmark-summary:end -->/ {skip=0; next} !skip {print}' "$README_FILE" >"$README_FILE.tmp"
mv "$README_FILE.tmp" "$README_FILE"
echo "Published aggregate results to $OUTPUT_FILE and $README_FILE"
