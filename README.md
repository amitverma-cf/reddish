# Reddish

Reddish is a small Redis-like inmemory database written in C++23. It speaks RESP over TCP, keeps its data in memory, and includes a C++ client library.

The project is a learning implementation. It is useful for exploring a single-threaded event loop, RESP parsing, in-memory data structures, TTLs, LRU eviction, persistence, and benchmarking. It is not a drop-in replacement for Redis or Valkey.

## What it supports

- RESP commands over TCP on port `6379` by default
- Multiple clients through one event loop
- Strings and integers: `PING`, `SET`, `GET`, `INCR`, `DEL`
- Lists: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN`
- Hashes: `HSET`, `HGET`, `HDEL`, `HLEN`
- Expiry: `EXPIRE`, `TTL`
- LRU eviction when `--max-keys` is reached
- Server and memory statistics: `INFO`, `INFO memory`
- Write-ahead logging with recovery at startup and configurable sync policy

## Build

Requirements:

- CMake 3.20 or newer
- A C++23 compiler
- Git and an internet connection on the first test build, because Catch2 is fetched by CMake

### Windows

Open a Visual Studio Developer PowerShell in the repository root:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The server executable is written to `build/output/reddish.exe`.

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The server executable is `build/output/reddish`.

## Run

```bash
./build/output/reddish
```

The first positional argument changes the port. Configuration flags are optional:

```text
reddish [port] [--max-keys <count>] [--wal-path <path>] \
        [--wal-sync everysec|always|none] [--no-wal]
```

Examples:

```bash
./build/output/reddish 6380 --max-keys 50000

./build/output/reddish --wal-path ./data/reddish.wal --wal-sync always
```

Any RESP client can connect. For a quick manual check, `redis-cli` works for the commands Reddish supports:

```bash
redis-cli -p 6379 SET greeting hello
redis-cli -p 6379 GET greeting
redis-cli -p 6379 LPUSH jobs build test
redis-cli -p 6379 INFO memory
```

## Command reference

| Group | Commands |
|---|---|
| Connection | `PING` |
| Strings | `SET`, `GET`, `INCR`, `DEL` |
| Lists | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LLEN` |
| Hashes | `HSET`, `HGET`, `HDEL`, `HLEN` |
| Expiry | `EXPIRE`, `TTL` |
| Introspection | `INFO`, `INFO memory` |

Command names are case-insensitive. Values supplied through RESP are strings. `INCR` converts a string value at a top-level key into an integer.

## Persistence and eviction

The default WAL is `reddish.wal`, with `everysec` syncing. Recovery replays the log at startup, retaining absolute expiry times. `always` syncs each write; `none` leaves syncing to the operating system. `--no-wal` disables persistence. Create the parent directory before choosing a custom WAL path. Snapshot flags are no longer supported. The WAL does not compact automatically.

To compact a WAL, stop its server and run:

```sh
reddish --wal-path reddish.wal --compact-wal
```

This rewrites live state, verifies the new log, replaces the old file, and exits. It preserves collection order, integer types, and expiry deadlines. The `.lock` sidecar protects the WAL across replacement; leave it in place. Compaction needs space for both generations and fails if another writer holds the lock. It is an offline operation.

Expiry cleanup uses a min-heap, so normal command processing does not scan every key to find expired entries. When the key limit is reached, the least recently used key is evicted. The default limit is 1,024 keys; this is a key count limit, not a byte limit.

`--max-memory <bytes>` adds a database allocation limit; omitting it leaves allocation unlimited. It accounts for slab reservations, direct values, buckets, expiry metadata, promoted hashes, and temporary database allocations. Empty slabs are reclaimed when needed. If the allocation still cannot fit, the write returns `OOM`; this limit does not evict live keys. Rejected writes preserve the target value and TTL. Both limits apply independently.

`INFO memory` exposes `owned_memory_bytes` and `max_memory_bytes`. The allocation budget excludes the database object's fixed size, client/request/reply buffers, WAL/recovery buffers, and allocator/process overhead; it is not an RSS limit. Recovery must fit the configured budget, including intermediate replay states. Compact without a byte limit first if discarded history prevents replay under a tighter limit.

## C++ client

The client library lives in `client/C++`. It is built separately:

```bash
cmake -S client/C++ -B build/client
cmake --build build/client
```

Include `client.hpp`, link `reddish_client`, create `reddish::client::Client`, then call `connect()` before sending commands. The client exposes methods for every server command and a generic `execute()` method for RESP command vectors. Calls return `std::expected` results containing raw RESP replies or an error. Replies are limited to 16 MiB; use one client per calling thread.

## Benchmarks

The benchmark runner compares Reddish, Redis, and Valkey with `memtier_benchmark` on Linux. Memcached can be included as a string-cache baseline for `GET`, `SET`, and string-memory measurement only.

```bash
bash benchmarks/run_local.sh
```

See [benchmarks/README.md](benchmarks/README.md) for setup and [benchmarks/benchmarks.md](benchmarks/benchmarks.md) for the recorded results and measurement settings.

<!-- benchmark-summary:start -->
### Benchmark results

GitHub Actions / Ubuntu 24.04, measured 2026-10-31 20:26:40 UTC; source revision [c7e0ec2](https://github.com/amitverma-cf/reddish/commit/c7e0ec2c428ad563635ca8c81803f4598aa3e98a). Persistence disabled. [Full report](benchmarks/benchmarks.md) contains the same dataset, trial ranges, latency, CPU, memory, and setup.

Baseline: 100000 keys, 32-byte values; client threads: 1, total connections: 10, pipeline: 16; 5 trials of 30 seconds.

These rows use the fixed baseline configuration. The full report lists capacity sweeps separately; changing client threads or pipeline depth changes the result.

[Source workflow run](https://github.com/amitverma-cf/reddish/actions/runs/41238487500).

| Workload | reddish ops/s | redis ops/s | valkey ops/s | memcached ops/s |
|---|---:|---:|---:|---:|
| reads | 787971 | 833592 | 842762 | 965655 |
| writes | 720373 | 772334 | 778251 | 940754 |
| mixed | 748157 | 804526 | 804030 | — |

| Shape | Server | RSS MiB | Delta bytes/key |
|---|---|---:|---:|
| hashes | reddish | 14.34 | 108.91 |
| hashes | redis | 18.82 | 109.16 |
| hashes | valkey | 17.04 | 104.00 |
| lists | reddish | 15.02 | 116.12 |
| lists | redis | 18.92 | 109.81 |
| lists | valkey | 17.05 | 103.25 |
| strings | memcached | 17.62 | 137.09 |
| strings | reddish | 12.69 | 91.88 |
| strings | redis | 17.49 | 94.33 |
| strings | valkey | 15.56 | 87.50 |

<!-- benchmark-summary:end -->

## Scope

Reddish deliberately does not implement Redis replication, clustering, ACLs, TLS, transactions, Lua scripting, modules, Pub/Sub, streams, or Redis's full command and data-type surface. Commands run on one event-loop thread; maintenance and WAL work use background threads. Slow commands and synchronous WAL writes can delay other clients.
