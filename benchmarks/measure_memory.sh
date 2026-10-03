#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export RUN_PERFORMANCE=0 RUN_MEMORY=1 RUN_EXTENDED=0
exec bash "$ROOT_DIR/benchmarks/run_local.sh"
