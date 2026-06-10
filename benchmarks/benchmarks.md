# Redis, Valkey, and reddish benchmarks

Run five trials on one Linux or WSL2 host with `bash benchmarks/run_local.sh`. The generated `summary.csv` contains the average for each server and workload. Raw logs and command output stay in `results/<timestamp>/temp/` and are never committed.

The default workloads are `reads` (100% GET), `writes` (100% SET), and `mixed` (50% GET, 50% SET). Raw benchmark results are intentionally not committed because logs may contain machine-specific information.

This is a narrow loopback benchmark using 32-byte values, one memtier worker, ten clients, pipeline depth 16, and no timed persistence. It must not be presented as a general Redis or Valkey comparison.

## Published results

No five-trial result has been published yet. Add only reviewed aggregate values here after a clean five-trial run.
