# Benchmark results

Measured revision: [37fa2e4](https://github.com/amitverma-cf/reddish/commit/37fa2e4e55fc632abae4e4117eb26f60d712f5fd).

Environment: **GitHub Actions / Ubuntu 24.04**. Measured: **2026-10-31 06:58:23 UTC**.

[Source workflow run](https://github.com/amitverma-cf/reddish/actions/runs/41178440561). Raw data and profiles are attached to that run.

These are results for this machine and workload. They do not establish a general ranking or maximum server capacity.

Loopback TCP on the machine below. Persistence is disabled for the server comparison. Each server receives the same preload, warmup, values, clients, and pipeline. Server order rotates between trials; GET and SET order alternates.

## Setup

```text
Measured at 2026-10-31 06:58:23 UTC
Linux runnervm48cm3 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
CPU(s):                                  4
Model name:                              AMD EPYC 9V74 80-Core Processor
memtier_benchmark v=2.5.1 sha=5f634d17:0 bits=64 libevent=2.1.12-stable openssl=OpenSSL 3.0.13 30 Jan 2024 prometheus=yes
Redis server v=8.10.1 sha=3399357e:0 malloc=jemalloc-5.3.0 bits=64 build=ba14b7aebb207b99
Valkey server v=8.1.1 sha=fcd8bc3e:0 malloc=jemalloc-5.3.0 bits=64 build=6671c1fb0c94de5b
memcached 1.6.24
SERVER_CPU=unbound CLIENT_CPUS=unbound
REQUESTS_PER_CLIENT=0
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=5 DURATION_SECONDS=30 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=5 persistence=disabled
```

## Throughput

Operations per second. Range and standard deviation describe the trial means, not individual requests.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 5 | 972967 | 963185 | 987684 | 10158 |
| mixed | redis | 5 | 1044922 | 1027949 | 1086084 | 23382 |
| mixed | valkey | 5 | 1042838 | 1032448 | 1049271 | 6252 |
| reads | memcached | 5 | 1118063 | 1088528 | 1138528 | 19242 |
| reads | reddish | 5 | 1013326 | 997038 | 1024928 | 11675 |
| reads | redis | 5 | 1064699 | 1042578 | 1097135 | 19887 |
| reads | valkey | 5 | 1067436 | 1053065 | 1076798 | 9063 |
| writes | memcached | 5 | 1089096 | 989587 | 1156945 | 68210 |
| writes | reddish | 5 | 956837 | 954078 | 959831 | 2753 |
| writes | redis | 5 | 1023477 | 1013011 | 1044401 | 13007 |
| writes | valkey | 5 | 1008318 | 995977 | 1020643 | 9482 |

## Other commands

Each paired workload reports individual commands per second.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 5 | 969453 | 941909 | 987520 | 17556 |
| expire_ttl | redis | 5 | 1010132 | 1008534 | 1011151 | 1196 |
| expire_ttl | valkey | 5 | 996055 | 990055 | 1001197 | 4977 |
| hget | reddish | 5 | 935824 | 922535 | 946585 | 8973 |
| hget | redis | 5 | 967098 | 962225 | 975751 | 5159 |
| hget | valkey | 5 | 969358 | 967101 | 971550 | 1985 |
| hlen | reddish | 5 | 1002662 | 993267 | 1012802 | 7162 |
| hlen | redis | 5 | 1049893 | 1047517 | 1054195 | 2540 |
| hlen | valkey | 5 | 1050294 | 1045796 | 1054931 | 4289 |
| hset_hdel | reddish | 5 | 915235 | 903139 | 928176 | 10325 |
| hset_hdel | redis | 5 | 971152 | 956837 | 977998 | 8776 |
| hset_hdel | valkey | 5 | 956271 | 935297 | 966973 | 12658 |
| incr | reddish | 5 | 1006617 | 987679 | 1019070 | 14495 |
| incr | redis | 5 | 1045450 | 1038246 | 1050186 | 5446 |
| incr | valkey | 5 | 1052026 | 1040316 | 1058054 | 6788 |
| llen | reddish | 5 | 1005737 | 975597 | 1024564 | 18085 |
| llen | redis | 5 | 1041176 | 1019975 | 1050065 | 12458 |
| llen | valkey | 5 | 1047322 | 1042118 | 1056405 | 5418 |
| lpush_lpop | reddish | 5 | 912067 | 901307 | 928652 | 12666 |
| lpush_lpop | redis | 5 | 971182 | 968405 | 974068 | 2094 |
| lpush_lpop | valkey | 5 | 972943 | 965508 | 979689 | 5078 |
| ping | reddish | 5 | 1270912 | 1247361 | 1295331 | 17408 |
| ping | redis | 5 | 1303389 | 1292171 | 1312891 | 8816 |
| ping | valkey | 5 | 1319944 | 1310573 | 1325523 | 6802 |
| rpush_rpop | reddish | 5 | 901180 | 885418 | 916935 | 11801 |
| rpush_rpop | redis | 5 | 967192 | 965234 | 969843 | 2350 |
| rpush_rpop | valkey | 5 | 964290 | 933311 | 973408 | 17346 |
| set_del | reddish | 5 | 945555 | 940035 | 951879 | 5660 |
| set_del | redis | 5 | 981246 | 968632 | 989625 | 8850 |
| set_del | valkey | 5 | 995532 | 989992 | 1000926 | 4365 |

## Latency and CPU

Latency is in milliseconds. Percentiles are averages of each trial percentile; they are not pooled percentiles. CPU is server user plus system time per operation, excluding the load generator.

| Workload | Server | Mean latency ms | p50 ms | p99 ms | p99.9 ms | CPU µs/op |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 0.163 | 0.153 | 0.268 | 0.362 | 0.770 |
| expire_ttl | redis | 0.156 | 0.164 | 0.236 | 0.343 | 0.970 |
| expire_ttl | valkey | 0.159 | 0.159 | 0.340 | 0.602 | 0.988 |
| hget | reddish | 0.168 | 0.159 | 0.279 | 0.402 | 0.732 |
| hget | redis | 0.163 | 0.151 | 0.260 | 0.401 | 0.905 |
| hget | valkey | 0.162 | 0.151 | 0.250 | 0.369 | 0.935 |
| hlen | reddish | 0.158 | 0.151 | 0.281 | 0.385 | 0.659 |
| hlen | redis | 0.151 | 0.141 | 0.226 | 0.311 | 0.842 |
| hlen | valkey | 0.151 | 0.141 | 0.229 | 0.329 | 0.869 |
| hset_hdel | reddish | 0.173 | 0.167 | 0.285 | 0.405 | 0.697 |
| hset_hdel | redis | 0.163 | 0.164 | 0.249 | 0.338 | 1.027 |
| hset_hdel | valkey | 0.166 | 0.169 | 0.249 | 0.338 | 1.044 |
| incr | reddish | 0.157 | 0.151 | 0.268 | 0.386 | 0.662 |
| incr | redis | 0.151 | 0.141 | 0.237 | 0.340 | 0.857 |
| incr | valkey | 0.150 | 0.143 | 0.225 | 0.314 | 0.881 |
| llen | reddish | 0.157 | 0.151 | 0.261 | 0.362 | 0.652 |
| llen | redis | 0.152 | 0.141 | 0.237 | 0.357 | 0.839 |
| llen | valkey | 0.151 | 0.143 | 0.228 | 0.348 | 0.862 |
| lpush_lpop | reddish | 0.173 | 0.167 | 0.301 | 0.410 | 0.705 |
| lpush_lpop | redis | 0.162 | 0.167 | 0.233 | 0.332 | 1.010 |
| lpush_lpop | valkey | 0.162 | 0.165 | 0.234 | 0.361 | 1.019 |
| mixed | reddish | 0.162 | 0.153 | 0.285 | 0.410 | 0.651 |
| mixed | redis | 0.151 | 0.148 | 0.221 | 0.329 | 0.912 |
| mixed | valkey | 0.151 | 0.153 | 0.220 | 0.314 | 0.942 |
| ping | reddish | 0.124 | 0.119 | 0.205 | 0.277 | 0.515 |
| ping | redis | 0.121 | 0.111 | 0.186 | 0.591 | 0.692 |
| ping | valkey | 0.119 | 0.111 | 0.181 | 0.268 | 0.705 |
| reads | memcached | 0.141 | 0.135 | 0.284 | 0.346 | 1.048 |
| reads | reddish | 0.156 | 0.149 | 0.261 | 0.375 | 0.657 |
| reads | redis | 0.148 | 0.135 | 0.229 | 0.354 | 0.864 |
| reads | valkey | 0.148 | 0.149 | 0.220 | 0.301 | 0.916 |
| rpush_rpop | reddish | 0.175 | 0.167 | 0.305 | 0.417 | 0.719 |
| rpush_rpop | redis | 0.163 | 0.169 | 0.237 | 0.335 | 1.009 |
| rpush_rpop | valkey | 0.164 | 0.169 | 0.233 | 0.319 | 1.022 |
| set_del | reddish | 0.167 | 0.159 | 0.269 | 0.391 | 0.667 |
| set_del | redis | 0.161 | 0.151 | 0.247 | 0.346 | 0.930 |
| set_del | valkey | 0.159 | 0.151 | 0.231 | 0.327 | 0.948 |
| writes | memcached | 0.146 | 0.133 | 0.319 | 0.428 | 1.029 |
| writes | reddish | 0.166 | 0.159 | 0.268 | 0.399 | 0.685 |
| writes | redis | 0.155 | 0.159 | 0.223 | 0.329 | 0.949 |
| writes | valkey | 0.157 | 0.159 | 0.237 | 0.322 | 0.986 |

## Memory

Fresh server processes are measured before and after each preload. Each key holds one string, one list element, or one hash field. RSS includes process and allocator overhead. Delta bytes/key is (loaded RSS − idle RSS) × 1024 / verified key count. It is not the exact allocation size of a value.

| Shape | Server | Trials | Keys | Idle RSS MiB | Loaded RSS MiB | Delta bytes/key | Min bytes/key | Max bytes/key |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| hashes | reddish | 3 | 100000 | 3.96 | 14.35 | 108.91 | 108.91 | 108.91 |
| hashes | redis | 3 | 100000 | 8.66 | 19.16 | 110.13 | 109.85 | 110.51 |
| hashes | valkey | 3 | 100000 | 7.40 | 17.31 | 103.88 | 103.63 | 104.33 |
| lists | reddish | 3 | 100000 | 3.99 | 15.06 | 116.12 | 116.12 | 116.12 |
| lists | redis | 3 | 100000 | 8.65 | 19.17 | 110.27 | 109.90 | 110.47 |
| lists | valkey | 3 | 100000 | 7.60 | 17.42 | 103.03 | 103.01 | 103.06 |
| strings | memcached | 3 | 100000 | 4.61 | 17.68 | 137.09 | 137.09 | 137.09 |
| strings | reddish | 3 | 100000 | 3.99 | 12.75 | 91.87 | 91.87 | 91.87 |
| strings | redis | 3 | 100000 | 8.68 | 17.67 | 94.26 | 94.25 | 94.29 |
| strings | valkey | 3 | 100000 | 7.45 | 15.79 | 87.49 | 87.49 | 87.49 |

## WAL modes

Reddish only. These samples include WAL cost and use a separate workload; they are not part of the persistence-disabled comparison.

These samples use 32-byte values and a 50/50 SET/GET mix without CPU binding.

```text
KEYS=1000 CLIENTS=10 PIPELINE=16 DURATION_SECONDS=5 TRIALS=3 WARMUP_SECONDS=2
```

| Mode | Trials | Mean ops/s | Min | Max | Mean p99 ms | CPU µs/op |
|---|---:|---:|---:|---:|---:|---:|
| always | 3 | 70620 | 66178 | 73038 | 6.132 | 2.624 |
| everysec | 3 | 853303 | 838704 | 867280 | 0.287 | 0.673 |
| off | 3 | 984700 | 971634 | 997543 | 0.268 | 0.569 |

## Validation

No command-preflight artifact was recorded for this run. Original preload counts, miss/error checks and unit tests are separate evidence.

## Client CPU and server scheduling

Client CPU and main-thread scheduling were not recorded in this run. Server CPU/op above does not establish the limiting resource.

## Capacity measurements

No capacity sweep was recorded for this run. The baseline comparison is not a capacity claim.

## Pressure scenarios

No pressure-scenario artifact was recorded for this run. Baseline TTL bookkeeping does not measure expiry storms, and the baseline does not induce eviction.

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
| expire_ttl | reddish | 1 | 970140 | 0.163 | 0.255 |
| expire_ttl | reddish | 2 | 966478 | 0.164 | 0.271 |
| expire_ttl | reddish | 3 | 941909 | 0.168 | 0.327 |
| expire_ttl | reddish | 4 | 981219 | 0.161 | 0.247 |
| expire_ttl | reddish | 5 | 987520 | 0.160 | 0.239 |
| expire_ttl | redis | 1 | 1009202 | 0.157 | 0.239 |
| expire_ttl | redis | 2 | 1011151 | 0.156 | 0.231 |
| expire_ttl | redis | 3 | 1011140 | 0.156 | 0.231 |
| expire_ttl | redis | 4 | 1010631 | 0.156 | 0.231 |
| expire_ttl | redis | 5 | 1008534 | 0.157 | 0.247 |
| expire_ttl | valkey | 1 | 991545 | 0.160 | 0.343 |
| expire_ttl | valkey | 2 | 990055 | 0.160 | 0.359 |
| expire_ttl | valkey | 3 | 1001197 | 0.158 | 0.335 |
| expire_ttl | valkey | 4 | 999710 | 0.158 | 0.343 |
| expire_ttl | valkey | 5 | 997767 | 0.159 | 0.319 |
| hget | reddish | 1 | 934348 | 0.169 | 0.279 |
| hget | reddish | 2 | 922535 | 0.171 | 0.327 |
| hget | reddish | 3 | 934709 | 0.168 | 0.287 |
| hget | reddish | 4 | 946585 | 0.166 | 0.255 |
| hget | reddish | 5 | 940944 | 0.167 | 0.247 |
| hget | redis | 1 | 962225 | 0.164 | 0.287 |
| hget | redis | 2 | 966250 | 0.163 | 0.247 |
| hget | redis | 3 | 966818 | 0.163 | 0.263 |
| hget | redis | 4 | 964447 | 0.163 | 0.263 |
| hget | redis | 5 | 975751 | 0.161 | 0.239 |
| hget | valkey | 1 | 970249 | 0.162 | 0.247 |
| hget | valkey | 2 | 967407 | 0.163 | 0.255 |
| hget | valkey | 3 | 967101 | 0.163 | 0.247 |
| hget | valkey | 4 | 970484 | 0.162 | 0.263 |
| hget | valkey | 5 | 971550 | 0.162 | 0.239 |
| hlen | reddish | 1 | 1000023 | 0.158 | 0.295 |
| hlen | reddish | 2 | 1005243 | 0.157 | 0.263 |
| hlen | reddish | 3 | 993267 | 0.159 | 0.303 |
| hlen | reddish | 4 | 1012802 | 0.156 | 0.271 |
| hlen | reddish | 5 | 1001974 | 0.158 | 0.271 |
| hlen | redis | 1 | 1049637 | 0.151 | 0.223 |
| hlen | redis | 2 | 1048750 | 0.151 | 0.223 |
| hlen | redis | 3 | 1047517 | 0.151 | 0.239 |
| hlen | redis | 4 | 1054195 | 0.150 | 0.223 |
| hlen | redis | 5 | 1049364 | 0.151 | 0.223 |
| hlen | valkey | 1 | 1054440 | 0.150 | 0.223 |
| hlen | valkey | 2 | 1049808 | 0.151 | 0.239 |
| hlen | valkey | 3 | 1045796 | 0.151 | 0.231 |
| hlen | valkey | 4 | 1046497 | 0.151 | 0.231 |
| hlen | valkey | 5 | 1054931 | 0.150 | 0.223 |
| hset_hdel | reddish | 1 | 903139 | 0.175 | 0.327 |
| hset_hdel | reddish | 2 | 911128 | 0.174 | 0.279 |
| hset_hdel | reddish | 3 | 923592 | 0.171 | 0.263 |
| hset_hdel | reddish | 4 | 910141 | 0.174 | 0.303 |
| hset_hdel | reddish | 5 | 928176 | 0.171 | 0.255 |
| hset_hdel | redis | 1 | 977998 | 0.162 | 0.231 |
| hset_hdel | redis | 2 | 976860 | 0.162 | 0.231 |
| hset_hdel | redis | 3 | 975359 | 0.162 | 0.231 |
| hset_hdel | redis | 4 | 968708 | 0.163 | 0.239 |
| hset_hdel | redis | 5 | 956837 | 0.165 | 0.311 |
| hset_hdel | valkey | 1 | 960497 | 0.165 | 0.247 |
| hset_hdel | valkey | 2 | 964311 | 0.164 | 0.239 |
| hset_hdel | valkey | 3 | 935297 | 0.169 | 0.271 |
| hset_hdel | valkey | 4 | 954276 | 0.166 | 0.247 |
| hset_hdel | valkey | 5 | 966973 | 0.164 | 0.239 |
| incr | reddish | 1 | 1019070 | 0.155 | 0.239 |
| incr | reddish | 2 | 987679 | 0.160 | 0.311 |
| incr | reddish | 3 | 1012712 | 0.156 | 0.247 |
| incr | reddish | 4 | 994781 | 0.159 | 0.303 |
| incr | reddish | 5 | 1018842 | 0.155 | 0.239 |
| incr | redis | 1 | 1050186 | 0.151 | 0.223 |
| incr | redis | 2 | 1038246 | 0.152 | 0.247 |
| incr | redis | 3 | 1049573 | 0.151 | 0.223 |
| incr | redis | 4 | 1048226 | 0.151 | 0.223 |
| incr | redis | 5 | 1041022 | 0.152 | 0.271 |
| incr | valkey | 1 | 1053944 | 0.150 | 0.223 |
| incr | valkey | 2 | 1040316 | 0.152 | 0.239 |
| incr | valkey | 3 | 1054150 | 0.150 | 0.223 |
| incr | valkey | 4 | 1053666 | 0.150 | 0.223 |
| incr | valkey | 5 | 1058054 | 0.149 | 0.215 |
| llen | reddish | 1 | 1010811 | 0.156 | 0.247 |
| llen | reddish | 2 | 1008591 | 0.157 | 0.255 |
| llen | reddish | 3 | 975597 | 0.162 | 0.295 |
| llen | reddish | 4 | 1024564 | 0.154 | 0.231 |
| llen | reddish | 5 | 1009121 | 0.157 | 0.279 |
| llen | redis | 1 | 1019975 | 0.155 | 0.239 |
| llen | redis | 2 | 1050065 | 0.151 | 0.231 |
| llen | redis | 3 | 1040409 | 0.152 | 0.255 |
| llen | redis | 4 | 1045953 | 0.151 | 0.239 |
| llen | redis | 5 | 1049477 | 0.151 | 0.223 |
| llen | valkey | 1 | 1042118 | 0.152 | 0.231 |
| llen | valkey | 2 | 1056405 | 0.150 | 0.223 |
| llen | valkey | 3 | 1044801 | 0.151 | 0.239 |
| llen | valkey | 4 | 1046111 | 0.151 | 0.223 |
| llen | valkey | 5 | 1047174 | 0.151 | 0.223 |
| lpush_lpop | reddish | 1 | 901307 | 0.175 | 0.335 |
| lpush_lpop | reddish | 2 | 922712 | 0.171 | 0.263 |
| lpush_lpop | reddish | 3 | 902863 | 0.175 | 0.319 |
| lpush_lpop | reddish | 4 | 904800 | 0.174 | 0.335 |
| lpush_lpop | reddish | 5 | 928652 | 0.170 | 0.255 |
| lpush_lpop | redis | 1 | 968405 | 0.163 | 0.239 |
| lpush_lpop | redis | 2 | 971910 | 0.162 | 0.231 |
| lpush_lpop | redis | 3 | 974068 | 0.162 | 0.231 |
| lpush_lpop | redis | 4 | 970210 | 0.163 | 0.231 |
| lpush_lpop | redis | 5 | 971316 | 0.162 | 0.231 |
| lpush_lpop | valkey | 1 | 979689 | 0.161 | 0.223 |
| lpush_lpop | valkey | 2 | 965508 | 0.163 | 0.247 |
| lpush_lpop | valkey | 3 | 972499 | 0.162 | 0.239 |
| lpush_lpop | valkey | 4 | 972640 | 0.162 | 0.231 |
| lpush_lpop | valkey | 5 | 974378 | 0.162 | 0.231 |
| mixed | reddish | 1 | 987684 | 0.160 | 0.247 |
| mixed | reddish | 2 | 968809 | 0.163 | 0.295 |
| mixed | reddish | 3 | 963185 | 0.164 | 0.327 |
| mixed | reddish | 4 | 966141 | 0.163 | 0.303 |
| mixed | reddish | 5 | 979014 | 0.161 | 0.255 |
| mixed | redis | 1 | 1038916 | 0.152 | 0.223 |
| mixed | redis | 2 | 1037047 | 0.152 | 0.223 |
| mixed | redis | 3 | 1027949 | 0.154 | 0.223 |
| mixed | redis | 4 | 1034611 | 0.153 | 0.223 |
| mixed | redis | 5 | 1086084 | 0.145 | 0.215 |
| mixed | valkey | 1 | 1044871 | 0.151 | 0.215 |
| mixed | valkey | 2 | 1043091 | 0.151 | 0.223 |
| mixed | valkey | 3 | 1044511 | 0.151 | 0.215 |
| mixed | valkey | 4 | 1032448 | 0.153 | 0.231 |
| mixed | valkey | 5 | 1049271 | 0.150 | 0.215 |
| ping | reddish | 1 | 1295331 | 0.122 | 0.191 |
| ping | reddish | 2 | 1276991 | 0.123 | 0.191 |
| ping | reddish | 3 | 1267272 | 0.124 | 0.223 |
| ping | reddish | 4 | 1247361 | 0.126 | 0.223 |
| ping | reddish | 5 | 1267606 | 0.124 | 0.199 |
| ping | redis | 1 | 1301356 | 0.121 | 0.175 |
| ping | redis | 2 | 1312891 | 0.120 | 0.175 |
| ping | redis | 3 | 1292171 | 0.122 | 0.207 |
| ping | redis | 4 | 1298796 | 0.121 | 0.191 |
| ping | redis | 5 | 1311729 | 0.120 | 0.183 |
| ping | valkey | 1 | 1324722 | 0.119 | 0.183 |
| ping | valkey | 2 | 1325523 | 0.119 | 0.175 |
| ping | valkey | 3 | 1314826 | 0.120 | 0.183 |
| ping | valkey | 4 | 1310573 | 0.120 | 0.191 |
| ping | valkey | 5 | 1324078 | 0.119 | 0.175 |
| reads | memcached | 1 | 1118260 | 0.141 | 0.287 |
| reads | memcached | 2 | 1088528 | 0.145 | 0.303 |
| reads | memcached | 3 | 1138528 | 0.139 | 0.255 |
| reads | memcached | 4 | 1113822 | 0.142 | 0.295 |
| reads | memcached | 5 | 1131175 | 0.140 | 0.279 |
| reads | reddish | 1 | 997038 | 0.158 | 0.311 |
| reads | reddish | 2 | 1006338 | 0.157 | 0.271 |
| reads | reddish | 3 | 1015391 | 0.155 | 0.247 |
| reads | reddish | 4 | 1022932 | 0.154 | 0.239 |
| reads | reddish | 5 | 1024928 | 0.154 | 0.239 |
| reads | redis | 1 | 1042578 | 0.151 | 0.263 |
| reads | redis | 2 | 1063091 | 0.148 | 0.223 |
| reads | redis | 3 | 1060615 | 0.149 | 0.223 |
| reads | redis | 4 | 1060074 | 0.149 | 0.223 |
| reads | redis | 5 | 1097135 | 0.144 | 0.215 |
| reads | valkey | 1 | 1066138 | 0.148 | 0.223 |
| reads | valkey | 2 | 1053065 | 0.150 | 0.231 |
| reads | valkey | 3 | 1076798 | 0.146 | 0.215 |
| reads | valkey | 4 | 1068060 | 0.148 | 0.215 |
| reads | valkey | 5 | 1073118 | 0.147 | 0.215 |
| rpush_rpop | reddish | 1 | 894894 | 0.176 | 0.319 |
| rpush_rpop | reddish | 2 | 916935 | 0.172 | 0.279 |
| rpush_rpop | reddish | 3 | 903596 | 0.175 | 0.319 |
| rpush_rpop | reddish | 4 | 905058 | 0.174 | 0.271 |
| rpush_rpop | reddish | 5 | 885418 | 0.178 | 0.335 |
| rpush_rpop | redis | 1 | 965234 | 0.163 | 0.239 |
| rpush_rpop | redis | 2 | 965621 | 0.163 | 0.239 |
| rpush_rpop | redis | 3 | 969678 | 0.163 | 0.231 |
| rpush_rpop | redis | 4 | 965583 | 0.163 | 0.247 |
| rpush_rpop | redis | 5 | 969843 | 0.163 | 0.231 |
| rpush_rpop | valkey | 1 | 970688 | 0.163 | 0.231 |
| rpush_rpop | valkey | 2 | 933311 | 0.169 | 0.255 |
| rpush_rpop | valkey | 3 | 973408 | 0.162 | 0.223 |
| rpush_rpop | valkey | 4 | 971735 | 0.162 | 0.231 |
| rpush_rpop | valkey | 5 | 972309 | 0.162 | 0.223 |
| set_del | reddish | 1 | 951275 | 0.166 | 0.263 |
| set_del | reddish | 2 | 940937 | 0.168 | 0.287 |
| set_del | reddish | 3 | 943646 | 0.168 | 0.263 |
| set_del | reddish | 4 | 951879 | 0.166 | 0.255 |
| set_del | reddish | 5 | 940035 | 0.168 | 0.279 |
| set_del | redis | 1 | 976258 | 0.162 | 0.239 |
| set_del | redis | 2 | 968632 | 0.163 | 0.279 |
| set_del | redis | 3 | 982986 | 0.161 | 0.247 |
| set_del | redis | 4 | 989625 | 0.160 | 0.231 |
| set_del | redis | 5 | 988731 | 0.160 | 0.239 |
| set_del | valkey | 1 | 993188 | 0.159 | 0.231 |
| set_del | valkey | 2 | 994796 | 0.159 | 0.231 |
| set_del | valkey | 3 | 1000926 | 0.158 | 0.223 |
| set_del | valkey | 4 | 998755 | 0.158 | 0.231 |
| set_del | valkey | 5 | 989992 | 0.160 | 0.239 |
| writes | memcached | 1 | 1062614 | 0.149 | 0.375 |
| writes | memcached | 2 | 989587 | 0.160 | 0.423 |
| writes | memcached | 3 | 1147437 | 0.138 | 0.223 |
| writes | memcached | 4 | 1088900 | 0.146 | 0.351 |
| writes | memcached | 5 | 1156945 | 0.137 | 0.223 |
| writes | reddish | 1 | 954078 | 0.166 | 0.287 |
| writes | reddish | 2 | 959831 | 0.165 | 0.263 |
| writes | reddish | 3 | 954670 | 0.166 | 0.263 |
| writes | reddish | 4 | 955906 | 0.166 | 0.263 |
| writes | reddish | 5 | 959698 | 0.165 | 0.263 |
| writes | redis | 1 | 1013011 | 0.156 | 0.231 |
| writes | redis | 2 | 1016355 | 0.156 | 0.223 |
| writes | redis | 3 | 1027854 | 0.154 | 0.223 |
| writes | redis | 4 | 1015765 | 0.156 | 0.223 |
| writes | redis | 5 | 1044401 | 0.151 | 0.215 |
| writes | valkey | 1 | 1013626 | 0.156 | 0.231 |
| writes | valkey | 2 | 995977 | 0.159 | 0.247 |
| writes | valkey | 3 | 1020643 | 0.155 | 0.231 |
| writes | valkey | 4 | 1003100 | 0.158 | 0.239 |
| writes | valkey | 5 | 1008245 | 0.157 | 0.239 |

</details>

## Individual memory trials

<details>
<summary>All measured samples</summary>

| Shape | Server | Trial | Keys | Idle RSS KiB | Loaded RSS KiB | Delta bytes/key |
|---|---|---:|---:|---:|---:|---:|
| hashes | reddish | 1 | 100000 | 4060 | 14696 | 108.91 |
| hashes | reddish | 2 | 100000 | 4048 | 14684 | 108.91 |
| hashes | reddish | 3 | 100000 | 4052 | 14688 | 108.91 |
| hashes | redis | 1 | 100000 | 8892 | 19620 | 109.85 |
| hashes | redis | 2 | 100000 | 8876 | 19668 | 110.51 |
| hashes | redis | 3 | 100000 | 8840 | 19584 | 110.02 |
| hashes | valkey | 1 | 100000 | 7580 | 17700 | 103.63 |
| hashes | valkey | 2 | 100000 | 7496 | 17684 | 104.33 |
| hashes | valkey | 3 | 100000 | 7656 | 17780 | 103.67 |
| lists | reddish | 1 | 100000 | 4064 | 15404 | 116.12 |
| lists | reddish | 2 | 100000 | 4088 | 15428 | 116.12 |
| lists | reddish | 3 | 100000 | 4096 | 15436 | 116.12 |
| lists | redis | 1 | 100000 | 8916 | 19704 | 110.47 |
| lists | redis | 2 | 100000 | 8864 | 19648 | 110.43 |
| lists | redis | 3 | 100000 | 8804 | 19536 | 109.90 |
| lists | valkey | 1 | 100000 | 7824 | 17884 | 103.01 |
| lists | valkey | 2 | 100000 | 7712 | 17772 | 103.01 |
| lists | valkey | 3 | 100000 | 7808 | 17872 | 103.06 |
| strings | memcached | 1 | 100000 | 4704 | 18092 | 137.09 |
| strings | memcached | 2 | 100000 | 4728 | 18116 | 137.09 |
| strings | memcached | 3 | 100000 | 4724 | 18112 | 137.09 |
| strings | reddish | 1 | 100000 | 4088 | 13060 | 91.87 |
| strings | reddish | 2 | 100000 | 4096 | 13068 | 91.87 |
| strings | reddish | 3 | 100000 | 4060 | 13032 | 91.87 |
| strings | redis | 1 | 100000 | 8940 | 18144 | 94.25 |
| strings | redis | 2 | 100000 | 8792 | 17996 | 94.25 |
| strings | redis | 3 | 100000 | 8936 | 18144 | 94.29 |
| strings | valkey | 1 | 100000 | 7772 | 16316 | 87.49 |
| strings | valkey | 2 | 100000 | 7496 | 16040 | 87.49 |
| strings | valkey | 3 | 100000 | 7608 | 16152 | 87.49 |

</details>

## Measurement manifest

<details>
<summary>Revision, executable hash, hardware, tools, and settings</summary>

```text
37fa2e4e55fc632abae4e4117eb26f60d712f5fd
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  -
a41b76a5a2af503eb6bce2aa40340add712a1165507ff987112ab436398efd83  /home/runner/work/reddish/reddish/build/output/reddish
Measured at 2026-10-31 06:58:23 UTC
Linux runnervm48cm3 7.0.0-1017-azure #17~24.04.1-Ubuntu SMP PREEMPT Thu Sep 24 18:46:57 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux
Architecture:                            x86_64
CPU op-mode(s):                          32-bit, 64-bit
Address sizes:                           48 bits physical, 48 bits virtual
Byte Order:                              Little Endian
CPU(s):                                  4
On-line CPU(s) list:                     0-3
Vendor ID:                               AuthenticAMD
Model name:                              AMD EPYC 9V74 80-Core Processor
CPU family:                              25
Model:                                   17
Thread(s) per core:                      2
Core(s) per socket:                      2
Socket(s):                               1
Stepping:                                1
BogoMIPS:                                5192.28
Flags:                                   fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl xtopology tsc_reliable nonstop_tsc cpuid extd_apicid aperfmperf tsc_known_freq pni pclmulqdq ssse3 fma cx16 pcid sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand hypervisor lahf_lm cmp_legacy svm cr8_legacy abm sse4a misalignsse 3dnowprefetch osvw topoext vmmcall fsgsbase bmi1 avx2 smep bmi2 erms invpcid avx512f avx512dq rdseed adx smap avx512ifma clflushopt clwb avx512cd sha_ni avx512bw avx512vl xsaveopt xsavec xgetbv1 xsaves user_shstk avx512_bf16 clzero xsaveerptr rdpru arat npt nrip_save tsc_scale vmcb_clean flushbyasid decodeassists pausefilter pfthreshold v_vmsave_vmload avx512vbmi umip avx512_vbmi2 gfni vaes vpclmulqdq avx512_vnni avx512_bitalg avx512_vpopcntdq rdpid fsrm
Virtualization:                          AMD-V
Hypervisor vendor:                       Microsoft
Virtualization type:                     full
L1d cache:                               64 KiB (2 instances)
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
Vulnerability Tsa:                       Vulnerable: No microcode
Vulnerability Tsx async abort:           Not affected
Vulnerability Vmscape:                   Not affected
memtier_benchmark v=2.5.1 sha=5f634d17:0 bits=64 libevent=2.1.12-stable openssl=OpenSSL 3.0.13 30 Jan 2024 prometheus=yes
Copyright (C) 2011-2026 Redis Ltd.
This is free software.  You may redistribute copies of it under the terms of
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.
There is NO WARRANTY, to the extent permitted by law.
Redis server v=8.10.1 sha=3399357e:0 malloc=jemalloc-5.3.0 bits=64 build=ba14b7aebb207b99
Valkey server v=8.1.1 sha=fcd8bc3e:0 malloc=jemalloc-5.3.0 bits=64 build=6671c1fb0c94de5b
memcached 1.6.24
SERVER_CPU=unbound CLIENT_CPUS=unbound
REQUESTS_PER_CLIENT=0
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=5 DURATION_SECONDS=30 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=5 persistence=disabled
```

</details>
