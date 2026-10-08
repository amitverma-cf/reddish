#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULTS_DIR=${RESULTS_DIR:?Set RESULTS_DIR to one completed benchmark run}
OUTPUT_FILE=${OUTPUT_FILE:-"$RESULTS_DIR/report.md"}
arguments=("$RESULTS_DIR" --output "$OUTPUT_FILE")
[[ -z ${README_FILE:-} ]] || arguments+=(--readme "$README_FILE")
exec python3 "$ROOT_DIR/benchmarks/report.py" "${arguments[@]}"
