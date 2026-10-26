#!/usr/bin/env bash
set -euo pipefail
binary=${1:?Pass the fuzzer executable}
out=${2:?Pass a fresh output directory}
[[ ! -e $out/fuzz.log ]]
mkdir -p "$out/corpus" "$out/artifacts"
printf '*1\r\n\0444\r\nPING\r\n' > "$out/corpus/ping"
printf '*3\r\n\0443\r\nSET\r\n\0443\r\na\0b\r\n\0445\r\nx\r\ny\0\r\n' > "$out/corpus/binary"
printf '*4\r\n\0445\r\nLPUSH\r\n\0441\r\nl\r\n\0441\r\na\r\n\0441\r\nb\r\n' > "$out/corpus/list"
printf '*4\r\n\0444\r\nHSET\r\n\0441\r\nh\r\n\0441\r\nf\r\n\0441\r\nv\r\n' > "$out/corpus/hash"
"$binary" "$out/corpus" -max_total_time="${FUZZ_SECONDS:-120}" -max_len=65536 -rss_limit_mb=2048 -artifact_prefix="$out/artifacts/" -print_final_stats=1 > "$out/fuzz.log" 2>&1
