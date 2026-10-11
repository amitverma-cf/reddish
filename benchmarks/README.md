# Benchmarks

Requires Linux or WSL, Bash, jq, redis-cli, memtier_benchmark, Redis, and Valkey. Install Memcached to include its GET, SET, and string-memory results.

## Run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash benchmarks/run_local.sh
```

This updates [the full report](benchmarks.md) and the benchmark tables in the root README. Raw samples and logs stay in `local/benchmarks/`. Existing runs are never overwritten.

The default run uses 100,000 keys, 32-byte values, ten clients, pipeline 16, and five trials. GET, SET, and mixed samples last 30 seconds. Other commands run for five seconds. Memory measurements use three fresh processes per data shape. Persistence is disabled for every server in the comparison.

For a shorter run:

```sh
TRIALS=3 DURATION_SECONDS=5 AUXILIARY_DURATION_SECONDS=2 INCLUDE_MEMCACHED=1 bash benchmarks/run_local.sh
```

Set `REDDISH_BINARY` for another build. Use `SERVER_CPU` and `CLIENT_CPUS` to bind the server and client to separate physical cores. `REQUESTS_PER_CLIENT` selects a fixed command count instead of timed GET/SET/mixed samples. `RUN_EXTENDED=0` skips the other commands.

To regenerate Markdown from an existing run:

```sh
RESULTS_DIR=local/benchmarks/<run> bash benchmarks/publish_results.sh
```

`OUTPUT_FILE` changes the report destination. `README_FILE` changes the README destination; set it to an empty string to skip the README update.

## Tools and profiles

`install_tools.sh <new-directory>` builds pinned memtier, Redis, redis-cli, and Valkey binaries. It needs make, a C/C++ compiler, autoconf, automake, libtool, pkg-config, and development packages for libevent, PCRE2, OpenSSL, zlib, and zstd. Add `<new-directory>/bin` to `PATH`.

```sh
bash benchmarks/profile.sh
bash benchmarks/profile_network.sh build/profile/output/reddish local/benchmarks/tcp-profile
bash benchmarks/network_profile.sh build/output/reddish local/benchmarks/wal
```

TCP profiling requires Valgrind. WAL measurements compare disabled, every-second sync, and sync-on-every-write modes separately from the server comparison.

The [GitHub workflow](https://github.com/amitverma-cf/reddish/actions/workflows/benchmark.yml) runs manually for the full current main SHA. It uploads raw data, profiles, README tables, and the full report. It does not run on pushes or create commits.
