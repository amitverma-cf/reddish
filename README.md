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

Expiry cleanup uses a min-heap, so normal command processing does not scan every key to find expired entries. When the key limit is reached, the least recently used key is evicted. The default limit is 1,024 keys; this is a key count limit, not a byte limit.

## C++ client

The client library lives in `client/C++`. It is built separately:

```bash
cmake -S client/C++ -B build/client
cmake --build build/client
```

Include `client.hpp`, link `reddish_client`, create `reddish::client::Client`, then call `connect()` before sending commands. The client exposes methods for every server command and a generic `execute()` method for RESP command vectors. Calls return `std::expected` results containing raw RESP replies or an error. Replies are limited to 16 MiB; use one client per calling thread.

## Benchmarks

The benchmark runner compares Reddish, Redis, and Valkey with `memtier_benchmark` on Linux. Memcached is included as a string-cache baseline for `GET`, `SET`, and string-memory measurement only.

```bash
bash benchmarks/run_local.sh
```

See [benchmarks/README.md](benchmarks/README.md) for setup and [benchmarks/benchmarks.md](benchmarks/benchmarks.md) for published local results.

<!-- benchmark-summary:start -->
### Benchmark results

Local loopback results; persistence disabled. [Full report](benchmarks/benchmarks.md) includes trial ranges, latency, CPU, memory, and setup.

| Workload | reddish ops/s | redis ops/s | valkey ops/s | memcached ops/s |
|---|---:|---:|---:|---:|
| reads | 409028 | 382192 | 390619 | 448644 |
| writes | 368414 | 345827 | 359454 | 510926 |
| mixed | 370083 | 390756 | 363498 | — |

| Shape | Server | RSS MiB | Delta bytes/key |
|---|---|---:|---:|
| hashes | reddish | 13.92 | 109.27 |
| hashes | redis | 23.12 | 105.73 |
| hashes | valkey | 24.33 | 111.85 |
| lists | reddish | 14.63 | 116.76 |
| lists | redis | 23.12 | 106.17 |
| lists | valkey | 24.33 | 111.85 |
| strings | memcached | 17.83 | 131.07 |
| strings | reddish | 12.33 | 92.60 |
| strings | redis | 21.54 | 90.00 |
| strings | valkey | 22.71 | 95.68 |

<!-- benchmark-summary:end -->

## Scope

Reddish deliberately does not implement Redis replication, clustering, ACLs, TLS, transactions, Lua scripting, modules, Pub/Sub, streams, or Redis's full command and data-type surface. Commands run on one event-loop thread; maintenance and WAL work use background threads. Slow commands and synchronous WAL writes can delay other clients.
