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
- Periodic snapshots and a final snapshot during a clean shutdown
- Optional loading from a snapshot at startup

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

The server executable is written to `build/output/Release/reddish.exe`.

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
reddish [port] [--max-keys <count>] [--dump-interval <seconds>] \
        [--dump-path <path>] [--load-dump <path>]
```

Examples:

```bash
# Listen on port 6380 and allow at most 50,000 keys.
./build/output/reddish 6380 --max-keys 50000

# Load a previous snapshot if it exists. A missing file prints a warning and starts empty.
./build/output/reddish --load-dump ./data/reddish.snapshot
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

Snapshots are written every five minutes by default and once when the server receives a clean stop signal. Pass `--dump-interval` or `--dump-path` to change that behavior. Reddish writes a temporary snapshot before replacing the old one.

Expiry cleanup uses a min-heap, so normal command processing does not scan every key to find expired entries. When the key limit is reached, the least recently used key is evicted.

## C++ client

The client library lives in `client/C++`. It is built separately:

```bash
cmake -S client/C++ -B build/client
cmake --build build/client
```

Include `client.hpp`, link `reddish_client`, create `reddish::client::Client`, then call `connect()` before sending commands. The client exposes methods for every server command and a generic `execute()` method for RESP command vectors.

## Benchmarks

The benchmark runner compares Reddish, Redis, and Valkey with `memtier_benchmark` on Linux or WSL. It is a loopback measurement of the supported `GET` and `SET` workloads, not a general comparison of the three servers.

```bash
bash benchmarks/run_local.sh
```

See [benchmarks/README.md](benchmarks/README.md) for setup and [benchmarks/benchmarks.md](benchmarks/benchmarks.md) for published local results.

<!-- benchmark-summary:start -->
### Latest benchmark summary

Generated from [the full benchmark report](benchmarks/benchmarks.md). This is a loopback GET/SET measurement, not a general Redis or Valkey comparison.

| Workload | Reddish ops/s | Redis ops/s | Valkey ops/s |
|---|---:|---:|---:|
| Read (GET) | 1.0723m | 1.0634m | 1.0726m |
| Write (SET) | 0.9829m | 0.9810m | 0.9777m |
| Mixed (50/50) | 1.0176m | 1.0153m | 1.0161m |

<!-- benchmark-summary:end -->

## API documentation

Doxygen output is committed in [`docs/`](docs/index.html). To regenerate it on Linux:

```bash
cmake -S . -B build-docs -DREDDISH_BUILD_DOCS=ON
cmake --build build-docs --target docs
```

## Scope

Reddish deliberately does not implement Redis replication, clustering, ACLs, TLS, transactions, Lua scripting, modules, Pub/Sub, streams, or Redis's full command and data-type surface. It also has one server thread, so a slow snapshot or command blocks other clients until it finishes.
