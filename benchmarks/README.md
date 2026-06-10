# Linux benchmarks

Run on Linux or WSL2 after installing `memtier_benchmark`, Redis, and Valkey. Native Windows is unsupported.

The runner uses the same keyspace, value size, client count, pipeline depth, warm-up preload, duration, and five trials for reddish, Redis, and Valkey. It disables Redis/Valkey persistence so the comparison measures command-path performance; reddish uses a one-hour dump interval so no snapshot happens during a run.

Each run writes reviewed aggregates (`results.csv`, `summary.csv`) beside a `temp/` directory containing raw logs and memtier output. The entire run directory stays untracked; copy only reviewed aggregate results into `benchmarks.md` for publication.

```bash
KEYS=100000 VALUE_BYTES=64 CLIENTS=50 PIPELINE=32 DURATION_SECONDS=60 bash benchmarks/run_local.sh
```
