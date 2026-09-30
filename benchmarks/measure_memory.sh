#!/usr/bin/env bash
# Reuse the same repeated, validated RSS measurements as the throughput runner.
# KEYS, VALUE_BYTES, MEMORY_TRIALS, REDDISH_BINARY and RESULT_DIR are inherited.
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export RUN_PERFORMANCE=0 RUN_MEMORY=1 RUN_EXTENDED=0
exec bash "$ROOT_DIR/benchmarks/run_local.sh"
