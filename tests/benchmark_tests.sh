#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TASK_DIR=$(mktemp -d /tmp/reddish-benchmark.XXXXXXXX)
trap '[[ $TASK_DIR == /tmp/reddish-benchmark.* ]] && rm -rf -- "$TASK_DIR"' EXIT
trap 'echo "Benchmark regression failed at line $LINENO" >&2' ERR
source "$ROOT_DIR/benchmarks/common.sh"
run="$TASK_DIR/samples"
mkdir -p "$run/temp"
printf '%040d\n' 0 > "$run/manifest.txt"
printf 'Measured at test\nKEYS=100 VALUE_BYTES=32 THREADS=1 CLIENTS=1 PIPELINE=1 TRIALS=1 MEMORY_TRIALS=1 RUN_EXTENDED=0 RUN_PERFORMANCE=1 RUN_MEMORY=0 REQUESTS_PER_CLIENT=0\n' >> "$run/manifest.txt"
printf 'server,workload,trial,ops_per_sec,avg_latency_ms,p50_latency_ms,p99_latency_ms,p999_latency_ms\n' > "$run/results.csv"
printf 'server,workload,samples,average_ops_per_sec,average_p99_latency_ms,min_ops_per_sec,max_ops_per_sec,stddev_ops_per_sec\n' > "$run/summary.csv"
printf 'server,workload,trial,cpu_seconds,operation_count,cpu_us_per_op\n' > "$run/cpu.csv"
printf 'server,workload,trial,workload_seconds,server_cpu_seconds,client_cpu_seconds,client_max_rss_kib,main_runtime_seconds,main_runqueue_seconds,main_blocked_seconds,main_voluntary_switches,main_involuntary_switches,operation_count,client_elapsed_seconds\n' > "$run/process.csv"
printf 'server,scenario,trial,key_count,idle_rss_kib,rss_kib,delta_rss_kib,bytes_per_key\n' > "$run/memory.csv"
printf 'server,scenario,samples,mean_bytes_per_key,min_bytes_per_key,max_bytes_per_key,mean_total_rss_mib\n' > "$run/memory-summary.csv"
for server in reddish redis valkey; do
  for workload in reads writes mixed; do
    raw="$run/temp/$server-$workload-1.txt.json"
    jq -n '{configuration:{clients:1,threads:1,pipeline:1,key_minimum:1,key_maximum:100,data_size:32},"ALL STATS":{Runtime:{"Total duration":1000},Totals:{Count:1000,"Ops/sec":1000,"Average Latency":0.1,"Misses/sec":0,"Connection Errors":0,"Percentile Latencies":{"p50.00":0.1,"p99.00":0.1,"p99.90":0.1}}}}' > "$raw"
    touch "${raw%.json}.failed.csv"
    printf '%s,%s,1,1000,0.1,0.1,0.1,0.1\n' "$server" "$workload" >> "$run/results.csv"
    printf '%s,%s,1,1000,0.1,1000,1000,0\n' "$server" "$workload" >> "$run/summary.csv"
    printf '%s,%s,1,0.01,1000,10\n' "$server" "$workload" >> "$run/cpu.csv"
    printf '%s,%s,1,1,0.01,0.02,1024,NA,NA,NA,0,0,1000,1.01\n' "$server" "$workload" >> "$run/process.csv"
  done
done
bash "$ROOT_DIR/benchmarks/validate_results.sh" "$run"
raw="$run/temp/reddish-reads-1.txt.json"
cp "$raw" "$TASK_DIR/original.json"
for workload in reads mixed hget hlen llen; do
  jq '.["ALL STATS"].Totals["Misses/sec"]=1' "$TASK_DIR/original.json" > "$raw"
  if read_stats "$raw" "$workload" >/dev/null 2>&1; then echo "Accepted misses: $workload" >&2; exit 1; fi
done
cp "$TASK_DIR/original.json" "$raw"
mv "${raw%.json}.failed.csv" "$TASK_DIR/failed.csv"
if read_stats "$raw" reads >/dev/null 2>&1; then echo 'Accepted missing failure log' >&2; exit 1; fi
mv "$TASK_DIR/failed.csv" "${raw%.json}.failed.csv"
jq '.configuration.clients=2' "$TASK_DIR/original.json" > "$raw"
if bash "$ROOT_DIR/benchmarks/validate_results.sh" "$run" >/dev/null 2>&1; then echo 'Accepted wrong workload settings' >&2; exit 1; fi
cp "$TASK_DIR/original.json" "$raw"
cp "$run/summary.csv" "$TASK_DIR/original.csv"
sed '2s/,1000,/,9999,/' "$TASK_DIR/original.csv" > "$run/summary.csv"
if bash "$ROOT_DIR/benchmarks/validate_results.sh" "$run" >/dev/null 2>&1; then echo 'Accepted wrong summary' >&2; exit 1; fi
cp "$TASK_DIR/original.csv" "$run/summary.csv"
cp "$run/process.csv" "$TASK_DIR/original-process.csv"
sed '2s/,1,1,/,1,2,/' "$TASK_DIR/original-process.csv" > "$run/process.csv"
if bash "$ROOT_DIR/benchmarks/validate_results.sh" "$run" >/dev/null 2>&1; then echo 'Accepted wrong measurement duration' >&2; exit 1; fi
cp "$TASK_DIR/original-process.csv" "$run/process.csv"
for copy in one two; do
  printf 'Before\n<!-- benchmark-summary:start -->\nold\n<!-- benchmark-summary:end -->\nAfter\n' > "$TASK_DIR/$copy-README.md"
  RESULTS_DIR="$run" OUTPUT_FILE="$TASK_DIR/$copy-report.md" README_FILE="$TASK_DIR/$copy-README.md" bash "$ROOT_DIR/benchmarks/publish_results.sh" >/dev/null
