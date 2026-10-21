#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
source "$ROOT_DIR/benchmarks/common.sh"
RESULT_DIR=${RESULT_DIR:-"$ROOT_DIR/local/benchmarks/capacity-$(date +%Y%m%d-%H%M%S)"}
REDDISH_BINARY=${REDDISH_BINARY:-"$ROOT_DIR/build/output/reddish"}
DURATION_SECONDS=${DURATION_SECONDS:-5}
TRIALS=${TRIALS:-3}
P99_LIMIT_MS=${P99_LIMIT_MS:-2}
CAPACITY_POINTS=${CAPACITY_POINTS:-'1:1:1:32:100000:S 10:1:1:32:100000:S 10:1:16:32:100000:S 10:2:16:32:100000:S 100:2:16:32:100000:S 100:4:16:32:100000:S 100:2:64:32:100000:S 10:2:16:256:100000:S 10:2:16:4096:100000:S 10:2:16:32:1000000:S 10:2:16:32:100000:R'}
if ! [[ $P99_LIMIT_MS =~ ^[0-9]+([.][0-9]+)?$ ]] || ! awk -v n="$P99_LIMIT_MS" 'BEGIN{exit n<=0}'; then echo 'Invalid p99 limit' >&2; exit 1; fi
[[ ! -e "$RESULT_DIR/summary.csv" ]] || { echo 'Capacity output exists' >&2; exit 1; }
mkdir -p "$RESULT_DIR"
RESULT_DIR=$(realpath "$RESULT_DIR")
if [[ -z ${SERVER_CPU:-} && -z ${CLIENT_CPUS:-} ]]; then choose_cpus; fi
[[ -n ${SERVER_CPU:-} && -n ${CLIENT_CPUS:-} ]] || { echo 'Set both CPU affinity options' >&2; exit 1; }
client_slots=$(taskset -c "$CLIENT_CPUS" nproc)
{
  git -C "$ROOT_DIR" rev-parse HEAD
  date -u '+Measured at %Y-%m-%d %H:%M:%S UTC'
  uname -a
  printf 'SERVER_CPU=%s CLIENT_CPUS=%s P99_LIMIT_MS=%s TRIALS=%s DURATION_SECONDS=%s\n' "$SERVER_CPU" "$CLIENT_CPUS" "$P99_LIMIT_MS" "$TRIALS" "$DURATION_SECONDS"
  printf 'Blocked time is estimated for each server main thread; missing schedstat is NA. CPU per worker includes client process overhead.\n'
} > "$RESULT_DIR/manifest.txt"
printf 'case,server,workload,keys,value_bytes,total_clients,threads,pipeline,key_pattern,trials,mean_ops_per_sec,min_ops_per_sec,max_ops_per_sec,max_trial_p99_ms,server_cpu_pct,client_cpu_pct_per_worker,mean_main_blocked_seconds,slo_pass,client_cpu_pct_total,client_cpu_slots\n' > "$RESULT_DIR/summary.csv"
if [[ -z ${VALIDATION_DIR:-} ]]; then
  REDDISH_BINARY="$REDDISH_BINARY" RESULT_DIR="$RESULT_DIR/validation" bash "$ROOT_DIR/benchmarks/validate.sh"
else
  [[ -f $VALIDATION_DIR/complete && $(cat "$VALIDATION_DIR/revision.txt") == "$(git -C "$ROOT_DIR" rev-parse HEAD)" ]] || { echo 'Missing validation for this revision' >&2; exit 1; }
fi
for point in $CAPACITY_POINTS; do
  IFS=: read -r total threads pipeline bytes keys pattern <<< "$point"
  for setting in total threads pipeline bytes keys; do
    value=${!setting}; [[ $value =~ ^[1-9][0-9]*$ && ${#value} -le 9 ]] || { echo "Invalid capacity point: $point" >&2; exit 1; }
  done
  if [[ $pattern != S && $pattern != R ]] || ((total%threads!=0)); then echo "Invalid capacity point: $point" >&2; exit 1; fi
  name="c$total-t$threads-p$pipeline-v$bytes-k$keys-$pattern"
  case_dir="$RESULT_DIR/$name"
  REDDISH_BINARY="$REDDISH_BINARY" RESULT_DIR="$case_dir" KEYS="$keys" VALUE_BYTES="$bytes" THREADS="$threads" CLIENTS="$((total/threads))" PIPELINE="$pipeline" KEY_PATTERN="$pattern:$pattern" TRIALS="$TRIALS" DURATION_SECONDS="$DURATION_SECONDS" WARMUP_SECONDS=1 REQUESTS_PER_CLIENT=0 RUN_VALIDATION=0 RUN_EXTENDED=0 RUN_MEMORY=0 RUN_PERFORMANCE=1 INCLUDE_MEMCACHED=0 REQUIRE_MEMCACHED=0 OUTPUT_FILE="$case_dir/report.md" README_FILE='' bash "$ROOT_DIR/benchmarks/run_local.sh"
  awk -F, -v name="$name" -v clients="$total" -v threads="$threads" -v pipeline="$pipeline" -v bytes="$bytes" -v keys="$keys" -v pattern="$pattern:$pattern" -v limit="$P99_LIMIT_MS" -v slots="$client_slots" 'FNR==1{next} FILENAME==ARGV[1]{k=$1 FS $2;elapsed[k]+=$4;server[k]+=$5;client[k]+=$6;if($10!="NA"){blocked[k]+=$10;bcount[k]++}next} FILENAME==ARGV[2]{k=$1 FS $2;if($7>p99[k])p99[k]=$7;next} {k=$1 FS $2;printf "%s,%s,%d,%d,%d,%d,%d,%s,%d,%.2f,%.2f,%.2f,%.3f,%.2f,%.2f,%s,%s,%.2f,%d\n",name,k,keys,bytes,clients,threads,pipeline,pattern,$3,$4,$6,$7,p99[k],server[k]/elapsed[k]*100,client[k]/elapsed[k]/threads*100,(bcount[k]?sprintf("%.6f",blocked[k]/bcount[k]):"NA"),(p99[k]<=limit?"true":"false"),client[k]/elapsed[k]*100,slots}' "$case_dir/process.csv" "$case_dir/results.csv" "$case_dir/summary.csv" >> "$RESULT_DIR/summary.csv"
done
{
  head -1 "$RESULT_DIR/summary.csv"
  awk -F, 'NR>1&&$18=="true"{k=$2 FS $3 FS $4 FS $5 FS $9;if(!(k in best)||$11>ops[k]){best[k]=$0;ops[k]=$11}} END{for(k in best)print best[k]}' "$RESULT_DIR/summary.csv" | sort -t, -k2,2 -k3,3 -k4,4n -k5,5n
} > "$RESULT_DIR/best-observed.csv"
touch "$RESULT_DIR/complete"
printf 'Measured operating points: %s\n' "$RESULT_DIR/summary.csv"
