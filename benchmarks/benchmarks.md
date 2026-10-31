# Benchmark results

Measured revision: [c7e0ec2](https://github.com/amitverma-cf/reddish/commit/c7e0ec2c428ad563635ca8c81803f4598aa3e98a).

Environment: **GitHub Actions / Ubuntu 24.04**. Measured: **2026-10-31 20:26:40 UTC**.

[Source workflow run](https://github.com/amitverma-cf/reddish/actions/runs/41238487500). Raw data and profiles are attached to that run.

These are results for this machine and workload. They do not establish a general ranking or maximum server capacity.

Loopback TCP on the machine below. Persistence is disabled for the server comparison. Each server receives the same preload, warmup, values, clients, and pipeline. Server order rotates between trials; GET and SET order alternates.

## Setup

```text
Measured at 2026-10-31 20:26:40 UTC
Linux runnervm48cm3 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
CPU(s):                                  4
Model name:                              AMD EPYC 7763 64-Core Processor
memtier_benchmark v=2.5.1 sha=5f634d17:0 bits=64 libevent=2.1.12-stable openssl=OpenSSL 3.0.13 30 Jan 2024 prometheus=yes
Redis server v=8.10.1 sha=3399357e:0 malloc=jemalloc-5.3.0 bits=64 build=655bce6717d2446
Valkey server v=8.1.1 sha=fcd8bc3e:0 malloc=jemalloc-5.3.0 bits=64 build=d189c1deef07e8aa
memcached 1.6.24
SERVER_CPU=0 CLIENT_CPUS=2,3
REQUESTS_PER_CLIENT=0 KEY_PATTERN=S:S
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=5 DURATION_SECONDS=30 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=5 persistence=disabled
```

## Throughput

Operations per second. Range and standard deviation describe the trial means, not individual requests.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 5 | 748157 | 733952 | 762222 | 11580 |
| mixed | redis | 5 | 804526 | 797556 | 811370 | 5560 |
| mixed | valkey | 5 | 804030 | 800792 | 811786 | 4438 |
| reads | memcached | 5 | 965655 | 958325 | 970911 | 4666 |
| reads | reddish | 5 | 787971 | 775000 | 793792 | 7584 |
| reads | redis | 5 | 833592 | 819773 | 845265 | 9563 |
| reads | valkey | 5 | 842762 | 836022 | 851952 | 5757 |
| writes | memcached | 5 | 940754 | 934708 | 951459 | 6421 |
| writes | reddish | 5 | 720373 | 714130 | 723845 | 4017 |
| writes | redis | 5 | 772334 | 768044 | 777616 | 3569 |
| writes | valkey | 5 | 778251 | 775126 | 780153 | 1943 |

## Other commands

Each paired workload reports individual commands per second.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 5 | 754315 | 720950 | 779145 | 21012 |
| expire_ttl | redis | 5 | 794689 | 791524 | 798865 | 2646 |
| expire_ttl | valkey | 5 | 778359 | 774953 | 782682 | 2855 |
| hget | reddish | 5 | 709969 | 704875 | 717489 | 4671 |
| hget | redis | 5 | 747805 | 737168 | 768478 | 12598 |
| hget | valkey | 5 | 747840 | 741009 | 753965 | 5039 |
| hlen | reddish | 5 | 785212 | 779486 | 790931 | 5304 |
| hlen | redis | 5 | 824595 | 818383 | 831206 | 4672 |
| hlen | valkey | 5 | 822864 | 811860 | 832662 | 7538 |
| hset_hdel | reddish | 5 | 705813 | 703120 | 708608 | 2047 |
| hset_hdel | redis | 5 | 734698 | 730913 | 739743 | 3575 |
| hset_hdel | valkey | 5 | 723990 | 716430 | 732032 | 6031 |
| incr | reddish | 5 | 784553 | 768071 | 794172 | 10439 |
| incr | redis | 5 | 826858 | 823658 | 831043 | 2844 |
| incr | valkey | 5 | 827261 | 824483 | 830314 | 2111 |
| llen | reddish | 5 | 789965 | 780413 | 808637 | 11040 |
| llen | redis | 5 | 826696 | 817186 | 839771 | 8807 |
| llen | valkey | 5 | 820622 | 810388 | 827723 | 7955 |
| lpush_lpop | reddish | 5 | 699504 | 692086 | 709750 | 6560 |
| lpush_lpop | redis | 5 | 752744 | 746817 | 758768 | 5281 |
| lpush_lpop | valkey | 5 | 755380 | 751328 | 759347 | 3276 |
| ping | reddish | 5 | 1035481 | 1029124 | 1039390 | 4203 |
| ping | redis | 5 | 1079852 | 1075178 | 1084313 | 4095 |
| ping | valkey | 5 | 1095293 | 1086351 | 1100056 | 5327 |
| rpush_rpop | reddish | 5 | 691006 | 680284 | 698790 | 7664 |
| rpush_rpop | redis | 5 | 743873 | 719883 | 751061 | 13454 |
| rpush_rpop | valkey | 5 | 752787 | 749998 | 759333 | 3778 |
| set_del | reddish | 5 | 730731 | 726774 | 734779 | 3119 |
| set_del | redis | 5 | 777324 | 775036 | 782297 | 2859 |
| set_del | valkey | 5 | 777055 | 774177 | 780938 | 2592 |

## Latency and CPU

Latency is in milliseconds. Percentiles are averages of each trial percentile; they are not pooled percentiles. CPU is server user plus system time per operation, excluding the load generator.

| Workload | Server | Mean latency ms | p50 ms | p99 ms | p99.9 ms | CPU µs/op |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 0.210 | 0.199 | 0.317 | 0.412 | 0.943 |
| expire_ttl | redis | 0.199 | 0.199 | 0.300 | 0.492 | 1.250 |
| expire_ttl | valkey | 0.203 | 0.199 | 0.484 | 0.657 | 1.281 |
| hget | reddish | 0.221 | 0.213 | 0.337 | 0.415 | 0.907 |
| hget | redis | 0.210 | 0.199 | 0.314 | 0.401 | 1.195 |
| hget | valkey | 0.210 | 0.199 | 0.317 | 0.453 | 1.206 |
| hlen | reddish | 0.201 | 0.191 | 0.308 | 0.437 | 0.860 |
| hlen | redis | 0.192 | 0.175 | 0.295 | 0.399 | 1.087 |
| hlen | valkey | 0.192 | 0.177 | 0.297 | 0.445 | 1.098 |
| hset_hdel | reddish | 0.224 | 0.215 | 0.341 | 0.442 | 0.895 |
| hset_hdel | redis | 0.215 | 0.215 | 0.335 | 0.521 | 1.358 |
| hset_hdel | valkey | 0.218 | 0.220 | 0.330 | 0.500 | 1.379 |
| incr | reddish | 0.201 | 0.193 | 0.306 | 0.434 | 0.824 |
| incr | redis | 0.191 | 0.178 | 0.287 | 0.457 | 1.101 |
| incr | valkey | 0.191 | 0.183 | 0.285 | 0.450 | 1.118 |
| llen | reddish | 0.200 | 0.189 | 0.306 | 0.482 | 0.845 |
| llen | redis | 0.191 | 0.175 | 0.292 | 0.420 | 1.074 |
| llen | valkey | 0.193 | 0.177 | 0.297 | 0.425 | 1.090 |
| lpush_lpop | reddish | 0.225 | 0.217 | 0.343 | 0.425 | 0.933 |
| lpush_lpop | redis | 0.209 | 0.210 | 0.308 | 0.439 | 1.316 |
| lpush_lpop | valkey | 0.208 | 0.207 | 0.309 | 0.469 | 1.320 |
| mixed | reddish | 0.211 | 0.204 | 0.322 | 0.447 | 0.811 |
| mixed | redis | 0.196 | 0.204 | 0.290 | 0.455 | 1.194 |
| mixed | valkey | 0.196 | 0.205 | 0.292 | 0.474 | 1.196 |
| ping | reddish | 0.152 | 0.143 | 0.236 | 0.388 | 0.650 |
| ping | redis | 0.145 | 0.135 | 0.221 | 0.913 | 0.858 |
| ping | valkey | 0.143 | 0.137 | 0.217 | 0.324 | 0.874 |
| reads | memcached | 0.163 | 0.159 | 0.242 | 0.749 | 0.970 |
| reads | reddish | 0.200 | 0.191 | 0.305 | 0.444 | 0.809 |
| reads | redis | 0.189 | 0.175 | 0.289 | 0.434 | 1.088 |
| reads | valkey | 0.187 | 0.181 | 0.282 | 0.426 | 1.107 |
| rpush_rpop | reddish | 0.228 | 0.218 | 0.346 | 0.461 | 0.936 |
| rpush_rpop | redis | 0.212 | 0.217 | 0.316 | 0.492 | 1.325 |
| rpush_rpop | valkey | 0.209 | 0.207 | 0.319 | 0.506 | 1.327 |
| set_del | reddish | 0.216 | 0.207 | 0.337 | 0.506 | 0.854 |
| set_del | redis | 0.203 | 0.207 | 0.300 | 0.420 | 1.200 |
| set_del | valkey | 0.203 | 0.210 | 0.298 | 0.455 | 1.220 |
| writes | memcached | 0.168 | 0.159 | 0.252 | 0.473 | 0.981 |
| writes | reddish | 0.219 | 0.210 | 0.338 | 0.548 | 0.844 |
| writes | redis | 0.205 | 0.215 | 0.298 | 0.479 | 1.252 |
| writes | valkey | 0.203 | 0.207 | 0.298 | 0.479 | 1.263 |

## Memory

Fresh server processes are measured before and after each preload. Each key holds one string, one list element, or one hash field. RSS includes process and allocator overhead. Delta bytes/key is (loaded RSS − idle RSS) × 1024 / verified key count. It is not the exact allocation size of a value.

| Shape | Server | Trials | Keys | Idle RSS MiB | Loaded RSS MiB | Delta bytes/key | Min bytes/key | Max bytes/key |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| hashes | reddish | 3 | 100000 | 3.95 | 14.34 | 108.91 | 108.91 | 108.91 |
| hashes | redis | 3 | 100000 | 8.41 | 18.82 | 109.16 | 108.58 | 109.69 |
| hashes | valkey | 3 | 100000 | 7.13 | 17.04 | 104.00 | 103.75 | 104.41 |
| lists | reddish | 3 | 100000 | 3.95 | 15.02 | 116.12 | 116.12 | 116.12 |
| lists | redis | 3 | 100000 | 8.45 | 18.92 | 109.81 | 109.04 | 110.59 |
| lists | valkey | 3 | 100000 | 7.20 | 17.05 | 103.25 | 103.06 | 103.59 |
| strings | memcached | 3 | 100000 | 4.54 | 17.62 | 137.09 | 137.09 | 137.09 |
| strings | reddish | 3 | 100000 | 3.93 | 12.69 | 91.88 | 91.87 | 91.91 |
| strings | redis | 3 | 100000 | 8.49 | 17.49 | 94.33 | 94.29 | 94.41 |
| strings | valkey | 3 | 100000 | 7.21 | 15.56 | 87.50 | 87.49 | 87.53 |

## WAL modes

Reddish only. These samples include WAL cost and use a separate workload; they are not part of the persistence-disabled comparison.

These samples use 32-byte values and a 50/50 SET/GET mix without CPU binding.

```text
KEYS=1000 CLIENTS=10 PIPELINE=16 DURATION_SECONDS=5 TRIALS=3 WARMUP_SECONDS=2
```

| Mode | Trials | Mean ops/s | Min | Max | Mean p99 ms | CPU µs/op |
|---|---:|---:|---:|---:|---:|---:|
| always | 3 | 62714 | 62132 | 63741 | 6.207 | 3.964 |
| everysec | 3 | 722765 | 713284 | 733954 | 0.402 | 0.898 |
| off | 3 | 742830 | 731080 | 754370 | 0.346 | 0.766 |

## Validation

| Server | Checks passed | Commands covered |
|---|---:|---:|
| reddish | 82 | 17 |
| redis | 82 | 17 |
| valkey | 82 | 17 |

The preflight checks supported command forms, payloads, list ordering, hash mutations, errors, overflow, missing keys and real TTL expiry. This is command coverage, not a proof of every input or execution path.

## Client CPU and server scheduling

CPU is normalized by the timed workload duration. Client process CPU includes startup and teardown. Main-thread runtime and queue counters bracket the client call; blocked time is an estimate after subtracting runtime and queue time from workload duration. It does not identify the wait stack. NA means scheduler counters were unavailable. A Memcached main thread is its dispatcher.

| Workload | Server | Server CPU/core % | Client CPU % | Main run queue % | Estimated main blocked % | Context switches/s |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 60.66 | 99.97 | 0.23 | 39.16 | 27511 |
| mixed | redis | 96.09 | 99.80 | 0.04 | 3.87 | 3299 |
| mixed | valkey | 96.18 | 99.78 | 0.05 | 3.77 | 3252 |
| reads | memcached | 93.71 | 98.92 | 0.00 | 100.00 | 1 |
| reads | reddish | 63.76 | 99.94 | 0.25 | 36.03 | 25341 |
| reads | redis | 90.72 | 99.74 | 0.12 | 9.16 | 7795 |
| reads | valkey | 93.32 | 99.85 | 0.07 | 6.60 | 5840 |
| writes | memcached | 92.31 | 99.58 | 0.00 | 100.00 | 1 |
| writes | reddish | 60.82 | 99.92 | 0.26 | 38.97 | 25992 |
| writes | redis | 96.69 | 99.78 | 0.06 | 3.25 | 2606 |
| writes | valkey | 98.27 | 99.76 | 0.03 | 1.70 | 1292 |

## Capacity measurements

Best observed points are grouped by server, workload, key count, value size and access pattern. Only points whose every trial meets the configured p99 limit qualify. These closed-loop measurements are not proof of absolute maximum capacity or an open-loop latency guarantee. Client CPU near a worker or assigned-CPU budget can limit the result. A one-trial row is a smoke measurement; repeated rows are needed for capacity conclusions.

```text
SERVER_CPU=0 CLIENT_CPUS=2,3 P99_LIMIT_MS=2 TRIALS=3 DURATION_SECONDS=5
```

| Server | Workload | Keys | Bytes | Clients | Threads | Pipeline | Pattern | Trials | Mean ops/s | Worst trial p99 ms |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---:|
| reddish | mixed | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 950057 | 0.255 |
| reddish | mixed | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 961695 | 0.247 |
| reddish | mixed | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 898244 | 0.271 |
| reddish | mixed | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 304877 | 0.879 |
| reddish | mixed | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 961105 | 0.247 |
| reddish | reads | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1001552 | 0.239 |
| reddish | reads | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 1011488 | 0.239 |
| reddish | reads | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 584930 | 0.375 |
| reddish | reads | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 139654 | 1.503 |
| reddish | reads | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1019487 | 0.239 |
| reddish | writes | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 914120 | 0.263 |
| reddish | writes | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 920677 | 0.263 |
| reddish | writes | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 867170 | 0.279 |
| reddish | writes | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 470779 | 0.535 |
| reddish | writes | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 927041 | 0.255 |
| redis | mixed | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 864329 | 0.303 |
| redis | mixed | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 885579 | 0.279 |
| redis | mixed | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 866132 | 0.311 |
| redis | mixed | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 322397 | 0.647 |
| redis | mixed | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 870503 | 0.279 |
| redis | reads | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1033743 | 0.255 |
| redis | reads | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 1053782 | 0.239 |
| redis | reads | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 464413 | 0.615 |
| redis | reads | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 203978 | 0.967 |
| redis | reads | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1030500 | 0.263 |
| redis | writes | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 819406 | 0.295 |
| redis | writes | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 829242 | 0.287 |
| redis | writes | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 857998 | 0.295 |
| redis | writes | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 225977 | 0.871 |
| redis | writes | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 811613 | 0.303 |
| valkey | mixed | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 843095 | 0.295 |
| valkey | mixed | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 880728 | 0.303 |
| valkey | mixed | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 887104 | 0.287 |
| valkey | mixed | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 322332 | 0.655 |
| valkey | mixed | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 892220 | 0.279 |
| valkey | reads | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 993786 | 0.271 |
| valkey | reads | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 1012022 | 0.295 |
| valkey | reads | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 459928 | 0.527 |
| valkey | reads | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 198916 | 0.967 |
| valkey | reads | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1010176 | 0.255 |
| valkey | writes | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 783066 | 0.303 |
| valkey | writes | 100000 | 32 | 10 | 2 | 16 | S:S | 3 | 798041 | 0.407 |
| valkey | writes | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 816918 | 0.407 |
| valkey | writes | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 223445 | 1.207 |
| valkey | writes | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 804722 | 0.311 |

<details>
<summary>Every tested operating point</summary>

| Case | Server | Workload | Trials | Mean ops/s | Worst p99 ms | Server CPU % | Client CPU/worker % | Client CPU/budget % | Meets p99 limit |
|---|---|---|---:|---:|---:|---:|---:|---:|---|
| c1-t1-p1-v32-k100000-S | reddish | mixed | 3 | 24462 | 0.071 | 42.13 | 58.33 | 29.16 | true |
| c1-t1-p1-v32-k100000-S | reddish | reads | 3 | 24329 | 0.071 | 42.07 | 58.20 | 29.10 | true |
| c1-t1-p1-v32-k100000-S | reddish | writes | 3 | 24255 | 0.071 | 42.00 | 58.33 | 29.16 | true |
| c1-t1-p1-v32-k100000-S | redis | mixed | 3 | 21578 | 0.079 | 49.20 | 52.13 | 26.07 | true |
| c1-t1-p1-v32-k100000-S | redis | reads | 3 | 22101 | 0.079 | 48.13 | 53.00 | 26.50 | true |
| c1-t1-p1-v32-k100000-S | redis | writes | 3 | 21642 | 0.079 | 49.53 | 51.80 | 25.90 | true |
| c1-t1-p1-v32-k100000-S | valkey | mixed | 3 | 21816 | 0.079 | 48.07 | 52.93 | 26.46 | true |
| c1-t1-p1-v32-k100000-S | valkey | reads | 3 | 22235 | 0.079 | 47.93 | 53.33 | 26.66 | true |
| c1-t1-p1-v32-k100000-S | valkey | writes | 3 | 21813 | 0.079 | 48.67 | 52.47 | 26.23 | true |
| c10-t1-p1-v32-k100000-S | reddish | mixed | 3 | 81860 | 0.191 | 78.86 | 99.99 | 49.99 | true |
| c10-t1-p1-v32-k100000-S | reddish | reads | 3 | 82454 | 0.191 | 80.12 | 99.91 | 49.95 | true |
| c10-t1-p1-v32-k100000-S | reddish | writes | 3 | 81785 | 0.191 | 78.92 | 99.79 | 49.90 | true |
| c10-t1-p1-v32-k100000-S | redis | mixed | 3 | 84180 | 0.191 | 92.38 | 99.85 | 49.92 | true |
| c10-t1-p1-v32-k100000-S | redis | reads | 3 | 86442 | 0.183 | 91.78 | 99.85 | 49.92 | true |
| c10-t1-p1-v32-k100000-S | redis | writes | 3 | 84804 | 0.191 | 93.06 | 99.79 | 49.90 | true |
| c10-t1-p1-v32-k100000-S | valkey | mixed | 3 | 84311 | 0.183 | 91.32 | 99.91 | 49.95 | true |
| c10-t1-p1-v32-k100000-S | valkey | reads | 3 | 83929 | 0.215 | 90.18 | 99.78 | 49.89 | true |
| c10-t1-p1-v32-k100000-S | valkey | writes | 3 | 84014 | 0.183 | 91.71 | 99.91 | 49.95 | true |
| c10-t1-p16-v32-k100000-S | reddish | mixed | 3 | 748142 | 0.327 | 60.79 | 99.98 | 49.99 | true |
| c10-t1-p16-v32-k100000-S | reddish | reads | 3 | 790826 | 0.311 | 64.66 | 99.99 | 49.99 | true |
| c10-t1-p16-v32-k100000-S | reddish | writes | 3 | 727357 | 0.335 | 60.92 | 99.85 | 49.92 | true |
| c10-t1-p16-v32-k100000-S | redis | mixed | 3 | 807555 | 0.287 | 96.26 | 99.86 | 49.93 | true |
| c10-t1-p16-v32-k100000-S | redis | reads | 3 | 832620 | 0.295 | 90.92 | 99.71 | 49.85 | true |
| c10-t1-p16-v32-k100000-S | redis | writes | 3 | 789334 | 0.287 | 97.58 | 99.78 | 49.89 | true |
| c10-t1-p16-v32-k100000-S | valkey | mixed | 3 | 799381 | 0.303 | 95.65 | 99.79 | 49.90 | true |
| c10-t1-p16-v32-k100000-S | valkey | reads | 3 | 840400 | 0.287 | 91.59 | 99.85 | 49.92 | true |
| c10-t1-p16-v32-k100000-S | valkey | writes | 3 | 783638 | 0.295 | 98.85 | 99.71 | 49.85 | true |
| c10-t2-p16-v32-k100000-S | reddish | mixed | 3 | 961695 | 0.247 | 69.72 | 99.92 | 99.92 | true |
| c10-t2-p16-v32-k100000-S | reddish | reads | 3 | 1011488 | 0.239 | 73.20 | 99.89 | 99.89 | true |
| c10-t2-p16-v32-k100000-S | reddish | writes | 3 | 920677 | 0.263 | 69.13 | 99.79 | 99.80 | true |
| c10-t2-p16-v32-k100000-S | redis | mixed | 3 | 885579 | 0.279 | 99.99 | 93.43 | 93.42 | true |
| c10-t2-p16-v32-k100000-S | redis | reads | 3 | 1053782 | 0.239 | 99.72 | 99.05 | 99.06 | true |
| c10-t2-p16-v32-k100000-S | redis | writes | 3 | 829242 | 0.287 | 99.93 | 91.59 | 91.59 | true |
| c10-t2-p16-v32-k100000-S | valkey | mixed | 3 | 880728 | 0.303 | 100.07 | 92.60 | 92.60 | true |
| c10-t2-p16-v32-k100000-S | valkey | reads | 3 | 1012022 | 0.295 | 99.65 | 97.15 | 97.14 | true |
| c10-t2-p16-v32-k100000-S | valkey | writes | 3 | 798041 | 0.407 | 99.92 | 88.89 | 88.89 | true |
| c100-t2-p16-v32-k100000-S | reddish | mixed | 3 | 1007767 | 2.367 | 61.73 | 100.22 | 100.22 | false |
| c100-t2-p16-v32-k100000-S | reddish | reads | 3 | 1053136 | 2.255 | 66.09 | 100.21 | 100.20 | false |
| c100-t2-p16-v32-k100000-S | reddish | writes | 3 | 957443 | 2.495 | 59.44 | 100.25 | 100.25 | false |
| c100-t2-p16-v32-k100000-S | redis | mixed | 3 | 904193 | 2.815 | 99.55 | 88.93 | 88.94 | false |
| c100-t2-p16-v32-k100000-S | redis | reads | 3 | 1062308 | 2.271 | 99.18 | 99.78 | 99.78 | false |
| c100-t2-p16-v32-k100000-S | redis | writes | 3 | 849767 | 2.799 | 99.45 | 87.43 | 87.44 | false |
| c100-t2-p16-v32-k100000-S | valkey | mixed | 3 | 917803 | 2.639 | 99.49 | 90.46 | 90.47 | false |
| c100-t2-p16-v32-k100000-S | valkey | reads | 3 | 1057255 | 2.223 | 99.38 | 99.55 | 99.55 | false |
| c100-t2-p16-v32-k100000-S | valkey | writes | 3 | 830479 | 2.751 | 99.59 | 85.47 | 85.47 | false |
| c100-t4-p16-v32-k100000-S | reddish | mixed | 3 | 992366 | 4.191 | 64.31 | 50.13 | 100.26 | false |
| c100-t4-p16-v32-k100000-S | reddish | reads | 3 | 1037302 | 4.159 | 68.80 | 50.14 | 100.28 | false |
| c100-t4-p16-v32-k100000-S | reddish | writes | 3 | 950457 | 4.255 | 63.51 | 50.13 | 100.25 | false |
| c100-t4-p16-v32-k100000-S | redis | mixed | 3 | 906289 | 2.831 | 99.61 | 45.00 | 89.99 | false |
| c100-t4-p16-v32-k100000-S | redis | reads | 3 | 1054462 | 4.767 | 98.38 | 49.89 | 99.78 | false |
| c100-t4-p16-v32-k100000-S | redis | writes | 3 | 853732 | 2.879 | 99.45 | 44.08 | 88.17 | false |
| c100-t4-p16-v32-k100000-S | valkey | mixed | 3 | 915319 | 2.783 | 99.60 | 45.54 | 91.08 | false |
| c100-t4-p16-v32-k100000-S | valkey | reads | 3 | 1034257 | 4.671 | 99.45 | 49.31 | 98.61 | false |
| c100-t4-p16-v32-k100000-S | valkey | writes | 3 | 839992 | 2.895 | 99.43 | 43.86 | 87.72 | false |
| c100-t2-p64-v32-k100000-S | reddish | mixed | 3 | 1576679 | 6.399 | 36.02 | 100.20 | 100.20 | false |
| c100-t2-p64-v32-k100000-S | reddish | reads | 3 | 1688547 | 5.919 | 40.60 | 100.29 | 100.30 | false |
| c100-t2-p64-v32-k100000-S | reddish | writes | 3 | 1484035 | 6.687 | 37.41 | 100.31 | 100.31 | false |
| c100-t2-p64-v32-k100000-S | redis | mixed | 3 | 1392465 | 6.655 | 99.54 | 86.29 | 86.29 | false |
| c100-t2-p64-v32-k100000-S | redis | reads | 3 | 1690583 | 6.559 | 93.85 | 98.48 | 98.47 | false |
| c100-t2-p64-v32-k100000-S | redis | writes | 3 | 1249675 | 7.391 | 99.37 | 82.90 | 82.89 | false |
| c100-t2-p64-v32-k100000-S | valkey | mixed | 3 | 1410880 | 6.975 | 99.54 | 87.26 | 87.25 | false |
| c100-t2-p64-v32-k100000-S | valkey | reads | 3 | 1692689 | 6.431 | 97.56 | 98.86 | 98.86 | false |
| c100-t2-p64-v32-k100000-S | valkey | writes | 3 | 1210625 | 8.831 | 99.57 | 80.77 | 80.77 | false |
| c10-t2-p16-v256-k100000-S | reddish | mixed | 3 | 898244 | 0.271 | 70.59 | 99.85 | 99.86 | true |
| c10-t2-p16-v256-k100000-S | reddish | reads | 3 | 584930 | 0.375 | 97.66 | 71.50 | 71.50 | true |
| c10-t2-p16-v256-k100000-S | reddish | writes | 3 | 867170 | 0.279 | 70.72 | 99.71 | 99.72 | true |
| c10-t2-p16-v256-k100000-S | redis | mixed | 3 | 866132 | 0.311 | 99.93 | 95.26 | 95.26 | true |
| c10-t2-p16-v256-k100000-S | redis | reads | 3 | 464413 | 0.615 | 99.99 | 53.00 | 52.99 | true |
| c10-t2-p16-v256-k100000-S | redis | writes | 3 | 857998 | 0.295 | 100.12 | 96.65 | 96.66 | true |
| c10-t2-p16-v256-k100000-S | valkey | mixed | 3 | 887104 | 0.287 | 99.99 | 96.63 | 96.62 | true |
| c10-t2-p16-v256-k100000-S | valkey | reads | 3 | 459928 | 0.527 | 100.05 | 52.22 | 52.23 | true |
| c10-t2-p16-v256-k100000-S | valkey | writes | 3 | 816918 | 0.407 | 99.78 | 93.95 | 93.95 | true |
| c10-t2-p16-v4096-k100000-S | reddish | mixed | 3 | 304877 | 0.879 | 99.47 | 99.77 | 99.77 | true |
| c10-t2-p16-v4096-k100000-S | reddish | reads | 3 | 139654 | 1.503 | 98.18 | 99.98 | 99.98 | true |
| c10-t2-p16-v4096-k100000-S | reddish | writes | 3 | 470779 | 0.535 | 99.93 | 90.56 | 90.56 | true |
| c10-t2-p16-v4096-k100000-S | redis | mixed | 3 | 322397 | 0.647 | 99.99 | 99.56 | 99.56 | true |
| c10-t2-p16-v4096-k100000-S | redis | reads | 3 | 203978 | 0.967 | 99.99 | 99.92 | 99.92 | true |
| c10-t2-p16-v4096-k100000-S | redis | writes | 3 | 225977 | 0.871 | 99.91 | 61.39 | 61.39 | true |
| c10-t2-p16-v4096-k100000-S | valkey | mixed | 3 | 322332 | 0.655 | 100.05 | 99.55 | 99.56 | true |
| c10-t2-p16-v4096-k100000-S | valkey | reads | 3 | 198916 | 0.967 | 99.98 | 99.91 | 99.92 | true |
| c10-t2-p16-v4096-k100000-S | valkey | writes | 3 | 223445 | 1.207 | 99.99 | 59.56 | 59.56 | true |
| c10-t2-p16-v32-k1000000-S | reddish | mixed | 3 | 961105 | 0.247 | 71.86 | 99.79 | 99.80 | true |
| c10-t2-p16-v32-k1000000-S | reddish | reads | 3 | 1019487 | 0.239 | 77.46 | 99.85 | 99.86 | true |
| c10-t2-p16-v32-k1000000-S | reddish | writes | 3 | 927041 | 0.255 | 74.39 | 99.78 | 99.78 | true |
| c10-t2-p16-v32-k1000000-S | redis | mixed | 3 | 870503 | 0.279 | 99.99 | 92.55 | 92.56 | true |
| c10-t2-p16-v32-k1000000-S | redis | reads | 3 | 1030500 | 0.263 | 99.99 | 98.02 | 98.02 | true |
| c10-t2-p16-v32-k1000000-S | redis | writes | 3 | 811613 | 0.303 | 99.92 | 90.15 | 90.16 | true |
| c10-t2-p16-v32-k1000000-S | valkey | mixed | 3 | 892220 | 0.279 | 99.93 | 93.77 | 93.77 | true |
| c10-t2-p16-v32-k1000000-S | valkey | reads | 3 | 1010176 | 0.255 | 99.85 | 97.61 | 97.61 | true |
| c10-t2-p16-v32-k1000000-S | valkey | writes | 3 | 804722 | 0.311 | 99.99 | 89.53 | 89.53 | true |
| c10-t2-p16-v32-k100000-R | reddish | mixed | 3 | 950057 | 0.255 | 71.72 | 99.75 | 99.75 | true |
| c10-t2-p16-v32-k100000-R | reddish | reads | 3 | 1001552 | 0.239 | 74.32 | 99.89 | 99.89 | true |
| c10-t2-p16-v32-k100000-R | reddish | writes | 3 | 914120 | 0.263 | 70.25 | 99.88 | 99.88 | true |
| c10-t2-p16-v32-k100000-R | redis | mixed | 3 | 864329 | 0.303 | 99.99 | 92.49 | 92.50 | true |
| c10-t2-p16-v32-k100000-R | redis | reads | 3 | 1033743 | 0.255 | 99.91 | 98.78 | 98.78 | true |
| c10-t2-p16-v32-k100000-R | redis | writes | 3 | 819406 | 0.295 | 99.93 | 91.46 | 91.46 | true |
| c10-t2-p16-v32-k100000-R | valkey | mixed | 3 | 843095 | 0.295 | 99.99 | 91.33 | 91.33 | true |
| c10-t2-p16-v32-k100000-R | valkey | reads | 3 | 993786 | 0.271 | 99.98 | 97.35 | 97.34 | true |
| c10-t2-p16-v32-k100000-R | valkey | writes | 3 | 783066 | 0.303 | 99.93 | 88.19 | 88.19 | true |

Case names record total clients (c), client threads (t), pipeline (p), bytes (v), keys (k), and sequential/random pattern (S/R). Client budget uses the available CPU count reported by nproc under the assigned affinity; SMT threads share physical resources. Older data without that count is NA.

</details>

## Pressure scenarios

Reddish only. These are bounded correctness scenarios, not comparative throughput measurements or a long soak test. Churn RSS can remain reserved after keys are freed.

Peak RSS includes the kernel high-water mark captured before each process stops.

```text
PRESSURE_KEYS=10000 COLLECTION_ITEMS=4096 CHURN_KEYS=1024
```

| Scenario | Result | Assertions | Elapsed ms | Peak RSS KiB | Final keys | Expired keys | Evictions |
|---|---|---:|---:|---:|---:|---:|---:|
| expiry | pass | 3 | 4110 | 5476 | 0 | 10000 | 0 |
| eviction | pass | 4 | 2946 | 5152 | 10000 | 0 | 10000 |
| large_list | pass | 6 | 707 | 4628 | 0 | 0 | 0 |
| promoted_hash | pass | 8 | 759 | 4960 | 0 | 0 | 0 |
| churn | pass | 19 | 4238 | 20908 | 0 | 0 | 0 |
| backpressure | pass | 3 | 284 | 6212 | 1 | 0 | 0 |
| durable_recovery | pass | 10 | 1336 | 4452 | 2 | 1 | 0 |

## Notes

- This is a single-machine loopback test. WSL host scheduling and clock changes can cause large trial variance. Compare the ranges as well as the means.
- GET and mixed samples must have zero misses. Preload key counts and fixed request counts are checked; failed requests invalidate the run.
- Paired extended workloads report commands per second, not completed pairs. set_del runs SET/DEL, list pairs push/pop, hset_hdel inserts/deletes a field, and expire_ttl runs SET/EXPIRE/TTL.
- Memcached is measured only for GET, SET, and string RSS. It does not implement the other workloads.

## Individual throughput trials

<details>
<summary>All measured samples</summary>

| Workload | Server | Trial | Ops/s | Mean latency ms | p99 ms |
|---|---|---:|---:|---:|---:|
| expire_ttl | reddish | 1 | 759707 | 0.208 | 0.311 |
| expire_ttl | reddish | 2 | 779145 | 0.203 | 0.303 |
| expire_ttl | reddish | 3 | 720950 | 0.219 | 0.335 |
| expire_ttl | reddish | 4 | 754835 | 0.209 | 0.319 |
| expire_ttl | reddish | 5 | 756941 | 0.209 | 0.319 |
| expire_ttl | redis | 1 | 794092 | 0.199 | 0.295 |
| expire_ttl | redis | 2 | 794710 | 0.199 | 0.295 |
| expire_ttl | redis | 3 | 791524 | 0.199 | 0.303 |
| expire_ttl | redis | 4 | 798865 | 0.198 | 0.295 |
| expire_ttl | redis | 5 | 794252 | 0.199 | 0.311 |
| expire_ttl | valkey | 1 | 777090 | 0.203 | 0.487 |
| expire_ttl | valkey | 2 | 782682 | 0.202 | 0.471 |
| expire_ttl | valkey | 3 | 777967 | 0.203 | 0.487 |
| expire_ttl | valkey | 4 | 779104 | 0.203 | 0.487 |
| expire_ttl | valkey | 5 | 774953 | 0.204 | 0.487 |
| hget | reddish | 1 | 708538 | 0.222 | 0.335 |
| hget | reddish | 2 | 717489 | 0.219 | 0.335 |
| hget | reddish | 3 | 708411 | 0.222 | 0.335 |
| hget | reddish | 4 | 704875 | 0.223 | 0.343 |
| hget | reddish | 5 | 710534 | 0.221 | 0.335 |
| hget | redis | 1 | 737168 | 0.213 | 0.319 |
| hget | redis | 2 | 750613 | 0.209 | 0.311 |
| hget | redis | 3 | 739972 | 0.212 | 0.319 |
| hget | redis | 4 | 768478 | 0.204 | 0.303 |
| hget | redis | 5 | 742795 | 0.211 | 0.319 |
| hget | valkey | 1 | 748360 | 0.210 | 0.311 |
| hget | valkey | 2 | 745009 | 0.211 | 0.319 |
| hget | valkey | 3 | 753965 | 0.208 | 0.311 |
| hget | valkey | 4 | 741009 | 0.212 | 0.327 |
| hget | valkey | 5 | 750858 | 0.209 | 0.319 |
| hlen | reddish | 1 | 790931 | 0.200 | 0.303 |
| hlen | reddish | 2 | 779486 | 0.203 | 0.311 |
| hlen | reddish | 3 | 788108 | 0.200 | 0.303 |
| hlen | reddish | 4 | 787919 | 0.200 | 0.311 |
| hlen | reddish | 5 | 779616 | 0.203 | 0.311 |
| hlen | redis | 1 | 826080 | 0.191 | 0.287 |
| hlen | redis | 2 | 818383 | 0.193 | 0.311 |
| hlen | redis | 3 | 824381 | 0.192 | 0.295 |
| hlen | redis | 4 | 831206 | 0.190 | 0.287 |
| hlen | redis | 5 | 822926 | 0.192 | 0.295 |
| hlen | valkey | 1 | 821028 | 0.192 | 0.295 |
| hlen | valkey | 2 | 825431 | 0.191 | 0.287 |
| hlen | valkey | 3 | 823336 | 0.192 | 0.287 |
| hlen | valkey | 4 | 832662 | 0.190 | 0.303 |
| hlen | valkey | 5 | 811860 | 0.194 | 0.311 |
| hset_hdel | reddish | 1 | 703120 | 0.225 | 0.343 |
| hset_hdel | reddish | 2 | 705895 | 0.224 | 0.343 |
| hset_hdel | reddish | 3 | 706636 | 0.224 | 0.335 |
| hset_hdel | reddish | 4 | 708608 | 0.223 | 0.343 |
| hset_hdel | reddish | 5 | 704807 | 0.224 | 0.343 |
| hset_hdel | redis | 1 | 730913 | 0.216 | 0.335 |
| hset_hdel | redis | 2 | 736558 | 0.215 | 0.327 |
| hset_hdel | redis | 3 | 734364 | 0.215 | 0.327 |
| hset_hdel | redis | 4 | 731910 | 0.216 | 0.359 |
| hset_hdel | redis | 5 | 739743 | 0.214 | 0.327 |
| hset_hdel | valkey | 1 | 716430 | 0.221 | 0.335 |
| hset_hdel | valkey | 2 | 720623 | 0.219 | 0.327 |
| hset_hdel | valkey | 3 | 727451 | 0.217 | 0.327 |
| hset_hdel | valkey | 4 | 723412 | 0.219 | 0.335 |
| hset_hdel | valkey | 5 | 732032 | 0.216 | 0.327 |
| incr | reddish | 1 | 794172 | 0.199 | 0.303 |
| incr | reddish | 2 | 781503 | 0.202 | 0.311 |
| incr | reddish | 3 | 786929 | 0.201 | 0.303 |
| incr | reddish | 4 | 792088 | 0.199 | 0.303 |
| incr | reddish | 5 | 768071 | 0.206 | 0.311 |
| incr | redis | 1 | 827622 | 0.191 | 0.287 |
| incr | redis | 2 | 824868 | 0.191 | 0.287 |
| incr | redis | 3 | 827101 | 0.191 | 0.287 |
| incr | redis | 4 | 823658 | 0.192 | 0.287 |
| incr | redis | 5 | 831043 | 0.190 | 0.287 |
| incr | valkey | 1 | 826464 | 0.191 | 0.287 |
| incr | valkey | 2 | 824483 | 0.191 | 0.295 |
| incr | valkey | 3 | 827440 | 0.191 | 0.287 |
| incr | valkey | 4 | 830314 | 0.190 | 0.279 |
| incr | valkey | 5 | 827603 | 0.191 | 0.279 |
| llen | reddish | 1 | 808637 | 0.195 | 0.303 |
| llen | reddish | 2 | 783332 | 0.202 | 0.311 |
| llen | reddish | 3 | 789247 | 0.200 | 0.303 |
| llen | reddish | 4 | 788196 | 0.200 | 0.311 |
| llen | reddish | 5 | 780413 | 0.202 | 0.303 |
| llen | redis | 1 | 820153 | 0.193 | 0.295 |
| llen | redis | 2 | 839771 | 0.188 | 0.287 |
| llen | redis | 3 | 829090 | 0.190 | 0.287 |
| llen | redis | 4 | 827279 | 0.191 | 0.287 |
| llen | redis | 5 | 817186 | 0.193 | 0.303 |
| llen | valkey | 1 | 813853 | 0.194 | 0.327 |
| llen | valkey | 2 | 826825 | 0.191 | 0.287 |
| llen | valkey | 3 | 810388 | 0.195 | 0.295 |
| llen | valkey | 4 | 824320 | 0.192 | 0.287 |
| llen | valkey | 5 | 827723 | 0.191 | 0.287 |
| lpush_lpop | reddish | 1 | 709750 | 0.222 | 0.335 |
| lpush_lpop | reddish | 2 | 696359 | 0.226 | 0.343 |
| lpush_lpop | reddish | 3 | 698624 | 0.226 | 0.343 |
| lpush_lpop | reddish | 4 | 700703 | 0.225 | 0.343 |
| lpush_lpop | reddish | 5 | 692086 | 0.228 | 0.351 |
| lpush_lpop | redis | 1 | 746817 | 0.211 | 0.303 |
| lpush_lpop | redis | 2 | 749228 | 0.210 | 0.343 |
| lpush_lpop | redis | 3 | 757766 | 0.208 | 0.295 |
| lpush_lpop | redis | 4 | 751141 | 0.210 | 0.303 |
| lpush_lpop | redis | 5 | 758768 | 0.207 | 0.295 |
| lpush_lpop | valkey | 1 | 753743 | 0.209 | 0.303 |
| lpush_lpop | valkey | 2 | 758060 | 0.208 | 0.303 |
| lpush_lpop | valkey | 3 | 751328 | 0.209 | 0.335 |
| lpush_lpop | valkey | 4 | 754421 | 0.209 | 0.311 |
| lpush_lpop | valkey | 5 | 759347 | 0.207 | 0.295 |
| mixed | reddish | 1 | 762222 | 0.207 | 0.319 |
| mixed | reddish | 2 | 747700 | 0.211 | 0.319 |
| mixed | reddish | 3 | 740197 | 0.213 | 0.327 |
| mixed | reddish | 4 | 756713 | 0.208 | 0.319 |
| mixed | reddish | 5 | 733952 | 0.215 | 0.327 |
| mixed | redis | 1 | 811370 | 0.194 | 0.287 |
| mixed | redis | 2 | 802739 | 0.196 | 0.287 |
| mixed | redis | 3 | 802077 | 0.196 | 0.303 |
| mixed | redis | 4 | 797556 | 0.197 | 0.287 |
| mixed | redis | 5 | 808888 | 0.195 | 0.287 |
| mixed | valkey | 1 | 800792 | 0.197 | 0.295 |
| mixed | valkey | 2 | 802880 | 0.196 | 0.287 |
| mixed | valkey | 3 | 811786 | 0.194 | 0.287 |
| mixed | valkey | 4 | 801588 | 0.197 | 0.287 |
| mixed | valkey | 5 | 803103 | 0.196 | 0.303 |
| ping | reddish | 1 | 1029124 | 0.153 | 0.239 |
| ping | reddish | 2 | 1035228 | 0.152 | 0.231 |
| ping | reddish | 3 | 1039390 | 0.151 | 0.239 |
| ping | reddish | 4 | 1034459 | 0.152 | 0.239 |
| ping | reddish | 5 | 1039205 | 0.151 | 0.231 |
| ping | redis | 1 | 1080708 | 0.145 | 0.223 |
| ping | redis | 2 | 1075178 | 0.146 | 0.223 |
| ping | redis | 3 | 1076038 | 0.146 | 0.223 |
| ping | redis | 4 | 1083021 | 0.145 | 0.223 |
| ping | redis | 5 | 1084313 | 0.145 | 0.215 |
| ping | valkey | 1 | 1098467 | 0.143 | 0.215 |
| ping | valkey | 2 | 1100056 | 0.143 | 0.215 |
| ping | valkey | 3 | 1096179 | 0.143 | 0.215 |
| ping | valkey | 4 | 1086351 | 0.145 | 0.223 |
| ping | valkey | 5 | 1095410 | 0.143 | 0.215 |
| reads | memcached | 1 | 967934 | 0.163 | 0.239 |
| reads | memcached | 2 | 970911 | 0.162 | 0.247 |
| reads | memcached | 3 | 965035 | 0.163 | 0.239 |
| reads | memcached | 4 | 966069 | 0.163 | 0.239 |
| reads | memcached | 5 | 958325 | 0.165 | 0.247 |
| reads | reddish | 1 | 792583 | 0.199 | 0.303 |
| reads | reddish | 2 | 775000 | 0.203 | 0.311 |
| reads | reddish | 3 | 793792 | 0.198 | 0.303 |
| reads | reddish | 4 | 787926 | 0.200 | 0.303 |
| reads | reddish | 5 | 790554 | 0.199 | 0.303 |
| reads | redis | 1 | 819773 | 0.192 | 0.303 |
| reads | redis | 2 | 845265 | 0.186 | 0.279 |
| reads | redis | 3 | 834080 | 0.189 | 0.287 |
| reads | redis | 4 | 838719 | 0.187 | 0.287 |
| reads | redis | 5 | 830125 | 0.190 | 0.287 |
| reads | valkey | 1 | 841336 | 0.187 | 0.279 |
| reads | valkey | 2 | 842483 | 0.187 | 0.303 |
| reads | valkey | 3 | 842015 | 0.187 | 0.279 |
| reads | valkey | 4 | 836022 | 0.188 | 0.279 |
| reads | valkey | 5 | 851952 | 0.185 | 0.271 |
| rpush_rpop | reddish | 1 | 694061 | 0.227 | 0.343 |
| rpush_rpop | reddish | 2 | 698790 | 0.225 | 0.343 |
| rpush_rpop | reddish | 3 | 680284 | 0.232 | 0.351 |
| rpush_rpop | reddish | 4 | 695952 | 0.226 | 0.343 |
| rpush_rpop | reddish | 5 | 685945 | 0.230 | 0.351 |
| rpush_rpop | redis | 1 | 750725 | 0.210 | 0.303 |
| rpush_rpop | redis | 2 | 748368 | 0.210 | 0.303 |
| rpush_rpop | redis | 3 | 719883 | 0.219 | 0.311 |
| rpush_rpop | redis | 4 | 749326 | 0.210 | 0.343 |
| rpush_rpop | redis | 5 | 751061 | 0.210 | 0.319 |
| rpush_rpop | valkey | 1 | 750843 | 0.210 | 0.303 |
| rpush_rpop | valkey | 2 | 749998 | 0.210 | 0.351 |
| rpush_rpop | valkey | 3 | 751157 | 0.210 | 0.303 |
| rpush_rpop | valkey | 4 | 752604 | 0.209 | 0.311 |
| rpush_rpop | valkey | 5 | 759333 | 0.207 | 0.327 |
| set_del | reddish | 1 | 731486 | 0.216 | 0.359 |
| set_del | reddish | 2 | 732033 | 0.216 | 0.327 |
| set_del | reddish | 3 | 728583 | 0.217 | 0.335 |
| set_del | reddish | 4 | 726774 | 0.218 | 0.335 |
| set_del | reddish | 5 | 734779 | 0.215 | 0.327 |
| set_del | redis | 1 | 776873 | 0.203 | 0.303 |
| set_del | redis | 2 | 775036 | 0.204 | 0.303 |
| set_del | redis | 3 | 782297 | 0.202 | 0.295 |
| set_del | redis | 4 | 776345 | 0.203 | 0.295 |
| set_del | redis | 5 | 776071 | 0.204 | 0.303 |
| set_del | valkey | 1 | 780938 | 0.202 | 0.295 |
| set_del | valkey | 2 | 776680 | 0.203 | 0.295 |
| set_del | valkey | 3 | 775480 | 0.204 | 0.295 |
| set_del | valkey | 4 | 778001 | 0.203 | 0.295 |
| set_del | valkey | 5 | 774177 | 0.204 | 0.311 |
| writes | memcached | 1 | 934708 | 0.169 | 0.255 |
| writes | memcached | 2 | 937328 | 0.169 | 0.255 |
| writes | memcached | 3 | 940933 | 0.168 | 0.247 |
| writes | memcached | 4 | 951459 | 0.166 | 0.247 |
| writes | memcached | 5 | 939343 | 0.168 | 0.255 |
| writes | reddish | 1 | 723363 | 0.219 | 0.335 |
| writes | reddish | 2 | 714130 | 0.221 | 0.343 |
| writes | reddish | 3 | 721774 | 0.219 | 0.335 |
| writes | reddish | 4 | 723845 | 0.218 | 0.335 |
| writes | reddish | 5 | 718756 | 0.220 | 0.343 |
| writes | redis | 1 | 771753 | 0.205 | 0.303 |
| writes | redis | 2 | 768044 | 0.206 | 0.303 |
| writes | redis | 3 | 770677 | 0.205 | 0.295 |
| writes | redis | 4 | 773579 | 0.204 | 0.295 |
| writes | redis | 5 | 777616 | 0.203 | 0.295 |
| writes | valkey | 1 | 777786 | 0.203 | 0.295 |
| writes | valkey | 2 | 778897 | 0.203 | 0.303 |
| writes | valkey | 3 | 779294 | 0.203 | 0.295 |
| writes | valkey | 4 | 775126 | 0.204 | 0.303 |
| writes | valkey | 5 | 780153 | 0.202 | 0.295 |

</details>

## Individual memory trials

<details>
<summary>All measured samples</summary>

| Shape | Server | Trial | Keys | Idle RSS KiB | Loaded RSS KiB | Delta bytes/key |
|---|---|---:|---:|---:|---:|---:|
| hashes | reddish | 1 | 100000 | 4056 | 14692 | 108.91 |
| hashes | reddish | 2 | 100000 | 4020 | 14656 | 108.91 |
| hashes | reddish | 3 | 100000 | 4056 | 14692 | 108.91 |
| hashes | redis | 1 | 100000 | 8560 | 19272 | 109.69 |
| hashes | redis | 2 | 100000 | 8640 | 19304 | 109.20 |
| hashes | redis | 3 | 100000 | 8632 | 19236 | 108.58 |
| hashes | valkey | 1 | 100000 | 7324 | 17464 | 103.83 |
| hashes | valkey | 2 | 100000 | 7192 | 17388 | 104.41 |
| hashes | valkey | 3 | 100000 | 7376 | 17508 | 103.75 |
| lists | reddish | 1 | 100000 | 4048 | 15388 | 116.12 |
| lists | reddish | 2 | 100000 | 4020 | 15360 | 116.12 |
| lists | reddish | 3 | 100000 | 4060 | 15400 | 116.12 |
| lists | redis | 1 | 100000 | 8608 | 19408 | 110.59 |
| lists | redis | 2 | 100000 | 8696 | 19344 | 109.04 |
| lists | redis | 3 | 100000 | 8660 | 19384 | 109.81 |
| lists | valkey | 1 | 100000 | 7404 | 17472 | 103.10 |
| lists | valkey | 2 | 100000 | 7324 | 17440 | 103.59 |
| lists | valkey | 3 | 100000 | 7388 | 17452 | 103.06 |
| strings | memcached | 1 | 100000 | 4664 | 18052 | 137.09 |
| strings | memcached | 2 | 100000 | 4660 | 18048 | 137.09 |
| strings | memcached | 3 | 100000 | 4636 | 18024 | 137.09 |
| strings | reddish | 1 | 100000 | 4020 | 12996 | 91.91 |
| strings | reddish | 2 | 100000 | 4004 | 12976 | 91.87 |
| strings | reddish | 3 | 100000 | 4048 | 13020 | 91.87 |
| strings | redis | 1 | 100000 | 8820 | 18040 | 94.41 |
| strings | redis | 2 | 100000 | 8700 | 17908 | 94.29 |
| strings | redis | 3 | 100000 | 8560 | 17768 | 94.29 |
| strings | valkey | 1 | 100000 | 7324 | 15868 | 87.49 |
| strings | valkey | 2 | 100000 | 7452 | 16000 | 87.53 |
| strings | valkey | 3 | 100000 | 7388 | 15932 | 87.49 |

</details>

## Client driver checks

Fixed 16 connections, 100000 keys, 32-byte values, GET, and the recorded CPU affinity. Throughput is total operations divided by the envelope of all process start/finish times. Worst p99 is the worst process percentile, not a pooled percentile. Extra workers sharing assigned cores are not extra physical capacity.

| Server | Processes | Threads/process | Pipeline | Trials | Mean ops/s | Worst process p99 ms |
|---|---:|---:|---:|---:|---:|---:|
| reddish | 1 | 1 | 16 | 3 | 813148 | 0.487 |
| reddish | 1 | 1 | 64 | 3 | 1496551 | 1.031 |
| reddish | 1 | 2 | 16 | 3 | 1024918 | 0.375 |
| reddish | 1 | 2 | 64 | 3 | 1703391 | 0.903 |
| reddish | 1 | 4 | 16 | 3 | 1010555 | 3.183 |
| reddish | 1 | 4 | 64 | 3 | 1684098 | 3.391 |
| reddish | 4 | 1 | 16 | 3 | 984288 | 3.199 |
| reddish | 4 | 1 | 64 | 3 | 1643290 | 3.407 |
| redis | 1 | 1 | 16 | 3 | 850611 | 0.447 |
| redis | 1 | 1 | 64 | 3 | 1511110 | 1.079 |
| redis | 1 | 2 | 16 | 3 | 1066280 | 0.391 |
| redis | 1 | 2 | 64 | 3 | 1720881 | 0.967 |
| redis | 1 | 4 | 16 | 3 | 1040879 | 3.135 |
| redis | 1 | 4 | 64 | 3 | 1638124 | 2.191 |
| redis | 4 | 1 | 16 | 3 | 1023948 | 3.167 |
| redis | 4 | 1 | 64 | 3 | 1584336 | 2.767 |
| valkey | 1 | 1 | 16 | 3 | 847677 | 0.503 |
| valkey | 1 | 1 | 64 | 3 | 1516238 | 1.127 |
| valkey | 1 | 2 | 16 | 3 | 1004424 | 0.407 |
| valkey | 1 | 2 | 64 | 3 | 1591285 | 0.999 |
| valkey | 1 | 4 | 16 | 3 | 975605 | 0.535 |
| valkey | 1 | 4 | 64 | 3 | 1572329 | 1.199 |
| valkey | 4 | 1 | 16 | 3 | 974042 | 1.943 |
| valkey | 4 | 1 | 64 | 3 | 1552897 | 1.567 |

## Sustained eviction and expiry

Mixed SET/GET with 20000 possible keys, a 10000-key limit and an 8 MiB owned-allocation budget. Misses are expected under eviction; request and connection errors are rejected. Concurrent EXPIRE calls may produce zero expirations when SET clears TTLs or eviction removes keys first. The separate expiry pressure scenario verifies actual expiration. RSS includes allocator and process memory outside the database budget.

Observed samples: 350; last sample: 1799 seconds; peak sampled owned memory: 3.26 MiB; peak sampled RSS: 7.29 MiB; expired keys: 0; evictions: 79143521.

## Measurement manifest

<details>
<summary>Revision, executable hash, hardware, tools, and settings</summary>

```text
c7e0ec2c428ad563635ca8c81803f4598aa3e98a
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  -
6fc9a8ff393fa269a2fbd431c9eb33fed4d819df5da564f02f8ee19219860145  /home/runner/work/reddish/reddish/build/output/reddish
Measured at 2026-10-31 20:26:40 UTC
WORKTREE_STATE=clean
Linux runnervm48cm3 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
Architecture:                            x86_64
CPU op-mode(s):                          32-bit, 64-bit
Address sizes:                           48 bits physical, 48 bits virtual
Byte Order:                              Little Endian
CPU(s):                                  4
On-line CPU(s) list:                     0-3
Vendor ID:                               AuthenticAMD
Model name:                              AMD EPYC 7763 64-Core Processor
CPU family:                              25
Model:                                   1
Thread(s) per core:                      2
Core(s) per socket:                      2
Socket(s):                               1
Stepping:                                1
BogoMIPS:                                4890.86
Flags:                                   fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl tsc_reliable nonstop_tsc cpuid extd_apicid aperfmperf tsc_known_freq pni pclmulqdq ssse3 fma cx16 pcid sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand hypervisor lahf_lm cmp_legacy svm cr8_legacy abm sse4a misalignsse 3dnowprefetch osvw topoext vmmcall fsgsbase bmi1 avx2 smep bmi2 erms invpcid rdseed adx smap clflushopt clwb sha_ni xsaveopt xsavec xgetbv1 xsaves user_shstk clzero xsaveerptr rdpru arat npt nrip_save tsc_scale vmcb_clean flushbyasid decodeassists pausefilter pfthreshold v_vmsave_vmload umip vaes vpclmulqdq rdpid fsrm
Virtualization:                          AMD-V
Hypervisor vendor:                       Microsoft
Virtualization type:                     full
L1d cache:                               64 KiB (2 instances)
L1i cache:                               64 KiB (2 instances)
L2 cache:                                1 MiB (2 instances)
L3 cache:                                32 MiB (1 instance)
NUMA node(s):                            1
NUMA node0 CPU(s):                       0-3
Vulnerability Gather data sampling:      Not affected
Vulnerability Ghostwrite:                Not affected
Vulnerability Indirect target selection: Not affected
Vulnerability Itlb multihit:             Not affected
Vulnerability L1tf:                      Not affected
Vulnerability Mds:                       Not affected
Vulnerability Meltdown:                  Not affected
Vulnerability Mmio stale data:           Not affected
Vulnerability Old microcode:             Not affected
Vulnerability Reg file data sampling:    Not affected
Vulnerability Retbleed:                  Not affected
Vulnerability Spec rstack overflow:      Vulnerable: Safe RET, no microcode
Vulnerability Spec store bypass:         Vulnerable
Vulnerability Spectre v1:                Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:                Mitigation; Retpolines; STIBP disabled; RSB filling; PBRSB-eIBRS Not affected; BHI Not affected
Vulnerability Srbds:                     Not affected
Vulnerability Tsa:                       Vulnerable: No microcode
Vulnerability Tsx async abort:           Not affected
Vulnerability Vmscape:                   Not affected
memtier_benchmark v=2.5.1 sha=5f634d17:0 bits=64 libevent=2.1.12-stable openssl=OpenSSL 3.0.13 30 Jan 2024 prometheus=yes
Copyright (C) 2011-2026 Redis Ltd.
This is free software.  You may redistribute copies of it under the terms of
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.
There is NO WARRANTY, to the extent permitted by law.
Redis server v=8.10.1 sha=3399357e:0 malloc=jemalloc-5.3.0 bits=64 build=655bce6717d2446
Valkey server v=8.1.1 sha=fcd8bc3e:0 malloc=jemalloc-5.3.0 bits=64 build=d189c1deef07e8aa
memcached 1.6.24
SCHEDSTATS_MODE=1
SERVER_CPU=0 CLIENT_CPUS=2,3
REQUESTS_PER_CLIENT=0 KEY_PATTERN=S:S
RUN_PERFORMANCE=1 RUN_MEMORY=1 RUN_VALIDATION=0
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=5 DURATION_SECONDS=30 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=5 persistence=disabled
```

</details>
