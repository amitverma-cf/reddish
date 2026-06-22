# Linux benchmarks

Run on Linux or WSL2 after installing `memtier_benchmark`, Redis, and Valkey. Native Windows is unsupported.

The runner uses the same keyspace, value size, client count, pipeline depth, warm-up preload, duration, and five trials for reddish, Redis, and Valkey. It disables Redis/Valkey persistence so the comparison measures command-path performance; reddish uses a one-hour dump interval so no snapshot happens during a run.

Each run writes reviewed aggregates (`results.csv`, `summary.csv`) beside a `temp/` directory containing raw logs and memtier output. The entire run directory stays untracked; copy only reviewed aggregate results into `benchmarks.md` for publication.

## GitHub Actions

The **Benchmark summary** workflow can be started manually from the repository's Actions page. It runs the same script on a Linux GitHub-hosted runner, regenerates `benchmarks.md` and the marked benchmark-summary section in the root `README.md`, then commits those files only when they changed. GitHub-hosted runner results are useful for repeatability, but they must not be combined with local samples because the hardware differs.

```bash
KEYS=100000 VALUE_BYTES=64 CLIENTS=50 PIPELINE=32 DURATION_SECONDS=60 bash benchmarks/run_local.sh
```
