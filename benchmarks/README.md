# Benchmarks

Requires Linux or WSL, Bash, jq, redis-cli, memtier_benchmark, Redis, Valkey, GNU time, and iproute2. Memcached is optional for GET, SET, and string memory comparisons.

## Build and run

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
bash benchmarks/run_local.sh
```

The runner first checks all 17 supported commands on fresh Reddish, Redis, and Valkey processes. It then compares identical workloads with persistence disabled. Defaults: 100,000 keys, 32-byte values, ten clients, pipeline 16, five trials, 30 seconds per GET/SET/mixed sample, five seconds per other workload, and three memory trials per shape.

Results go to `local/benchmarks/`. Raw samples and summaries are validated before [benchmarks.md](benchmarks.md) and the root README are updated. Both files show the measured revision, environment, date, and source run. These are measurements for that setup, not a general speed ranking.

For a shorter run:

```sh
TRIALS=3 DURATION_SECONDS=5 AUXILIARY_DURATION_SECONDS=2 INCLUDE_MEMCACHED=1 bash benchmarks/run_local.sh
```

Set `REDDISH_BINARY` for another build from the measured checkout. Dirty runs are labeled and retain source/script/binary hashes. `SERVER_CPU` and `CLIENT_CPUS` bind the server and client to separate physical cores. `CLIENTS` is per client thread; total connections are `CLIENTS * THREADS`. `KEY_PATTERN=S:S` traverses keys sequentially; `R:R` chooses random keys. `REQUESTS_PER_CLIENT` selects fixed work instead of timed headline samples.

Raw data includes throughput, latency, server/client CPU, scheduler counters, memory, source hashes, tool versions, logs, and failed requests. Client CPU includes setup and teardown. Main-thread blocked time is estimated from timed duration, runtime, and run-queue counters; it is not a stack trace. Missing scheduler data is reported as NA.

## Validation, capacity, and pressure

```sh
bash benchmarks/validate.sh
bash benchmarks/capacity.sh
bash benchmarks/pressure.sh
bash tests/benchmark_tests.sh
```

Command validation covers supported forms, binary/empty values, missing keys, errors, overflow, ordering, mutations, and expiry. It complements the C++ and integration suites; it does not prove every possible input correct.

The current capacity sweep covers GET, SET, and mixed string traffic. It varies total connections, client threads, pipeline depth, values, key counts, and sequential/random access. Defaults use three trials and a 2 ms limit on every trial's p99. The report selects the best observed point separately for each workload, key count, value size, and pattern. Client CPU is recorded to help identify a load-generator limit. This is a bounded closed-loop sweep, not proof of an absolute maximum.

Pressure tests check expiry cleanup, LRU eviction, large lists, promoted hashes, allocation churn, slow readers, and durable recovery after a crash and torn WAL tail. These are Reddish correctness scenarios, not comparative throughput tests or a long soak test.

## Regenerate a report

```sh
RESULTS_DIR=local/benchmarks/<run> bash benchmarks/publish_results.sh
```

`OUTPUT_FILE` changes the report destination. `README_FILE` changes the README destination; an empty string skips its update. `VALIDATION_DIR`, `CAPACITY_DIR`, `PRESSURE_DIR`, `WAL_RESULTS_DIR`, `DRIVER_DIR`, and `SOAK_DIR` add completed suites. Suites must reference the same measured revision and host. Missing measurements are labeled as missing, never shown as zero results.

## Tools and profiles

`install_tools.sh <new-directory>` builds pinned memtier, Redis, redis-cli, and Valkey. It needs make, a C/C++ compiler, autoconf, automake, libtool, pkg-config, and development packages for libevent, PCRE2, OpenSSL, zlib, and zstd.

```sh
bash benchmarks/install_tools.sh local/benchmark-tools
export PATH="$PWD/local/benchmark-tools/bin:$PATH"
bash benchmarks/profile.sh
bash benchmarks/profile_network.sh build/profile/output/reddish local/benchmarks/tcp-profile
bash benchmarks/network_profile.sh build/output/reddish local/benchmarks/wal
bash benchmarks/driver.sh
bash benchmarks/profile_native.sh
bash benchmarks/soak.sh
```

TCP profiling requires Valgrind. Callgrind reports user instructions, not elapsed time or kernel waits. Separate WAL measurements compare Reddish's sync modes; they do not compare durability performance against Redis or Valkey.

`driver.sh` keeps 16 total connections while comparing one, two, or four threads and four independent client processes. Its throughput uses all operations divided by the envelope of client start/finish times. Worst process p99 is not a pooled percentile. Available CPU slots and SMT topology still limit the driver.

`profile_native.sh` records CPU counters and cycle samples for 32-, 256-, and 4096-byte GETs when perf is available. Strace records socket and wait calls; root may be needed for attachment. Missing hardware counters are recorded as unavailable. Tracing changes timing, and maintenance-thread waits are not main-thread lock contention.

`soak.sh` defaults to 1800 seconds of mixed traffic, eviction and expiry. It checks the 10000-key limit and an 8 MiB database allocation budget while recording RSS. Misses are expected because its key space exceeds its key limit; request errors are rejected. `SOAK_SECONDS` can shorten a local smoke check. Do not describe that shortened run as a thirty-minute soak.

## GitHub Actions

The [workflow](https://github.com/amitverma-cf/reddish/actions/workflows/benchmark.yml) runs manually for the full current main SHA. It validates commands, selects separate physical cores, compares throughput/memory, sweeps capacity, tests pressure/recovery, and profiles the server. Raw artifacts are retained for 90 days.

The full run also checks independent client processes, captures native CPU/socket diagnostics, and runs the thirty-minute soak. Ordinary CI builds the Clang RESP fuzzer and exercises it for two minutes. Linux process tests inject write, flush, sync, rename, and compaction-crash failures.

After every measurement succeeds, a separate publishing job rebuilds the Markdown from the artifact and checks that it matches the generated files. It commits only README.md and benchmarks/benchmarks.md together. If main changed during the run, publishing fails and preserves the artifact instead of overwriting newer work. The benchmark has no push trigger, so the report commit does not start another benchmark. Reports identify the measured source commit, not the later report commit.
