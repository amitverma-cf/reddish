# Linux benchmarks

Build a Release server first, then install `memtier_benchmark`, Redis, and Valkey. Memcached is optional locally. Native Windows is unsupported; WSL works, but host scheduling can produce large trial variance.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash benchmarks/run_local.sh
```

`install_tools.sh <directory>` builds pinned memtier 2.5.1, Redis 8.10.1, and Valkey 8.1.1 revisions into that directory. It requires Git, a C/C++ toolchain, make, autoconf, automake, libtool, pkg-config, and development packages for libevent, PCRE2, zlib, OpenSSL, and zstd. Add its `bin` directory to `PATH`. Source directories must not already exist. The installer records revisions and versions in `manifest.txt`.

The comparison disables persistence on all servers. Every server gets the same preload, keyspace, values, client count, pipeline, and warmup. Defaults are 100,000 keys, 32-byte values, one worker, ten clients, pipeline 16, two seconds warmup, and five trials. GET, SET, and mixed trials last 30 seconds; extended command workloads last five seconds. Servers run sequentially, so temperature and host load can affect comparisons.

```bash
RESULT_DIR=local/benchmarks/example REDDISH_BINARY=build/output/reddish \
TRIALS=3 DURATION_SECONDS=10 INCLUDE_MEMCACHED=1 bash benchmarks/run_local.sh
RESULTS_DIR=local/benchmarks/example bash benchmarks/publish_results.sh
```

Set `REQUESTS_PER_CLIENT` to a positive count for fixed-work headline samples instead of timed samples. `RUN_EXTENDED=0` skips extended commands; `RUN_PERFORMANCE=0` or `RUN_MEMORY=0` selects one measurement class. `MEMORY_TRIALS` defaults to three. `SERVER_CPU` and `CLIENT_CPUS` optionally bind server and load-generator CPUs; choose distinct physical cores. `KEYS`, `VALUE_BYTES`, `THREADS`, `CLIENTS`, `PIPELINE`, and `WARMUP_SECONDS` configure the workload. `REQUIRE_MEMCACHED=1` fails if the requested baseline is missing.

Memcached participates only in GET, SET, and string memory measurements. String, list, and hash memory samples report total process RSS and the increase over idle RSS per key. RSS includes allocator and process overhead; it is not an exact allocation measurement.

Each run retains `results.csv`, `summary.csv`, `cpu.csv`, `memory.csv`, source and binary hashes, a measurement manifest, and raw load-generator logs. Defaults go under ignored `local/benchmarks/`. Existing results are not overwritten. `report.py` reads one completed run; it never combines unrelated runs. `publish_results.sh` writes that run's `report.md`; only an explicit `README_FILE` updates a marked README section.

## Profiling

```bash
bash benchmarks/profile.sh
python3 benchmarks/profile_network.py --help
python3 benchmarks/network_profile.py --help
```

The microbenchmark builds with symbols and records command costs and Callgrind attribution when Valgrind is installed. The network profiler measures real TCP traffic; the WAL comparison measures persistence modes separately. These diagnostics have different workloads and must not be substituted for comparison throughput.

## GitHub Actions

The **Benchmark artifacts** workflow is manual-only on `main`. Supply the complete current main commit SHA as `commit`; a different revision is rejected. It builds pinned comparison tools, runs tests, compares throughput and memory, profiles the server, and uploads an artifact named for that SHA. The report also appears in the job summary. It never commits generated results and has no push, pull-request, or schedule trigger.

Artifacts and execution status are available on the [workflow page](https://github.com/amitverma-cf/reddish/actions/workflows/benchmark.yml). Hosted-runner numbers describe that runner and workload, not general production performance.