done
cmp "$TASK_DIR/one-report.md" "$TASK_DIR/two-report.md"
cmp "$TASK_DIR/one-README.md" "$TASK_DIR/two-README.md"
printf '<!-- benchmark-summary:end -->\n<!-- benchmark-summary:start -->\n' > "$TASK_DIR/bad-README.md"
if RESULTS_DIR="$run" OUTPUT_FILE="$TASK_DIR/one-report.md" README_FILE="$TASK_DIR/bad-README.md" bash "$ROOT_DIR/benchmarks/publish_results.sh" >/dev/null 2>&1; then echo 'Accepted reversed README markers' >&2; exit 1; fi
cmp "$TASK_DIR/one-report.md" "$TASK_DIR/two-report.md"
git_test() { git -c user.name='Benchmark test' -c user.email='benchmark-test@example.invalid' -c core.autocrlf=false "$@"; }
for scenario in publish stale wrong_run staged_code; do
  case_dir="$TASK_DIR/$scenario"
  mkdir -p "$case_dir/work/benchmarks/results/comparison"
  git_test init -q --bare "$case_dir/remote.git"
  git_test -C "$case_dir/work" init -q -b main
  cp "$ROOT_DIR/benchmarks/publish_actions.sh" "$case_dir/work/benchmarks/"
  printf 'old\n' > "$case_dir/work/README.md"
  printf 'old\n' > "$case_dir/work/benchmarks/benchmarks.md"
  printf 'benchmarks/results/\n' > "$case_dir/work/.gitignore"
  git_test -C "$case_dir/work" add .
  git_test -C "$case_dir/work" commit -qm Source
  revision=$(git_test -C "$case_dir/work" rev-parse HEAD)
  git_test -C "$case_dir/work" remote add origin "$case_dir/remote.git"
  git_test -C "$case_dir/work" push -q origin HEAD:main
  expected=$revision
  printf 'Measured %s\n' "$revision" > "$case_dir/work/README.md"
  printf 'Report %s\n' "$revision" > "$case_dir/work/benchmarks/benchmarks.md"
  url=https://github.com/fixture/reddish/actions/runs/123
  [[ $scenario != wrong_run ]] || url=https://github.com/fixture/reddish/actions/runs/456
  jq -n --arg revision "$revision" --arg run_url "$url" '{revision:$revision,run_url:$run_url}' > "$case_dir/work/benchmarks/results/comparison/provenance.json"
  if [[ $scenario == stale ]]; then
    git_test clone -q --branch main "$case_dir/remote.git" "$case_dir/other"
    printf 'new work\n' > "$case_dir/other/new-code"
    git_test -C "$case_dir/other" add .
    git_test -C "$case_dir/other" commit -qm 'New work'
    git_test -C "$case_dir/other" push -q origin HEAD:main
    expected=$(git_test -C "$case_dir/other" rev-parse HEAD)
  fi
  if [[ $scenario == staged_code ]]; then
    printf 'unrelated\n' > "$case_dir/work/unrelated-code"
    git_test -C "$case_dir/work" add unrelated-code
  fi
  status=0
  (cd "$case_dir/work"; GITHUB_ACTIONS=true GITHUB_REF=refs/heads/main GITHUB_REPOSITORY=fixture/reddish GITHUB_RUN_ID=123 EXPECTED_SHA="$revision" bash benchmarks/publish_actions.sh) > "$case_dir/output.log" 2>&1 || status=$?
  actual=$(git_test --git-dir="$case_dir/remote.git" rev-parse refs/heads/main)
  if [[ $scenario == publish ]]; then
    [[ $status == 0 && $actual != "$revision" ]]
    [[ $(git_test -C "$case_dir/work" diff --name-only "$revision" "$actual") == $'README.md\nbenchmarks/benchmarks.md' ]]
    [[ $(git_test -C "$case_dir/work" rev-parse "$actual^") == "$revision" ]]
  else
    [[ $status != 0 && $actual == "$expected" ]]
  fi
done
printf 'Benchmark validation, report reproducibility and guarded publication tests passed.\n'
