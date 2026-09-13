# Linux benchmarks

Run on Linux after installing `memtier_benchmark`, Redis, and Valkey. Native Windows is unsupported.

The runner uses the same keyspace, value size, client count, pipeline depth, warm-up preload, duration, and five trials for reddish, Redis, and Valkey. It disables Redis/Valkey persistence so the comparison measures command-path performance; reddish uses a one-hour dump interval so no snapshot happens during a run. The headline workloads are GET, SET, and a 50/50 mix; the full report also exercises every implemented command family and measures RSS after string, list, and hash preloads.

Each run writes `results.csv`, `memory.csv`, and `summary.csv` beside a `temp/` directory containing raw logs and memtier output. The whole run directory stays untracked. RSS includes process and allocator overhead, so it is a comparative estimate rather than exact per-value memory use.

Headline workloads run for 30 seconds by default. Extended command-family workloads run for 5 seconds each; configure them with `DURATION_SECONDS` and `AUXILIARY_DURATION_SECONDS`.

## GitHub Actions

The **Benchmark summary** workflow can be started manually from the repository's Actions page. It runs the same script on a Linux GitHub-hosted runner, regenerates `benchmarks.md` and the marked benchmark-summary section in the root `README.md`, then commits those files only when they changed. GitHub-hosted runner results are useful for repeatability.

```bash
KEYS=100000 VALUE_BYTES=64 CLIENTS=50 PIPELINE=32 DURATION_SECONDS=60 bash benchmarks/run_local.sh
```
