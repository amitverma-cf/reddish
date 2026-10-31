# Benchmark results

Measured revision: [da330d0](https://github.com/amitverma-cf/reddish/commit/da330d0debfc2e3e7f965da3f0bcdd35e0fc7fc4).

Environment: **GitHub Actions / Ubuntu 24.04**. Measured: **2026-10-31 13:17:53 UTC**.

[Source workflow run](https://github.com/amitverma-cf/reddish/actions/runs/41204638257). Raw data and profiles are attached to that run.

These are results for this machine and workload. They do not establish a general ranking or maximum server capacity.

Loopback TCP on the machine below. Persistence is disabled for the server comparison. Each server receives the same preload, warmup, values, clients, and pipeline. Server order rotates between trials; GET and SET order alternates.

## Setup

```text
Measured at 2026-10-31 13:17:53 UTC
Linux runnervm48cm3 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
CPU(s):                                  4
Model name:                              AMD EPYC 9V45 96-Core Processor
memtier_benchmark v=2.5.1 sha=5f634d17:0 bits=64 libevent=2.1.12-stable openssl=OpenSSL 3.0.13 30 Jan 2024 prometheus=yes
Redis server v=8.10.1 sha=3399357e:0 malloc=jemalloc-5.3.0 bits=64 build=169b86a7ab614613
Valkey server v=8.1.1 sha=fcd8bc3e:0 malloc=jemalloc-5.3.0 bits=64 build=1eb268b486ff9107
memcached 1.6.24
SERVER_CPU=0 CLIENT_CPUS=2,3
REQUESTS_PER_CLIENT=0 KEY_PATTERN=S:S
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=5 DURATION_SECONDS=30 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=5 persistence=disabled
```

## Throughput

Operations per second. Range and standard deviation describe the trial means, not individual requests.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 5 | 1170320 | 1142022 | 1209823 | 26103 |
| mixed | redis | 5 | 1236031 | 1181200 | 1262985 | 31610 |
| mixed | valkey | 5 | 1214440 | 1180592 | 1265541 | 35342 |
| reads | memcached | 5 | 1299509 | 1045371 | 1371523 | 142390 |
| reads | reddish | 5 | 1210722 | 1188725 | 1241256 | 25726 |
| reads | redis | 5 | 1257425 | 1228005 | 1302413 | 30481 |
| reads | valkey | 5 | 1250783 | 1221388 | 1292687 | 37380 |
| writes | memcached | 5 | 1238113 | 1028809 | 1335352 | 134091 |
| writes | reddish | 5 | 1127578 | 1089749 | 1161024 | 27097 |
| writes | redis | 5 | 1201410 | 1150642 | 1223287 | 29225 |
| writes | valkey | 5 | 1162666 | 1107974 | 1230342 | 59172 |

## Other commands

Each paired workload reports individual commands per second.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 5 | 1149630 | 1103346 | 1178732 | 32597 |
| expire_ttl | redis | 5 | 1190755 | 1146344 | 1228688 | 39687 |
| expire_ttl | valkey | 5 | 1174321 | 1146633 | 1211318 | 24088 |
| hget | reddish | 5 | 1122393 | 1096224 | 1157952 | 24186 |
| hget | redis | 5 | 1154898 | 1109972 | 1186399 | 32716 |
| hget | valkey | 5 | 1154688 | 1114103 | 1185875 | 28849 |
| hlen | reddish | 5 | 1200104 | 1172787 | 1232387 | 26116 |
| hlen | redis | 5 | 1224662 | 1176687 | 1268312 | 36266 |
| hlen | valkey | 5 | 1242460 | 1209907 | 1286925 | 30823 |
| hset_hdel | reddish | 5 | 1104350 | 1077878 | 1133764 | 24966 |
| hset_hdel | redis | 5 | 1144835 | 1098291 | 1182846 | 40137 |
| hset_hdel | valkey | 5 | 1133556 | 1111237 | 1152193 | 16025 |
| incr | reddish | 5 | 1206533 | 1166921 | 1246630 | 33132 |
| incr | redis | 5 | 1247767 | 1201972 | 1286986 | 31372 |
| incr | valkey | 5 | 1233732 | 1191560 | 1283497 | 38081 |
| llen | reddish | 5 | 1197508 | 1176792 | 1236513 | 22750 |
| llen | redis | 5 | 1226749 | 1182090 | 1291654 | 44432 |
| llen | valkey | 5 | 1248526 | 1199648 | 1285049 | 34566 |
| lpush_lpop | reddish | 5 | 1099397 | 1082939 | 1117365 | 15144 |
| lpush_lpop | redis | 5 | 1149128 | 1097182 | 1189189 | 36682 |
| lpush_lpop | valkey | 5 | 1160304 | 1117464 | 1197951 | 38311 |
| ping | reddish | 5 | 1462308 | 1429150 | 1506888 | 30791 |
| ping | redis | 5 | 1530336 | 1459492 | 1568989 | 44196 |
| ping | valkey | 5 | 1509818 | 1446265 | 1553951 | 52755 |
| rpush_rpop | reddish | 5 | 1092452 | 1072065 | 1135318 | 25302 |
| rpush_rpop | redis | 5 | 1154531 | 1117615 | 1210167 | 35531 |
| rpush_rpop | valkey | 5 | 1164029 | 1123769 | 1208018 | 36384 |
| set_del | reddish | 5 | 1132323 | 1096639 | 1160567 | 27438 |
| set_del | redis | 5 | 1166468 | 1117368 | 1197651 | 35260 |
| set_del | valkey | 5 | 1159513 | 1105178 | 1197742 | 39029 |

## Latency and CPU

Latency is in milliseconds. Percentiles are averages of each trial percentile; they are not pooled percentiles. CPU is server user plus system time per operation, excluding the load generator.

| Workload | Server | Mean latency ms | p50 ms | p99 ms | p99.9 ms | CPU µs/op |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 0.138 | 0.130 | 0.207 | 0.257 | 0.644 |
| expire_ttl | redis | 0.133 | 0.138 | 0.193 | 0.281 | 0.832 |
| expire_ttl | valkey | 0.135 | 0.138 | 0.269 | 0.524 | 0.842 |
| hget | reddish | 0.141 | 0.133 | 0.220 | 0.321 | 0.621 |
| hget | redis | 0.137 | 0.129 | 0.201 | 0.287 | 0.790 |
| hget | valkey | 0.136 | 0.129 | 0.202 | 0.284 | 0.773 |
| hlen | reddish | 0.132 | 0.125 | 0.202 | 0.386 | 0.576 |
| hlen | redis | 0.129 | 0.117 | 0.212 | 0.396 | 0.735 |
| hlen | valkey | 0.127 | 0.117 | 0.204 | 0.362 | 0.731 |
| hset_hdel | reddish | 0.144 | 0.138 | 0.223 | 0.386 | 0.593 |
| hset_hdel | redis | 0.138 | 0.138 | 0.199 | 0.325 | 0.872 |
| hset_hdel | valkey | 0.139 | 0.141 | 0.226 | 0.402 | 0.881 |
| incr | reddish | 0.131 | 0.125 | 0.196 | 0.213 | 0.561 |
| incr | redis | 0.127 | 0.117 | 0.188 | 0.301 | 0.728 |
| incr | valkey | 0.128 | 0.117 | 0.189 | 0.266 | 0.735 |
| llen | reddish | 0.132 | 0.125 | 0.202 | 0.314 | 0.579 |
| llen | redis | 0.129 | 0.117 | 0.196 | 0.372 | 0.734 |
| llen | valkey | 0.127 | 0.117 | 0.186 | 0.274 | 0.714 |
| lpush_lpop | reddish | 0.144 | 0.138 | 0.217 | 0.343 | 0.621 |
| lpush_lpop | redis | 0.138 | 0.143 | 0.196 | 0.284 | 0.862 |
| lpush_lpop | valkey | 0.136 | 0.138 | 0.189 | 0.295 | 0.859 |
| mixed | reddish | 0.135 | 0.132 | 0.205 | 0.322 | 0.557 |
| mixed | redis | 0.128 | 0.127 | 0.183 | 0.293 | 0.797 |
| mixed | valkey | 0.130 | 0.127 | 0.189 | 0.325 | 0.805 |
| ping | reddish | 0.108 | 0.103 | 0.165 | 0.292 | 0.476 |
| ping | redis | 0.103 | 0.097 | 0.167 | 0.393 | 0.607 |
| ping | valkey | 0.104 | 0.098 | 0.154 | 0.287 | 0.623 |
| reads | memcached | 0.123 | 0.109 | 0.313 | 0.637 | 0.721 |
| reads | reddish | 0.130 | 0.124 | 0.196 | 0.343 | 0.565 |
| reads | redis | 0.125 | 0.116 | 0.186 | 0.308 | 0.743 |
| reads | valkey | 0.126 | 0.117 | 0.188 | 0.335 | 0.761 |
| rpush_rpop | reddish | 0.144 | 0.140 | 0.225 | 0.345 | 0.616 |
| rpush_rpop | redis | 0.137 | 0.141 | 0.193 | 0.303 | 0.859 |
| rpush_rpop | valkey | 0.136 | 0.138 | 0.189 | 0.271 | 0.856 |
| set_del | reddish | 0.140 | 0.135 | 0.213 | 0.260 | 0.586 |
| set_del | redis | 0.136 | 0.125 | 0.220 | 0.397 | 0.807 |
| set_del | valkey | 0.137 | 0.127 | 0.209 | 0.373 | 0.819 |
| writes | memcached | 0.130 | 0.116 | 0.308 | 0.487 | 0.782 |
| writes | reddish | 0.140 | 0.137 | 0.213 | 0.362 | 0.570 |
| writes | redis | 0.132 | 0.133 | 0.189 | 0.372 | 0.819 |
| writes | valkey | 0.136 | 0.137 | 0.204 | 0.373 | 0.853 |

## Memory

Fresh server processes are measured before and after each preload. Each key holds one string, one list element, or one hash field. RSS includes process and allocator overhead. Delta bytes/key is (loaded RSS − idle RSS) × 1024 / verified key count. It is not the exact allocation size of a value.

| Shape | Server | Trials | Keys | Idle RSS MiB | Loaded RSS MiB | Delta bytes/key | Min bytes/key | Max bytes/key |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| hashes | reddish | 3 | 100000 | 3.97 | 14.36 | 108.91 | 108.91 | 108.91 |
| hashes | redis | 3 | 100000 | 8.54 | 19.04 | 110.07 | 109.20 | 110.51 |
| hashes | valkey | 3 | 100000 | 7.28 | 17.17 | 103.74 | 103.06 | 104.41 |
| lists | reddish | 3 | 100000 | 4.00 | 15.07 | 116.12 | 116.12 | 116.12 |
| lists | redis | 3 | 100000 | 8.48 | 18.98 | 110.04 | 109.77 | 110.55 |
| lists | valkey | 3 | 100000 | 7.33 | 17.18 | 103.30 | 103.01 | 103.71 |
| strings | memcached | 3 | 100000 | 4.60 | 17.67 | 137.09 | 137.09 | 137.09 |
| strings | reddish | 3 | 100000 | 3.98 | 12.75 | 91.95 | 91.87 | 92.12 |
| strings | redis | 3 | 100000 | 8.53 | 17.36 | 92.56 | 89.33 | 94.21 |
| strings | valkey | 3 | 100000 | 7.38 | 15.73 | 87.52 | 87.49 | 87.53 |

## WAL modes

Reddish only. These samples include WAL cost and use a separate workload; they are not part of the persistence-disabled comparison.

These samples use 32-byte values and a 50/50 SET/GET mix without CPU binding.

```text
KEYS=1000 CLIENTS=10 PIPELINE=16 DURATION_SECONDS=5 TRIALS=3 WARMUP_SECONDS=2
```

| Mode | Trials | Mean ops/s | Min | Max | Mean p99 ms | CPU µs/op |
|---|---:|---:|---:|---:|---:|---:|
| always | 3 | 70834 | 68697 | 72120 | 4.468 | 1.863 |
| everysec | 3 | 997188 | 991157 | 1000710 | 0.207 | 0.598 |
| off | 3 | 1144278 | 1137156 | 1150355 | 0.239 | 0.516 |

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
| mixed | reddish | 65.21 | 99.96 | 0.12 | 34.71 | 34456 |
| mixed | redis | 98.49 | 99.81 | 0.02 | 1.49 | 4261 |
| mixed | valkey | 97.80 | 99.70 | 0.02 | 2.17 | 5431 |
| reads | memcached | 93.64 | 93.63 | 0.00 | 100.00 | 1 |
| reads | reddish | 68.39 | 99.91 | 0.13 | 31.52 | 32220 |
| reads | redis | 93.43 | 99.83 | 0.04 | 6.53 | 13479 |
| reads | valkey | 95.19 | 99.68 | 0.03 | 4.78 | 9630 |
| writes | memcached | 96.76 | 93.74 | 0.00 | 100.00 | 1 |
| writes | reddish | 64.26 | 99.97 | 0.12 | 35.65 | 34691 |
| writes | redis | 98.36 | 99.70 | 0.02 | 1.62 | 3995 |
| writes | valkey | 99.17 | 98.52 | 0.01 | 0.82 | 2032 |

## Capacity measurements

Best observed points are grouped by server, workload, key count, value size and access pattern. Only points whose every trial meets the configured p99 limit qualify. These closed-loop measurements are not proof of absolute maximum capacity or an open-loop latency guarantee. Client CPU near a worker or assigned-CPU budget can limit the result. A one-trial row is a smoke measurement; repeated rows are needed for capacity conclusions.

```text
SERVER_CPU=0 CLIENT_CPUS=2,3 P99_LIMIT_MS=2 TRIALS=3 DURATION_SECONDS=5
```

| Server | Workload | Keys | Bytes | Clients | Threads | Pipeline | Pattern | Trials | Mean ops/s | Worst trial p99 ms |
|---|---|---:|---:|---:|---:|---:|---|---:|---:|---:|
| reddish | mixed | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1744400 | 0.135 |
| reddish | mixed | 100000 | 32 | 100 | 2 | 16 | S:S | 3 | 1726252 | 1.407 |
| reddish | mixed | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 1684738 | 0.135 |
| reddish | mixed | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 554429 | 0.399 |
| reddish | mixed | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1728019 | 0.159 |
| reddish | reads | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1816239 | 0.135 |
| reddish | reads | 100000 | 32 | 100 | 2 | 16 | S:S | 3 | 1795276 | 1.359 |
| reddish | reads | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 708714 | 0.503 |
| reddish | reads | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 331946 | 0.743 |
| reddish | reads | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1796137 | 0.175 |
| reddish | writes | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1663158 | 0.159 |
| reddish | writes | 100000 | 32 | 100 | 2 | 16 | S:S | 3 | 1668066 | 1.439 |
| reddish | writes | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 1590814 | 0.151 |
| reddish | writes | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 649676 | 0.367 |
| reddish | writes | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1647838 | 0.151 |
| redis | mixed | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1237668 | 0.191 |
| redis | mixed | 100000 | 32 | 100 | 2 | 16 | S:S | 3 | 1312838 | 1.735 |
| redis | mixed | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 1266703 | 0.183 |
| redis | mixed | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 527393 | 0.479 |
| redis | mixed | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1251075 | 0.255 |
| redis | reads | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1445736 | 0.175 |
| redis | reads | 100000 | 32 | 100 | 4 | 16 | S:S | 3 | 1513779 | 1.695 |
| redis | reads | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 610677 | 0.367 |
| redis | reads | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 329260 | 0.759 |
| redis | reads | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1440654 | 0.167 |
| redis | writes | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1198088 | 0.199 |
| redis | writes | 100000 | 32 | 100 | 4 | 16 | S:S | 3 | 1279186 | 1.935 |
| redis | writes | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 1294187 | 0.199 |
| redis | writes | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 319532 | 0.583 |
| redis | writes | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1222776 | 0.191 |
| valkey | mixed | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1139285 | 0.207 |
| valkey | mixed | 100000 | 32 | 100 | 4 | 16 | S:S | 3 | 1361048 | 1.807 |
| valkey | mixed | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 1277570 | 0.263 |
| valkey | mixed | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 527816 | 0.367 |
| valkey | mixed | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1285084 | 0.207 |
| valkey | reads | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1335629 | 0.183 |
| valkey | reads | 100000 | 32 | 100 | 2 | 16 | S:S | 3 | 1516613 | 1.695 |
| valkey | reads | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 607090 | 0.351 |
| valkey | reads | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 328597 | 0.703 |
| valkey | reads | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1431120 | 0.175 |
| valkey | writes | 100000 | 32 | 10 | 2 | 16 | R:R | 3 | 1088379 | 0.207 |
| valkey | writes | 100000 | 32 | 100 | 4 | 16 | S:S | 3 | 1284543 | 1.919 |
| valkey | writes | 100000 | 256 | 10 | 2 | 16 | S:S | 3 | 1163219 | 0.303 |
| valkey | writes | 100000 | 4096 | 10 | 2 | 16 | S:S | 3 | 311317 | 0.695 |
| valkey | writes | 1000000 | 32 | 10 | 2 | 16 | S:S | 3 | 1171626 | 0.215 |

<details>
<summary>Every tested operating point</summary>

| Case | Server | Workload | Trials | Mean ops/s | Worst p99 ms | Server CPU % | Client CPU/worker % | Client CPU/budget % | Meets p99 limit |
|---|---|---|---:|---:|---:|---:|---:|---:|---|
| c1-t1-p1-v32-k100000-S | reddish | mixed | 3 | 39311 | 0.039 | 46.20 | 55.47 | 27.73 | true |
| c1-t1-p1-v32-k100000-S | reddish | reads | 3 | 39937 | 0.039 | 46.20 | 55.07 | 27.54 | true |
| c1-t1-p1-v32-k100000-S | reddish | writes | 3 | 38937 | 0.039 | 46.40 | 55.20 | 27.60 | true |
| c1-t1-p1-v32-k100000-S | redis | mixed | 3 | 37395 | 0.039 | 48.87 | 53.47 | 26.73 | true |
| c1-t1-p1-v32-k100000-S | redis | reads | 3 | 37150 | 0.039 | 48.67 | 53.33 | 26.66 | true |
| c1-t1-p1-v32-k100000-S | redis | writes | 3 | 37018 | 0.039 | 49.07 | 53.00 | 26.50 | true |
| c1-t1-p1-v32-k100000-S | valkey | mixed | 3 | 37415 | 0.039 | 48.33 | 53.67 | 26.84 | true |
| c1-t1-p1-v32-k100000-S | valkey | reads | 3 | 37877 | 0.039 | 48.53 | 53.80 | 26.90 | true |
| c1-t1-p1-v32-k100000-S | valkey | writes | 3 | 37324 | 0.039 | 48.53 | 53.27 | 26.64 | true |
| c10-t1-p1-v32-k100000-S | reddish | mixed | 3 | 107624 | 0.143 | 82.33 | 99.80 | 49.90 | true |
| c10-t1-p1-v32-k100000-S | reddish | reads | 3 | 107962 | 0.175 | 83.66 | 99.85 | 49.92 | true |
| c10-t1-p1-v32-k100000-S | reddish | writes | 3 | 107271 | 0.151 | 82.39 | 99.85 | 49.92 | true |
| c10-t1-p1-v32-k100000-S | redis | mixed | 3 | 109918 | 0.135 | 92.06 | 99.86 | 49.93 | true |
| c10-t1-p1-v32-k100000-S | redis | reads | 3 | 110628 | 0.135 | 92.12 | 99.85 | 49.92 | true |
| c10-t1-p1-v32-k100000-S | redis | writes | 3 | 109330 | 0.143 | 91.73 | 99.86 | 49.93 | true |
| c10-t1-p1-v32-k100000-S | valkey | mixed | 3 | 109048 | 0.175 | 89.53 | 99.79 | 49.90 | true |
| c10-t1-p1-v32-k100000-S | valkey | reads | 3 | 109277 | 0.135 | 90.39 | 99.66 | 49.83 | true |
| c10-t1-p1-v32-k100000-S | valkey | writes | 3 | 109789 | 0.135 | 90.79 | 99.92 | 49.96 | true |
| c10-t1-p16-v32-k100000-S | reddish | mixed | 3 | 1130288 | 0.215 | 65.40 | 99.93 | 49.97 | true |
| c10-t1-p16-v32-k100000-S | reddish | reads | 3 | 1174346 | 0.207 | 68.26 | 99.92 | 49.96 | true |
| c10-t1-p16-v32-k100000-S | reddish | writes | 3 | 1099104 | 0.231 | 64.66 | 99.85 | 49.92 | true |
| c10-t1-p16-v32-k100000-S | redis | mixed | 3 | 1183219 | 0.191 | 98.52 | 99.79 | 49.90 | true |
| c10-t1-p16-v32-k100000-S | redis | reads | 3 | 1212371 | 0.191 | 92.39 | 99.93 | 49.97 | true |
| c10-t1-p16-v32-k100000-S | redis | writes | 3 | 1154353 | 0.199 | 98.05 | 99.79 | 49.90 | true |
| c10-t1-p16-v32-k100000-S | valkey | mixed | 3 | 1205241 | 0.191 | 97.47 | 99.73 | 49.87 | true |
| c10-t1-p16-v32-k100000-S | valkey | reads | 3 | 1203543 | 0.191 | 94.72 | 99.85 | 49.92 | true |
| c10-t1-p16-v32-k100000-S | valkey | writes | 3 | 1170566 | 0.199 | 98.65 | 99.59 | 49.80 | true |
| c10-t2-p16-v32-k100000-S | reddish | mixed | 3 | 1712098 | 0.143 | 84.13 | 99.33 | 99.33 | true |
| c10-t2-p16-v32-k100000-S | reddish | reads | 3 | 1793634 | 0.127 | 87.25 | 99.51 | 99.52 | true |
| c10-t2-p16-v32-k100000-S | reddish | writes | 3 | 1660844 | 0.143 | 82.66 | 99.69 | 99.69 | true |
| c10-t2-p16-v32-k100000-S | redis | mixed | 3 | 1259428 | 0.183 | 99.99 | 76.19 | 76.19 | true |
| c10-t2-p16-v32-k100000-S | redis | reads | 3 | 1463373 | 0.167 | 99.86 | 84.73 | 84.73 | true |
| c10-t2-p16-v32-k100000-S | redis | writes | 3 | 1233866 | 0.183 | 99.87 | 77.13 | 77.14 | true |
| c10-t2-p16-v32-k100000-S | valkey | mixed | 3 | 1292038 | 0.199 | 100.07 | 78.17 | 78.17 | true |
| c10-t2-p16-v32-k100000-S | valkey | reads | 3 | 1453304 | 0.159 | 99.86 | 83.76 | 83.76 | true |
| c10-t2-p16-v32-k100000-S | valkey | writes | 3 | 1205626 | 0.191 | 100.00 | 75.17 | 75.17 | true |
| c100-t2-p16-v32-k100000-S | reddish | mixed | 3 | 1726252 | 1.407 | 80.82 | 100.05 | 100.05 | true |
| c100-t2-p16-v32-k100000-S | reddish | reads | 3 | 1795276 | 1.359 | 85.80 | 99.96 | 99.96 | true |
| c100-t2-p16-v32-k100000-S | reddish | writes | 3 | 1668066 | 1.439 | 78.55 | 100.13 | 100.14 | true |
| c100-t2-p16-v32-k100000-S | redis | mixed | 3 | 1312838 | 1.735 | 100.03 | 74.57 | 74.57 | true |
| c100-t2-p16-v32-k100000-S | redis | reads | 3 | 1513035 | 1.711 | 99.80 | 82.71 | 82.71 | true |
| c100-t2-p16-v32-k100000-S | redis | writes | 3 | 1293513 | 2.063 | 99.67 | 76.49 | 76.48 | false |
| c100-t2-p16-v32-k100000-S | valkey | mixed | 3 | 1331476 | 2.319 | 99.89 | 77.26 | 77.27 | false |
| c100-t2-p16-v32-k100000-S | valkey | reads | 3 | 1516613 | 1.695 | 99.66 | 83.07 | 83.07 | true |
| c100-t2-p16-v32-k100000-S | valkey | writes | 3 | 1258102 | 1.983 | 99.87 | 74.96 | 74.95 | true |
| c100-t4-p16-v32-k100000-S | reddish | mixed | 3 | 1713967 | 3.711 | 81.82 | 49.99 | 99.98 | false |
| c100-t4-p16-v32-k100000-S | reddish | reads | 3 | 1758296 | 3.679 | 86.36 | 49.86 | 99.72 | false |
| c100-t4-p16-v32-k100000-S | reddish | writes | 3 | 1654743 | 3.743 | 80.61 | 50.05 | 100.09 | false |
| c100-t4-p16-v32-k100000-S | redis | mixed | 3 | 1291851 | 1.935 | 99.83 | 37.25 | 74.51 | true |
| c100-t4-p16-v32-k100000-S | redis | reads | 3 | 1513779 | 1.695 | 99.79 | 41.63 | 83.27 | true |
| c100-t4-p16-v32-k100000-S | redis | writes | 3 | 1279186 | 1.935 | 99.94 | 38.04 | 76.09 | true |
| c100-t4-p16-v32-k100000-S | valkey | mixed | 3 | 1361048 | 1.807 | 99.96 | 39.00 | 78.00 | true |
| c100-t4-p16-v32-k100000-S | valkey | reads | 3 | 1496956 | 2.015 | 99.75 | 41.13 | 82.25 | false |
| c100-t4-p16-v32-k100000-S | valkey | writes | 3 | 1284543 | 1.919 | 99.86 | 37.82 | 75.64 | true |
| c100-t2-p64-v32-k100000-S | reddish | mixed | 3 | 2920100 | 3.343 | 45.77 | 100.13 | 100.14 | false |
| c100-t2-p64-v32-k100000-S | reddish | reads | 3 | 3139235 | 3.119 | 50.29 | 100.12 | 100.12 | false |
| c100-t2-p64-v32-k100000-S | reddish | writes | 3 | 2735370 | 3.615 | 46.03 | 100.19 | 100.19 | false |
| c100-t2-p64-v32-k100000-S | redis | mixed | 3 | 2126318 | 4.639 | 99.99 | 74.11 | 74.11 | false |
| c100-t2-p64-v32-k100000-S | redis | reads | 3 | 2740182 | 3.855 | 99.65 | 89.20 | 89.19 | false |
| c100-t2-p64-v32-k100000-S | redis | writes | 3 | 2112671 | 4.351 | 99.79 | 75.67 | 75.67 | false |
| c100-t2-p64-v32-k100000-S | valkey | mixed | 3 | 2287408 | 4.319 | 99.94 | 79.22 | 79.22 | false |
| c100-t2-p64-v32-k100000-S | valkey | reads | 3 | 2709587 | 3.823 | 99.79 | 88.13 | 88.13 | false |
| c100-t2-p64-v32-k100000-S | valkey | writes | 3 | 2048701 | 4.543 | 99.79 | 74.61 | 74.61 | false |
| c10-t2-p16-v256-k100000-S | reddish | mixed | 3 | 1684738 | 0.135 | 87.79 | 99.89 | 99.89 | true |
| c10-t2-p16-v256-k100000-S | reddish | reads | 3 | 708714 | 0.503 | 99.99 | 46.96 | 46.97 | true |
| c10-t2-p16-v256-k100000-S | reddish | writes | 3 | 1590814 | 0.151 | 86.72 | 99.69 | 99.69 | true |
| c10-t2-p16-v256-k100000-S | redis | mixed | 3 | 1266703 | 0.183 | 100.00 | 79.87 | 79.86 | true |
| c10-t2-p16-v256-k100000-S | redis | reads | 3 | 610677 | 0.367 | 99.92 | 40.23 | 40.23 | true |
| c10-t2-p16-v256-k100000-S | redis | writes | 3 | 1294187 | 0.199 | 99.93 | 84.53 | 84.53 | true |
| c10-t2-p16-v256-k100000-S | valkey | mixed | 3 | 1277570 | 0.263 | 99.93 | 78.60 | 78.60 | true |
| c10-t2-p16-v256-k100000-S | valkey | reads | 3 | 607090 | 0.351 | 100.07 | 41.63 | 41.63 | true |
| c10-t2-p16-v256-k100000-S | valkey | writes | 3 | 1163219 | 0.303 | 99.86 | 75.36 | 75.36 | true |
| c10-t2-p16-v4096-k100000-S | reddish | mixed | 3 | 554429 | 0.399 | 100.07 | 99.70 | 99.70 | true |
| c10-t2-p16-v4096-k100000-S | reddish | reads | 3 | 331946 | 0.743 | 99.99 | 99.62 | 99.62 | true |
| c10-t2-p16-v4096-k100000-S | reddish | writes | 3 | 649676 | 0.367 | 99.93 | 69.26 | 69.26 | true |
| c10-t2-p16-v4096-k100000-S | redis | mixed | 3 | 527393 | 0.479 | 99.93 | 98.69 | 98.69 | true |
| c10-t2-p16-v4096-k100000-S | redis | reads | 3 | 329260 | 0.759 | 99.93 | 99.67 | 99.67 | true |
| c10-t2-p16-v4096-k100000-S | redis | writes | 3 | 319532 | 0.583 | 99.99 | 51.86 | 51.87 | true |
| c10-t2-p16-v4096-k100000-S | valkey | mixed | 3 | 527816 | 0.367 | 100.07 | 99.33 | 99.33 | true |
| c10-t2-p16-v4096-k100000-S | valkey | reads | 3 | 328597 | 0.703 | 99.93 | 99.43 | 99.44 | true |
| c10-t2-p16-v4096-k100000-S | valkey | writes | 3 | 311317 | 0.695 | 100.07 | 52.53 | 52.53 | true |
| c10-t2-p16-v32-k1000000-S | reddish | mixed | 3 | 1728019 | 0.159 | 87.80 | 99.50 | 99.50 | true |
| c10-t2-p16-v32-k1000000-S | reddish | reads | 3 | 1796137 | 0.175 | 90.46 | 99.26 | 99.26 | true |
| c10-t2-p16-v32-k1000000-S | reddish | writes | 3 | 1647838 | 0.151 | 86.33 | 99.30 | 99.30 | true |
| c10-t2-p16-v32-k1000000-S | redis | mixed | 3 | 1251075 | 0.255 | 100.07 | 74.80 | 74.80 | true |
| c10-t2-p16-v32-k1000000-S | redis | reads | 3 | 1440654 | 0.167 | 99.85 | 83.62 | 83.62 | true |
| c10-t2-p16-v32-k1000000-S | redis | writes | 3 | 1222776 | 0.191 | 99.85 | 75.86 | 75.86 | true |
| c10-t2-p16-v32-k1000000-S | valkey | mixed | 3 | 1285084 | 0.207 | 99.93 | 77.83 | 77.83 | true |
| c10-t2-p16-v32-k1000000-S | valkey | reads | 3 | 1431120 | 0.175 | 99.80 | 83.17 | 83.17 | true |
| c10-t2-p16-v32-k1000000-S | valkey | writes | 3 | 1171626 | 0.215 | 100.06 | 73.50 | 73.50 | true |
| c10-t2-p16-v32-k100000-R | reddish | mixed | 3 | 1744400 | 0.135 | 86.79 | 99.76 | 99.76 | true |
| c10-t2-p16-v32-k100000-R | reddish | reads | 3 | 1816239 | 0.135 | 89.40 | 99.53 | 99.53 | true |
| c10-t2-p16-v32-k100000-R | reddish | writes | 3 | 1663158 | 0.159 | 85.52 | 99.55 | 99.55 | true |
| c10-t2-p16-v32-k100000-R | redis | mixed | 3 | 1237668 | 0.191 | 99.79 | 75.09 | 75.09 | true |
| c10-t2-p16-v32-k100000-R | redis | reads | 3 | 1445736 | 0.175 | 99.86 | 84.09 | 84.09 | true |
| c10-t2-p16-v32-k100000-R | redis | writes | 3 | 1198088 | 0.199 | 100.06 | 75.59 | 75.59 | true |
| c10-t2-p16-v32-k100000-R | valkey | mixed | 3 | 1139285 | 0.207 | 100.00 | 68.83 | 68.83 | true |
| c10-t2-p16-v32-k100000-R | valkey | reads | 3 | 1335629 | 0.183 | 99.85 | 78.16 | 78.16 | true |
| c10-t2-p16-v32-k100000-R | valkey | writes | 3 | 1088379 | 0.207 | 99.92 | 67.22 | 67.22 | true |

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
| expiry | pass | 3 | 3520 | 5636 | 0 | 10000 | 0 |
| eviction | pass | 4 | 1794 | 5200 | 10000 | 0 | 10000 |
| large_list | pass | 6 | 431 | 4648 | 0 | 0 | 0 |
| promoted_hash | pass | 8 | 483 | 4920 | 0 | 0 | 0 |
| churn | pass | 19 | 3794 | 20908 | 0 | 0 | 0 |
| backpressure | pass | 3 | 275 | 6124 | 1 | 0 | 0 |
| durable_recovery | pass | 10 | 1327 | 4444 | 2 | 1 | 0 |

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
| expire_ttl | reddish | 1 | 1162973 | 0.136 | 0.207 |
| expire_ttl | reddish | 2 | 1103346 | 0.143 | 0.215 |
| expire_ttl | reddish | 3 | 1128358 | 0.140 | 0.207 |
| expire_ttl | reddish | 4 | 1174742 | 0.135 | 0.207 |
| expire_ttl | reddish | 5 | 1178732 | 0.134 | 0.199 |
| expire_ttl | redis | 1 | 1146344 | 0.138 | 0.199 |
| expire_ttl | redis | 2 | 1151101 | 0.137 | 0.199 |
| expire_ttl | redis | 3 | 1202591 | 0.131 | 0.199 |
| expire_ttl | redis | 4 | 1228688 | 0.129 | 0.183 |
| expire_ttl | redis | 5 | 1225053 | 0.129 | 0.183 |
| expire_ttl | valkey | 1 | 1146633 | 0.138 | 0.223 |
| expire_ttl | valkey | 2 | 1164159 | 0.136 | 0.279 |
| expire_ttl | valkey | 3 | 1168350 | 0.135 | 0.319 |
| expire_ttl | valkey | 4 | 1181144 | 0.134 | 0.335 |
| expire_ttl | valkey | 5 | 1211318 | 0.131 | 0.191 |
| hget | reddish | 1 | 1120142 | 0.141 | 0.215 |
| hget | reddish | 2 | 1096224 | 0.144 | 0.215 |
| hget | reddish | 3 | 1105405 | 0.143 | 0.215 |
| hget | reddish | 4 | 1157952 | 0.136 | 0.207 |
| hget | reddish | 5 | 1132241 | 0.139 | 0.247 |
| hget | redis | 1 | 1134960 | 0.139 | 0.199 |
| hget | redis | 2 | 1109972 | 0.142 | 0.207 |
| hget | redis | 3 | 1184160 | 0.133 | 0.199 |
| hget | redis | 4 | 1186399 | 0.133 | 0.199 |
| hget | redis | 5 | 1158999 | 0.136 | 0.199 |
| hget | valkey | 1 | 1151611 | 0.137 | 0.199 |
| hget | valkey | 2 | 1185875 | 0.133 | 0.199 |
| hget | valkey | 3 | 1114103 | 0.141 | 0.207 |
| hget | valkey | 4 | 1178562 | 0.133 | 0.199 |
| hget | valkey | 5 | 1143289 | 0.138 | 0.207 |
| hlen | reddish | 1 | 1216746 | 0.130 | 0.199 |
| hlen | reddish | 2 | 1174647 | 0.135 | 0.199 |
| hlen | reddish | 3 | 1172787 | 0.135 | 0.207 |
| hlen | reddish | 4 | 1232387 | 0.128 | 0.191 |
| hlen | reddish | 5 | 1203955 | 0.131 | 0.215 |
| hlen | redis | 1 | 1201213 | 0.132 | 0.191 |
| hlen | redis | 2 | 1176687 | 0.134 | 0.199 |
| hlen | redis | 3 | 1268312 | 0.125 | 0.183 |
| hlen | redis | 4 | 1230857 | 0.128 | 0.295 |
| hlen | redis | 5 | 1246241 | 0.127 | 0.191 |
| hlen | valkey | 1 | 1209907 | 0.131 | 0.191 |
| hlen | valkey | 2 | 1255097 | 0.126 | 0.191 |
| hlen | valkey | 3 | 1217887 | 0.130 | 0.191 |
| hlen | valkey | 4 | 1242484 | 0.127 | 0.263 |
| hlen | valkey | 5 | 1286925 | 0.123 | 0.183 |
| hset_hdel | reddish | 1 | 1121295 | 0.141 | 0.215 |
| hset_hdel | reddish | 2 | 1079532 | 0.147 | 0.215 |
| hset_hdel | reddish | 3 | 1077878 | 0.147 | 0.223 |
| hset_hdel | reddish | 4 | 1133764 | 0.140 | 0.215 |
| hset_hdel | reddish | 5 | 1109278 | 0.143 | 0.247 |
| hset_hdel | redis | 1 | 1104630 | 0.143 | 0.215 |
| hset_hdel | redis | 2 | 1098291 | 0.144 | 0.207 |
| hset_hdel | redis | 3 | 1165418 | 0.136 | 0.191 |
| hset_hdel | redis | 4 | 1182846 | 0.134 | 0.191 |
| hset_hdel | redis | 5 | 1172991 | 0.135 | 0.191 |
| hset_hdel | valkey | 1 | 1111237 | 0.142 | 0.207 |
| hset_hdel | valkey | 2 | 1131926 | 0.140 | 0.247 |
| hset_hdel | valkey | 3 | 1127130 | 0.140 | 0.255 |
| hset_hdel | valkey | 4 | 1145292 | 0.138 | 0.223 |
| hset_hdel | valkey | 5 | 1152193 | 0.137 | 0.199 |
| incr | reddish | 1 | 1189551 | 0.133 | 0.199 |
| incr | reddish | 2 | 1194913 | 0.132 | 0.199 |
| incr | reddish | 3 | 1166921 | 0.135 | 0.199 |
| incr | reddish | 4 | 1234650 | 0.128 | 0.191 |
| incr | reddish | 5 | 1246630 | 0.127 | 0.191 |
| incr | redis | 1 | 1246997 | 0.127 | 0.191 |
| incr | redis | 2 | 1201972 | 0.131 | 0.191 |
| incr | redis | 3 | 1263143 | 0.125 | 0.191 |
| incr | redis | 4 | 1286986 | 0.123 | 0.175 |
| incr | redis | 5 | 1239735 | 0.127 | 0.191 |
| incr | valkey | 1 | 1210116 | 0.131 | 0.191 |
| incr | valkey | 2 | 1262476 | 0.125 | 0.183 |
| incr | valkey | 3 | 1191560 | 0.133 | 0.199 |
| incr | valkey | 4 | 1283497 | 0.123 | 0.183 |
| incr | valkey | 5 | 1221014 | 0.129 | 0.191 |
| llen | reddish | 1 | 1189079 | 0.133 | 0.223 |
| llen | reddish | 2 | 1176792 | 0.134 | 0.199 |
| llen | reddish | 3 | 1192752 | 0.133 | 0.199 |
| llen | reddish | 4 | 1236513 | 0.128 | 0.191 |
| llen | reddish | 5 | 1192402 | 0.133 | 0.199 |
| llen | redis | 1 | 1191233 | 0.133 | 0.223 |
| llen | redis | 2 | 1182090 | 0.134 | 0.199 |
| llen | redis | 3 | 1246872 | 0.127 | 0.191 |
| llen | redis | 4 | 1291654 | 0.122 | 0.175 |
| llen | redis | 5 | 1221898 | 0.129 | 0.191 |
| llen | valkey | 1 | 1226775 | 0.129 | 0.191 |
| llen | valkey | 2 | 1264680 | 0.125 | 0.183 |
| llen | valkey | 3 | 1199648 | 0.132 | 0.191 |
| llen | valkey | 4 | 1285049 | 0.123 | 0.183 |
| llen | valkey | 5 | 1266477 | 0.125 | 0.183 |
| lpush_lpop | reddish | 1 | 1109252 | 0.142 | 0.215 |
| lpush_lpop | reddish | 2 | 1102713 | 0.143 | 0.215 |
| lpush_lpop | reddish | 3 | 1082939 | 0.146 | 0.215 |
| lpush_lpop | reddish | 4 | 1117365 | 0.141 | 0.223 |
| lpush_lpop | reddish | 5 | 1084717 | 0.146 | 0.215 |
| lpush_lpop | redis | 1 | 1146664 | 0.138 | 0.191 |
| lpush_lpop | redis | 2 | 1097182 | 0.144 | 0.199 |
| lpush_lpop | redis | 3 | 1134278 | 0.139 | 0.215 |
| lpush_lpop | redis | 4 | 1189189 | 0.133 | 0.183 |
| lpush_lpop | redis | 5 | 1178325 | 0.134 | 0.191 |
| lpush_lpop | valkey | 1 | 1120332 | 0.141 | 0.191 |
| lpush_lpop | valkey | 2 | 1182345 | 0.133 | 0.183 |
| lpush_lpop | valkey | 3 | 1117464 | 0.141 | 0.207 |
| lpush_lpop | valkey | 4 | 1197951 | 0.132 | 0.183 |
| lpush_lpop | valkey | 5 | 1183429 | 0.133 | 0.183 |
| mixed | reddish | 1 | 1158909 | 0.136 | 0.207 |
| mixed | reddish | 2 | 1181242 | 0.134 | 0.207 |
| mixed | reddish | 3 | 1142022 | 0.138 | 0.207 |
| mixed | reddish | 4 | 1209823 | 0.131 | 0.199 |
| mixed | reddish | 5 | 1159606 | 0.136 | 0.207 |
| mixed | redis | 1 | 1243741 | 0.127 | 0.183 |
| mixed | redis | 2 | 1181200 | 0.134 | 0.191 |
| mixed | redis | 3 | 1245386 | 0.127 | 0.183 |
| mixed | redis | 4 | 1262985 | 0.125 | 0.175 |
| mixed | redis | 5 | 1246842 | 0.127 | 0.183 |
| mixed | valkey | 1 | 1180592 | 0.134 | 0.199 |
| mixed | valkey | 2 | 1230557 | 0.128 | 0.191 |
| mixed | valkey | 3 | 1183217 | 0.133 | 0.191 |
| mixed | valkey | 4 | 1265541 | 0.125 | 0.175 |
| mixed | valkey | 5 | 1212291 | 0.130 | 0.191 |
| ping | reddish | 1 | 1429150 | 0.110 | 0.167 |
| ping | reddish | 2 | 1479366 | 0.107 | 0.167 |
| ping | reddish | 3 | 1445938 | 0.109 | 0.167 |
| ping | reddish | 4 | 1506888 | 0.105 | 0.159 |
| ping | reddish | 5 | 1450199 | 0.109 | 0.167 |
| ping | redis | 1 | 1540162 | 0.102 | 0.151 |
| ping | redis | 2 | 1459492 | 0.108 | 0.159 |
| ping | redis | 3 | 1568989 | 0.100 | 0.143 |
| ping | redis | 4 | 1519686 | 0.104 | 0.231 |
| ping | redis | 5 | 1563353 | 0.101 | 0.151 |
| ping | valkey | 1 | 1458794 | 0.108 | 0.159 |
| ping | valkey | 2 | 1539517 | 0.102 | 0.151 |
| ping | valkey | 3 | 1446265 | 0.109 | 0.159 |
| ping | valkey | 4 | 1553951 | 0.101 | 0.151 |
| ping | valkey | 5 | 1550562 | 0.102 | 0.151 |
| reads | memcached | 1 | 1045371 | 0.152 | 0.559 |
| reads | memcached | 2 | 1361425 | 0.116 | 0.199 |
| reads | memcached | 3 | 1348028 | 0.117 | 0.359 |
| reads | memcached | 4 | 1371199 | 0.115 | 0.279 |
| reads | memcached | 5 | 1371523 | 0.115 | 0.167 |
| reads | reddish | 1 | 1188725 | 0.133 | 0.199 |
| reads | reddish | 2 | 1194503 | 0.132 | 0.199 |
| reads | reddish | 3 | 1192901 | 0.132 | 0.199 |
| reads | reddish | 4 | 1236225 | 0.128 | 0.191 |
| reads | reddish | 5 | 1241256 | 0.127 | 0.191 |
| reads | redis | 1 | 1244476 | 0.127 | 0.191 |
| reads | redis | 2 | 1228005 | 0.128 | 0.191 |
| reads | redis | 3 | 1238007 | 0.127 | 0.183 |
| reads | redis | 4 | 1274224 | 0.124 | 0.191 |
| reads | redis | 5 | 1302413 | 0.121 | 0.175 |
| reads | valkey | 1 | 1227815 | 0.128 | 0.191 |
| reads | valkey | 2 | 1221388 | 0.129 | 0.191 |
| reads | valkey | 3 | 1221468 | 0.129 | 0.191 |
| reads | valkey | 4 | 1292687 | 0.122 | 0.183 |
| reads | valkey | 5 | 1290557 | 0.122 | 0.183 |
| rpush_rpop | reddish | 1 | 1094401 | 0.144 | 0.231 |
| rpush_rpop | reddish | 2 | 1081916 | 0.146 | 0.223 |
| rpush_rpop | reddish | 3 | 1078563 | 0.146 | 0.215 |
| rpush_rpop | reddish | 4 | 1135318 | 0.139 | 0.207 |
| rpush_rpop | reddish | 5 | 1072065 | 0.147 | 0.247 |
| rpush_rpop | redis | 1 | 1131262 | 0.140 | 0.191 |
| rpush_rpop | redis | 2 | 1117615 | 0.141 | 0.199 |
| rpush_rpop | redis | 3 | 1161273 | 0.136 | 0.191 |
| rpush_rpop | redis | 4 | 1210167 | 0.130 | 0.183 |
| rpush_rpop | redis | 5 | 1152338 | 0.137 | 0.199 |
| rpush_rpop | valkey | 1 | 1123769 | 0.140 | 0.191 |
| rpush_rpop | valkey | 2 | 1175330 | 0.134 | 0.191 |
| rpush_rpop | valkey | 3 | 1129140 | 0.140 | 0.199 |
| rpush_rpop | valkey | 4 | 1208018 | 0.131 | 0.183 |
| rpush_rpop | valkey | 5 | 1183886 | 0.133 | 0.183 |
| set_del | reddish | 1 | 1124160 | 0.141 | 0.215 |
| set_del | reddish | 2 | 1120734 | 0.141 | 0.215 |
| set_del | reddish | 3 | 1096639 | 0.144 | 0.223 |
| set_del | reddish | 4 | 1160567 | 0.136 | 0.207 |
| set_del | reddish | 5 | 1159515 | 0.136 | 0.207 |
| set_del | redis | 1 | 1195141 | 0.132 | 0.191 |
| set_del | redis | 2 | 1117368 | 0.142 | 0.215 |
| set_del | redis | 3 | 1142251 | 0.138 | 0.271 |
| set_del | redis | 4 | 1197651 | 0.132 | 0.191 |
| set_del | redis | 5 | 1179927 | 0.134 | 0.231 |
| set_del | valkey | 1 | 1132303 | 0.140 | 0.199 |
| set_del | valkey | 2 | 1181731 | 0.134 | 0.223 |
| set_del | valkey | 3 | 1105178 | 0.143 | 0.231 |
| set_del | valkey | 4 | 1197742 | 0.132 | 0.191 |
| set_del | valkey | 5 | 1180613 | 0.134 | 0.199 |
| writes | memcached | 1 | 1028809 | 0.154 | 0.455 |
| writes | memcached | 2 | 1325502 | 0.120 | 0.207 |
| writes | memcached | 3 | 1177251 | 0.135 | 0.391 |
| writes | memcached | 4 | 1335352 | 0.119 | 0.303 |
| writes | memcached | 5 | 1323653 | 0.120 | 0.183 |
| writes | reddish | 1 | 1113683 | 0.142 | 0.215 |
| writes | reddish | 2 | 1140411 | 0.139 | 0.215 |
| writes | reddish | 3 | 1089749 | 0.145 | 0.215 |
| writes | reddish | 4 | 1161024 | 0.136 | 0.207 |
| writes | reddish | 5 | 1133024 | 0.140 | 0.215 |
| writes | redis | 1 | 1204993 | 0.131 | 0.191 |
| writes | redis | 2 | 1150642 | 0.137 | 0.199 |
| writes | redis | 3 | 1210377 | 0.131 | 0.191 |
| writes | redis | 4 | 1223287 | 0.129 | 0.183 |
| writes | redis | 5 | 1217753 | 0.130 | 0.183 |
| writes | valkey | 1 | 1118729 | 0.141 | 0.215 |
| writes | valkey | 2 | 1107974 | 0.143 | 0.223 |
| writes | valkey | 3 | 1133282 | 0.140 | 0.215 |
| writes | valkey | 4 | 1230342 | 0.129 | 0.183 |
| writes | valkey | 5 | 1223001 | 0.129 | 0.183 |

</details>

## Individual memory trials

<details>
<summary>All measured samples</summary>

| Shape | Server | Trial | Keys | Idle RSS KiB | Loaded RSS KiB | Delta bytes/key |
|---|---|---:|---:|---:|---:|---:|
| hashes | reddish | 1 | 100000 | 4096 | 14732 | 108.91 |
| hashes | reddish | 2 | 100000 | 4100 | 14736 | 108.91 |
| hashes | reddish | 3 | 100000 | 4012 | 14648 | 108.91 |
| hashes | redis | 1 | 100000 | 8736 | 19528 | 110.51 |
| hashes | redis | 2 | 100000 | 8716 | 19508 | 110.51 |
| hashes | redis | 3 | 100000 | 8784 | 19448 | 109.20 |
| hashes | valkey | 1 | 100000 | 7392 | 17588 | 104.41 |
| hashes | valkey | 2 | 100000 | 7532 | 17596 | 103.06 |
| hashes | valkey | 3 | 100000 | 7440 | 17572 | 103.75 |
| lists | reddish | 1 | 100000 | 4096 | 15436 | 116.12 |
| lists | reddish | 2 | 100000 | 4096 | 15436 | 116.12 |
| lists | reddish | 3 | 100000 | 4088 | 15428 | 116.12 |
| lists | redis | 1 | 100000 | 8684 | 19404 | 109.77 |
| lists | redis | 2 | 100000 | 8688 | 19484 | 110.55 |
| lists | redis | 3 | 100000 | 8692 | 19416 | 109.81 |
| lists | valkey | 1 | 100000 | 7556 | 17632 | 103.18 |
| lists | valkey | 2 | 100000 | 7416 | 17544 | 103.71 |
| lists | valkey | 3 | 100000 | 7532 | 17592 | 103.01 |
| strings | memcached | 1 | 100000 | 4724 | 18112 | 137.09 |
| strings | memcached | 2 | 100000 | 4700 | 18088 | 137.09 |
| strings | memcached | 3 | 100000 | 4696 | 18084 | 137.09 |
| strings | reddish | 1 | 100000 | 4092 | 13064 | 91.87 |
| strings | reddish | 2 | 100000 | 4072 | 13068 | 92.12 |
| strings | reddish | 3 | 100000 | 4060 | 13032 | 91.87 |
| strings | redis | 1 | 100000 | 8768 | 17960 | 94.13 |
| strings | redis | 2 | 100000 | 8692 | 17416 | 89.33 |
| strings | redis | 3 | 100000 | 8752 | 17952 | 94.21 |
| strings | valkey | 1 | 100000 | 7608 | 16156 | 87.53 |
| strings | valkey | 2 | 100000 | 7528 | 16076 | 87.53 |
| strings | valkey | 3 | 100000 | 7544 | 16088 | 87.49 |

</details>

## Measurement manifest

<details>
<summary>Revision, executable hash, hardware, tools, and settings</summary>

```text
da330d0debfc2e3e7f965da3f0bcdd35e0fc7fc4
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  -
a41b76a5a2af503eb6bce2aa40340add712a1165507ff987112ab436398efd83  /home/runner/work/reddish/reddish/build/output/reddish
Measured at 2026-10-31 13:17:53 UTC
WORKTREE_STATE=clean
Linux runnervm48cm3 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
Architecture:                            x86_64
CPU op-mode(s):                          32-bit, 64-bit
Address sizes:                           48 bits physical, 48 bits virtual
Byte Order:                              Little Endian
CPU(s):                                  4
On-line CPU(s) list:                     0-3
Vendor ID:                               AuthenticAMD
Model name:                              AMD EPYC 9V45 96-Core Processor
CPU family:                              26
Model:                                   2
Thread(s) per core:                      2
Core(s) per socket:                      2
Socket(s):                               1
Stepping:                                1
BogoMIPS:                                5192.19
Flags:                                   fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl xtopology tsc_reliable nonstop_tsc cpuid extd_apicid aperfmperf tsc_known_freq pni pclmulqdq ssse3 fma cx16 pcid sse4_1 sse4_2 x2apic movbe popcnt aes xsave avx f16c rdrand hypervisor lahf_lm cmp_legacy svm cr8_legacy abm sse4a misalignsse 3dnowprefetch osvw topoext vmmcall fsgsbase bmi1 avx2 smep bmi2 erms invpcid avx512f avx512dq rdseed adx smap avx512ifma clflushopt clwb avx512cd sha_ni avx512bw avx512vl xsaveopt xsavec xgetbv1 xsaves user_shstk avx_vnni avx512_bf16 clzero xsaveerptr rdpru arat npt nrip_save tsc_scale vmcb_clean flushbyasid decodeassists pausefilter pfthreshold v_vmsave_vmload avx512vbmi umip avx512_vbmi2 gfni vaes vpclmulqdq avx512_vnni avx512_bitalg avx512_vpopcntdq rdpid fsrm avx512_vp2intersect
Virtualization:                          AMD-V
Hypervisor vendor:                       Microsoft
Virtualization type:                     full
L1d cache:                               96 KiB (2 instances)
L1i cache:                               64 KiB (2 instances)
L2 cache:                                2 MiB (2 instances)
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
Vulnerability Tsa:                       Not affected
Vulnerability Tsx async abort:           Not affected
Vulnerability Vmscape:                   Not affected
memtier_benchmark v=2.5.1 sha=5f634d17:0 bits=64 libevent=2.1.12-stable openssl=OpenSSL 3.0.13 30 Jan 2024 prometheus=yes
Copyright (C) 2011-2026 Redis Ltd.
This is free software.  You may redistribute copies of it under the terms of
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.
There is NO WARRANTY, to the extent permitted by law.
Redis server v=8.10.1 sha=3399357e:0 malloc=jemalloc-5.3.0 bits=64 build=169b86a7ab614613
Valkey server v=8.1.1 sha=fcd8bc3e:0 malloc=jemalloc-5.3.0 bits=64 build=1eb268b486ff9107
memcached 1.6.24
SCHEDSTATS_MODE=1
SERVER_CPU=0 CLIENT_CPUS=2,3
REQUESTS_PER_CLIENT=0 KEY_PATTERN=S:S
RUN_PERFORMANCE=1 RUN_MEMORY=1 RUN_VALIDATION=0
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=5 DURATION_SECONDS=30 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=5 persistence=disabled
```

</details>
