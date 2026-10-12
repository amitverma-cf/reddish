# Benchmark results

Measured revision: [7b54afa](https://github.com/amitverma-cf/reddish/commit/7b54afa105313650e51e9bf095a243056c72491f).

Loopback TCP on the machine below. Persistence is disabled for the server comparison. Each server receives the same preload, warmup, values, clients, and pipeline. Server order rotates between trials; GET and SET order alternates.

## Setup

```text
Measured at 2026-10-31 05:44:20 UTC
Linux amve 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun  5 18:30:46 UTC 2025 x86_64 GNU/Linux
CPU(s):                               16
Model name:                           AMD Ryzen 7 5700U with Radeon Graphics
memtier_benchmark v=2.5.1 sha=00000000:0 bits=64 libevent=2.1.13-stable openssl=OpenSSL 3.5.6 7 Apr 2026 prometheus=yes
Redis server v=8.10.1 sha=00000000:1 malloc=jemalloc-5.3.0 bits=64 build=37a21a9634b82552
Valkey server v=8.1.1 sha=00000000:0 malloc=jemalloc-5.3.0 bits=64 build=ad0f3ed608b40934
memcached 1.6.38
SERVER_CPU=2 CLIENT_CPUS=4
REQUESTS_PER_CLIENT=0
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=3 DURATION_SECONDS=5 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=2 persistence=disabled
```

## Throughput

Operations per second. Range and standard deviation describe the trial means, not individual requests.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 3 | 370083 | 336569 | 394658 | 30059 |
| mixed | redis | 3 | 390756 | 368847 | 424453 | 29618 |
| mixed | valkey | 3 | 363498 | 295739 | 402491 | 58903 |
| reads | memcached | 3 | 448644 | 384657 | 520394 | 68201 |
| reads | reddish | 3 | 409028 | 362206 | 440124 | 41271 |
| reads | redis | 3 | 382192 | 356858 | 402867 | 23355 |
| reads | valkey | 3 | 390619 | 325255 | 440196 | 59074 |
| writes | memcached | 3 | 510926 | 486098 | 534892 | 24409 |
| writes | reddish | 3 | 368414 | 316768 | 439413 | 63571 |
| writes | redis | 3 | 345827 | 285534 | 381999 | 52562 |
| writes | valkey | 3 | 359454 | 300038 | 392377 | 51556 |

## Other commands

Each paired workload reports individual commands per second.

| Workload | Server | Trials | Mean ops/s | Min | Max | Std. dev. |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 3 | 263406 | 174514 | 325367 | 78950 |
| expire_ttl | redis | 3 | 397730 | 388970 | 407585 | 9355 |
| expire_ttl | valkey | 3 | 358677 | 298305 | 427605 | 65073 |
| hget | reddish | 3 | 270687 | 231370 | 313674 | 41275 |
| hget | redis | 3 | 347216 | 295354 | 431585 | 73703 |
| hget | valkey | 3 | 363328 | 308440 | 404003 | 49341 |
| hlen | reddish | 3 | 305989 | 245610 | 379416 | 67851 |
| hlen | redis | 3 | 395940 | 332771 | 472709 | 70953 |
| hlen | valkey | 3 | 374664 | 316865 | 412354 | 50821 |
| hset_hdel | reddish | 3 | 183199 | 85513 | 305068 | 111757 |
| hset_hdel | redis | 3 | 357939 | 346121 | 369462 | 11673 |
| hset_hdel | valkey | 3 | 368068 | 342128 | 406333 | 33831 |
| incr | reddish | 3 | 326276 | 303484 | 352356 | 24602 |
| incr | redis | 3 | 320859 | 224423 | 431050 | 103998 |
| incr | valkey | 3 | 275045 | 203852 | 317475 | 62032 |
| llen | reddish | 3 | 367398 | 315548 | 394112 | 44910 |
| llen | redis | 3 | 362191 | 314180 | 441480 | 69174 |
| llen | valkey | 3 | 379551 | 329050 | 438192 | 55025 |
| lpush_lpop | reddish | 3 | 312749 | 268605 | 383614 | 61986 |
| lpush_lpop | redis | 3 | 313902 | 276757 | 335065 | 32273 |
| lpush_lpop | valkey | 3 | 279200 | 269767 | 284890 | 8227 |
| ping | reddish | 3 | 426533 | 381054 | 465411 | 42564 |
| ping | redis | 3 | 455145 | 400759 | 544809 | 78237 |
| ping | valkey | 3 | 400906 | 369512 | 420467 | 27462 |
| rpush_rpop | reddish | 3 | 334294 | 276928 | 378385 | 52015 |
| rpush_rpop | redis | 3 | 284815 | 273224 | 302036 | 15209 |
| rpush_rpop | valkey | 3 | 299810 | 259329 | 353580 | 48511 |
| set_del | reddish | 3 | 309690 | 256916 | 378714 | 62504 |
| set_del | redis | 3 | 313361 | 273761 | 383822 | 61177 |
| set_del | valkey | 3 | 286856 | 281715 | 295811 | 7784 |

## Latency and CPU

Latency is in milliseconds. Percentiles are averages of each trial percentile; they are not pooled percentiles. CPU is server user plus system time per operation, excluding the load generator.

| Workload | Server | Mean latency ms | p50 ms | p99 ms | p99.9 ms | CPU µs/op |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 0.645 | 0.591 | 1.626 | 4.239 | 2.522 |
| expire_ttl | redis | 0.397 | 0.338 | 1.164 | 2.092 | 2.299 |
| expire_ttl | valkey | 0.450 | 0.383 | 1.266 | 2.159 | 2.502 |
| hget | reddish | 0.591 | 0.519 | 1.660 | 4.138 | 2.386 |
| hget | redis | 0.466 | 0.418 | 1.242 | 2.175 | 2.374 |
| hget | valkey | 0.439 | 0.391 | 1.226 | 2.215 | 2.361 |
| hlen | reddish | 0.533 | 0.476 | 1.508 | 3.786 | 2.148 |
| hlen | redis | 0.407 | 0.359 | 1.194 | 2.567 | 2.074 |
| hlen | valkey | 0.427 | 0.372 | 1.234 | 1.986 | 2.245 |
| hset_hdel | reddish | 1.125 | 0.671 | 1.802 | 4.546 | 2.516 |
| hset_hdel | redis | 0.442 | 0.380 | 1.231 | 1.738 | 2.536 |
| hset_hdel | valkey | 0.432 | 0.380 | 1.175 | 1.748 | 2.488 |
| incr | reddish | 0.486 | 0.442 | 1.276 | 3.044 | 1.929 |
| incr | redis | 0.528 | 0.463 | 1.559 | 3.132 | 2.694 |
| incr | valkey | 0.598 | 0.527 | 1.663 | 3.866 | 3.196 |
| llen | reddish | 0.435 | 0.402 | 1.082 | 2.140 | 1.759 |
| llen | redis | 0.446 | 0.399 | 1.202 | 2.244 | 2.381 |
| llen | valkey | 0.422 | 0.380 | 1.124 | 2.172 | 2.238 |
| lpush_lpop | reddish | 0.517 | 0.482 | 1.223 | 2.628 | 1.949 |
| lpush_lpop | redis | 0.506 | 0.442 | 1.394 | 3.668 | 2.875 |
| lpush_lpop | valkey | 0.565 | 0.495 | 1.554 | 2.687 | 3.256 |
| mixed | reddish | 0.429 | 0.394 | 1.079 | 1.927 | 1.643 |
| mixed | redis | 0.406 | 0.343 | 1.175 | 2.180 | 2.347 |
| mixed | valkey | 0.443 | 0.378 | 1.276 | 2.260 | 2.472 |
| ping | reddish | 0.372 | 0.340 | 0.988 | 1.634 | 1.499 |
| ping | redis | 0.352 | 0.311 | 1.044 | 2.148 | 1.854 |
| ping | valkey | 0.394 | 0.348 | 1.143 | 2.623 | 2.147 |
| reads | memcached | 0.357 | 0.303 | 1.148 | 2.276 | 2.006 |
| reads | reddish | 0.389 | 0.362 | 0.956 | 1.820 | 1.638 |
| reads | redis | 0.413 | 0.367 | 1.124 | 2.052 | 2.325 |
| reads | valkey | 0.410 | 0.362 | 1.156 | 2.044 | 2.256 |
| rpush_rpop | reddish | 0.480 | 0.450 | 1.130 | 2.052 | 1.893 |
| rpush_rpop | redis | 0.555 | 0.492 | 1.535 | 3.487 | 3.169 |
| rpush_rpop | valkey | 0.535 | 0.458 | 1.562 | 4.298 | 2.999 |
| set_del | reddish | 0.524 | 0.487 | 1.290 | 2.596 | 1.957 |
| set_del | redis | 0.516 | 0.439 | 1.479 | 4.442 | 2.768 |
| set_del | valkey | 0.551 | 0.487 | 1.500 | 2.746 | 3.076 |
| writes | memcached | 0.310 | 0.268 | 1.015 | 1.842 | 1.757 |
| writes | reddish | 0.438 | 0.412 | 1.044 | 1.626 | 1.730 |
| writes | redis | 0.465 | 0.404 | 1.290 | 2.162 | 2.664 |
| writes | valkey | 0.447 | 0.375 | 1.316 | 2.338 | 2.544 |

## Memory

Fresh server processes are measured before and after each preload. Each key holds one string, one list element, or one hash field. RSS includes process and allocator overhead. Delta bytes/key is (loaded RSS − idle RSS) × 1024 / verified key count. It is not the exact allocation size of a value.

| Shape | Server | Trials | Keys | Idle RSS MiB | Loaded RSS MiB | Delta bytes/key | Min bytes/key | Max bytes/key |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| hashes | reddish | 3 | 100000 | 3.50 | 13.92 | 109.27 | 109.24 | 109.28 |
| hashes | redis | 3 | 100000 | 13.04 | 23.12 | 105.73 | 104.86 | 106.17 |
| hashes | valkey | 3 | 100000 | 13.67 | 24.33 | 111.85 | 111.41 | 112.72 |
| lists | reddish | 3 | 100000 | 3.50 | 14.64 | 116.76 | 116.49 | 116.90 |
| lists | redis | 3 | 100000 | 13.00 | 23.12 | 106.17 | 104.86 | 107.48 |
| lists | valkey | 3 | 100000 | 13.67 | 24.33 | 111.85 | 111.41 | 112.72 |
| strings | memcached | 3 | 100000 | 5.33 | 17.83 | 131.07 | 131.07 | 131.07 |
| strings | reddish | 3 | 100000 | 3.50 | 12.33 | 92.60 | 92.00 | 92.90 |
| strings | redis | 3 | 100000 | 12.96 | 21.54 | 90.00 | 89.13 | 90.44 |
| strings | valkey | 3 | 100000 | 13.58 | 22.71 | 95.68 | 95.68 | 95.68 |

## WAL modes

Reddish only. These samples include WAL cost and use a separate workload; they are not part of the persistence-disabled comparison.

These samples use 32-byte values and a 50/50 SET/GET mix without CPU binding.

```text
KEYS=1000 CLIENTS=10 PIPELINE=16 DURATION_SECONDS=3 TRIALS=3 WARMUP_SECONDS=2
```

| Mode | Trials | Mean ops/s | Min | Max | Mean p99 ms | CPU µs/op |
|---|---:|---:|---:|---:|---:|---:|
| always | 3 | 8859 | 8843 | 8887 | 38.740 | 14.375 |
| everysec | 3 | 229212 | 218453 | 235004 | 2.447 | 2.132 |
| off | 3 | 294701 | 285403 | 300852 | 1.292 | 2.016 |

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
| expire_ttl | reddish | 1 | 290336 | 0.544 | 1.583 |
| expire_ttl | reddish | 2 | 174514 | 0.905 | 2.159 |
| expire_ttl | reddish | 3 | 325367 | 0.486 | 1.135 |
| expire_ttl | redis | 1 | 407585 | 0.388 | 1.127 |
| expire_ttl | redis | 2 | 396636 | 0.398 | 1.167 |
| expire_ttl | redis | 3 | 388970 | 0.406 | 1.199 |
| expire_ttl | valkey | 1 | 298305 | 0.530 | 1.335 |
| expire_ttl | valkey | 2 | 427605 | 0.370 | 1.055 |
| expire_ttl | valkey | 3 | 350121 | 0.451 | 1.407 |
| hget | reddish | 1 | 267018 | 0.590 | 1.391 |
| hget | reddish | 2 | 231370 | 0.681 | 2.319 |
| hget | reddish | 3 | 313674 | 0.502 | 1.271 |
| hget | redis | 1 | 314710 | 0.500 | 1.367 |
| hget | redis | 2 | 431585 | 0.365 | 0.983 |
| hget | redis | 3 | 295354 | 0.533 | 1.375 |
| hget | valkey | 1 | 308440 | 0.510 | 1.327 |
| hget | valkey | 2 | 404003 | 0.390 | 1.111 |
| hget | valkey | 3 | 377540 | 0.417 | 1.239 |
| hlen | reddish | 1 | 292939 | 0.540 | 1.375 |
| hlen | reddish | 2 | 245610 | 0.643 | 2.007 |
| hlen | reddish | 3 | 379416 | 0.417 | 1.143 |
| hlen | redis | 1 | 382340 | 0.413 | 1.191 |
| hlen | redis | 2 | 472709 | 0.334 | 0.943 |
| hlen | redis | 3 | 332771 | 0.475 | 1.447 |
| hlen | valkey | 1 | 316865 | 0.499 | 1.367 |
| hlen | valkey | 2 | 394772 | 0.400 | 1.199 |
| hlen | valkey | 3 | 412354 | 0.383 | 1.135 |
| hset_hdel | reddish | 1 | 85513 | 1.864 | 1.863 |
| hset_hdel | reddish | 2 | 159017 | 0.994 | 2.399 |
| hset_hdel | reddish | 3 | 305068 | 0.518 | 1.143 |
| hset_hdel | redis | 1 | 369462 | 0.428 | 1.191 |
| hset_hdel | redis | 2 | 346121 | 0.457 | 1.255 |
| hset_hdel | redis | 3 | 358234 | 0.442 | 1.247 |
| hset_hdel | valkey | 1 | 342128 | 0.462 | 1.239 |
| hset_hdel | valkey | 2 | 406333 | 0.389 | 1.055 |
| hset_hdel | valkey | 3 | 355741 | 0.445 | 1.231 |
| incr | reddish | 1 | 322989 | 0.489 | 1.503 |
| incr | reddish | 2 | 352356 | 0.449 | 1.127 |
| incr | reddish | 3 | 303484 | 0.521 | 1.199 |
| incr | redis | 1 | 224423 | 0.704 | 2.239 |
| incr | redis | 2 | 431050 | 0.366 | 1.007 |
| incr | redis | 3 | 307104 | 0.515 | 1.431 |
| incr | valkey | 1 | 303808 | 0.520 | 1.407 |
| incr | valkey | 2 | 203852 | 0.775 | 2.255 |
| incr | valkey | 3 | 317475 | 0.498 | 1.327 |
| llen | reddish | 1 | 315548 | 0.501 | 1.111 |
| llen | reddish | 2 | 392534 | 0.403 | 1.047 |
| llen | reddish | 3 | 394112 | 0.401 | 1.087 |
| llen | redis | 1 | 314180 | 0.503 | 1.407 |
| llen | redis | 2 | 441480 | 0.358 | 0.975 |
| llen | redis | 3 | 330914 | 0.478 | 1.223 |
| llen | valkey | 1 | 329050 | 0.480 | 1.135 |
| llen | valkey | 2 | 438192 | 0.361 | 0.967 |
| llen | valkey | 3 | 371409 | 0.426 | 1.271 |
| lpush_lpop | reddish | 1 | 286028 | 0.552 | 1.279 |
| lpush_lpop | reddish | 2 | 383614 | 0.412 | 0.951 |
| lpush_lpop | reddish | 3 | 268605 | 0.587 | 1.439 |
| lpush_lpop | redis | 1 | 276757 | 0.569 | 1.559 |
| lpush_lpop | redis | 2 | 335065 | 0.470 | 1.247 |
| lpush_lpop | redis | 3 | 329885 | 0.478 | 1.375 |
| lpush_lpop | valkey | 1 | 269767 | 0.585 | 1.559 |
| lpush_lpop | valkey | 2 | 284890 | 0.553 | 1.663 |
| lpush_lpop | valkey | 3 | 282942 | 0.557 | 1.439 |
| mixed | reddish | 1 | 336569 | 0.469 | 1.135 |
| mixed | reddish | 2 | 394658 | 0.400 | 1.031 |
| mixed | reddish | 3 | 379023 | 0.417 | 1.071 |
| mixed | redis | 1 | 368847 | 0.428 | 1.175 |
| mixed | redis | 2 | 424453 | 0.372 | 1.111 |
| mixed | redis | 3 | 378968 | 0.417 | 1.239 |
| mixed | valkey | 1 | 402491 | 0.392 | 1.151 |
| mixed | valkey | 2 | 392264 | 0.402 | 1.199 |
| mixed | valkey | 3 | 295739 | 0.534 | 1.479 |
| ping | reddish | 1 | 465411 | 0.339 | 0.863 |
| ping | reddish | 2 | 433134 | 0.364 | 1.015 |
| ping | reddish | 3 | 381054 | 0.414 | 1.087 |
| ping | redis | 1 | 400759 | 0.393 | 1.151 |
| ping | redis | 2 | 544809 | 0.289 | 0.823 |
| ping | redis | 3 | 419867 | 0.375 | 1.159 |
| ping | valkey | 1 | 420467 | 0.375 | 1.039 |
| ping | valkey | 2 | 369512 | 0.426 | 1.311 |
| ping | valkey | 3 | 412739 | 0.382 | 1.079 |
| reads | memcached | 1 | 384657 | 0.410 | 1.271 |
| reads | memcached | 2 | 520394 | 0.303 | 1.047 |
| reads | memcached | 3 | 440880 | 0.358 | 1.127 |
| reads | reddish | 1 | 424754 | 0.371 | 0.927 |
| reads | reddish | 2 | 440124 | 0.359 | 0.887 |
| reads | reddish | 3 | 362206 | 0.436 | 1.055 |
| reads | redis | 1 | 386850 | 0.407 | 1.047 |
| reads | redis | 2 | 402867 | 0.391 | 1.135 |
| reads | redis | 3 | 356858 | 0.442 | 1.191 |
| reads | valkey | 1 | 406405 | 0.388 | 1.063 |
| reads | valkey | 2 | 440196 | 0.358 | 1.055 |
| reads | valkey | 3 | 325255 | 0.485 | 1.351 |
| rpush_rpop | reddish | 1 | 276928 | 0.570 | 1.295 |
| rpush_rpop | reddish | 2 | 378385 | 0.417 | 1.007 |
| rpush_rpop | reddish | 3 | 347570 | 0.454 | 1.087 |
| rpush_rpop | redis | 1 | 273224 | 0.577 | 1.543 |
| rpush_rpop | redis | 2 | 302036 | 0.522 | 1.527 |
| rpush_rpop | redis | 3 | 279186 | 0.565 | 1.535 |
| rpush_rpop | valkey | 1 | 259329 | 0.608 | 1.831 |
| rpush_rpop | valkey | 2 | 353580 | 0.446 | 1.319 |
| rpush_rpop | valkey | 3 | 286521 | 0.550 | 1.535 |
| set_del | reddish | 1 | 256916 | 0.616 | 1.679 |
| set_del | reddish | 2 | 378714 | 0.418 | 0.999 |
| set_del | reddish | 3 | 293439 | 0.539 | 1.191 |
| set_del | redis | 1 | 273761 | 0.577 | 1.839 |
| set_del | redis | 2 | 383822 | 0.412 | 1.127 |
| set_del | redis | 3 | 282499 | 0.560 | 1.471 |
| set_del | valkey | 1 | 295811 | 0.534 | 1.463 |
| set_del | valkey | 2 | 281715 | 0.561 | 1.479 |
| set_del | valkey | 3 | 283041 | 0.558 | 1.559 |
| writes | memcached | 1 | 511789 | 0.309 | 1.015 |
| writes | memcached | 2 | 534892 | 0.296 | 0.951 |
| writes | memcached | 3 | 486098 | 0.326 | 1.079 |
| writes | reddish | 1 | 349061 | 0.453 | 1.063 |
| writes | reddish | 2 | 316768 | 0.500 | 1.143 |
| writes | reddish | 3 | 439413 | 0.360 | 0.927 |
| writes | redis | 1 | 381999 | 0.414 | 1.071 |
| writes | redis | 2 | 285534 | 0.554 | 1.527 |
| writes | redis | 3 | 369949 | 0.428 | 1.271 |
| writes | valkey | 1 | 392377 | 0.403 | 1.175 |
| writes | valkey | 2 | 385948 | 0.410 | 1.207 |
| writes | valkey | 3 | 300038 | 0.527 | 1.567 |

</details>

## Individual memory trials

<details>
<summary>All measured samples</summary>

| Shape | Server | Trial | Keys | Idle RSS KiB | Loaded RSS KiB | Delta bytes/key |
|---|---|---:|---:|---:|---:|---:|
| hashes | reddish | 1 | 100000 | 3584 | 14252 | 109.24 |
| hashes | reddish | 2 | 100000 | 3584 | 14256 | 109.28 |
| hashes | reddish | 3 | 100000 | 3584 | 14256 | 109.28 |
| hashes | redis | 1 | 100000 | 13440 | 23680 | 104.86 |
| hashes | redis | 2 | 100000 | 13312 | 23680 | 106.17 |
| hashes | redis | 3 | 100000 | 13312 | 23680 | 106.17 |
| hashes | valkey | 1 | 100000 | 13952 | 24832 | 111.41 |
| hashes | valkey | 2 | 100000 | 13952 | 24960 | 112.72 |
| hashes | valkey | 3 | 100000 | 14080 | 24960 | 111.41 |
| lists | reddish | 1 | 100000 | 3584 | 15000 | 116.90 |
| lists | reddish | 2 | 100000 | 3584 | 14960 | 116.49 |
| lists | reddish | 3 | 100000 | 3584 | 15000 | 116.90 |
| lists | redis | 1 | 100000 | 13312 | 23680 | 106.17 |
| lists | redis | 2 | 100000 | 13440 | 23680 | 104.86 |
| lists | redis | 3 | 100000 | 13184 | 23680 | 107.48 |
| lists | valkey | 1 | 100000 | 13952 | 24832 | 111.41 |
| lists | valkey | 2 | 100000 | 13952 | 24960 | 112.72 |
| lists | valkey | 3 | 100000 | 14080 | 24960 | 111.41 |
| strings | memcached | 1 | 100000 | 5376 | 18176 | 131.07 |
| strings | memcached | 2 | 100000 | 5504 | 18304 | 131.07 |
| strings | memcached | 3 | 100000 | 5504 | 18304 | 131.07 |
| strings | reddish | 1 | 100000 | 3584 | 12568 | 92.00 |
| strings | reddish | 2 | 100000 | 3584 | 12656 | 92.90 |
| strings | reddish | 3 | 100000 | 3584 | 12656 | 92.90 |
| strings | redis | 1 | 100000 | 13184 | 22016 | 90.44 |
| strings | redis | 2 | 100000 | 13312 | 22016 | 89.13 |
| strings | redis | 3 | 100000 | 13312 | 22144 | 90.44 |
| strings | valkey | 1 | 100000 | 13952 | 23296 | 95.68 |
| strings | valkey | 2 | 100000 | 13824 | 23168 | 95.68 |
| strings | valkey | 3 | 100000 | 13952 | 23296 | 95.68 |

</details>

## Measurement manifest

<details>
<summary>Revision, executable hash, hardware, tools, and settings</summary>

```text
7b54afa105313650e51e9bf095a243056c72491f
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855  -
b08fcfd92e480e0560e070702d61c3b5cd81eb9a6dcba283558e230900a34c56  build-wsl-verify/output/reddish
Measured at 2026-10-31 05:44:20 UTC
Linux amve 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun  5 18:30:46 UTC 2025 x86_64 GNU/Linux
Architecture:                         x86_64
CPU op-mode(s):                       32-bit, 64-bit
Address sizes:                        48 bits physical, 48 bits virtual
Byte Order:                           Little Endian
CPU(s):                               16
On-line CPU(s) list:                  0-15
Vendor ID:                            AuthenticAMD
Model name:                           AMD Ryzen 7 5700U with Radeon Graphics
CPU family:                           23
Model:                                104
Thread(s) per core:                   2
Core(s) per socket:                   8
Socket(s):                            1
Stepping:                             1
BogoMIPS:                             3593.13
Flags:                                fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl tsc_reliable nonstop_tsc cpuid extd_apicid tsc_known_freq pni pclmulqdq ssse3 fma cx16 sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand hypervisor lahf_lm cmp_legacy svm cr8_legacy abm sse4a misalignsse 3dnowprefetch osvw topoext perfctr_core ssbd ibrs ibpb stibp vmmcall fsgsbase bmi1 avx2 smep bmi2 rdseed adx smap clflushopt clwb sha_ni xsaveopt xsavec xgetbv1 clzero xsaveerptr arat npt nrip_save tsc_scale vmcb_clean flushbyasid decodeassists pausefilter pfthreshold v_vmsave_vmload umip rdpid
Virtualization:                       AMD-V
Hypervisor vendor:                    Microsoft
Virtualization type:                  full
L1d cache:                            256 KiB (8 instances)
L1i cache:                            256 KiB (8 instances)
L2 cache:                             4 MiB (8 instances)
L3 cache:                             4 MiB (1 instance)
NUMA node(s):                         1
NUMA node0 CPU(s):                    0-15
Vulnerability Gather data sampling:   Not affected
Vulnerability Itlb multihit:          Not affected
Vulnerability L1tf:                   Not affected
Vulnerability Mds:                    Not affected
Vulnerability Meltdown:               Not affected
Vulnerability Mmio stale data:        Not affected
Vulnerability Reg file data sampling: Not affected
Vulnerability Retbleed:               Mitigation; untrained return thunk; SMT enabled with STIBP protection
Vulnerability Spec rstack overflow:   Vulnerable: Safe RET, no microcode
Vulnerability Spec store bypass:      Mitigation; Speculative Store Bypass disabled via prctl
Vulnerability Spectre v1:             Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:             Mitigation; Retpolines; IBPB conditional; STIBP always-on; RSB filling; PBRSB-eIBRS Not affected; BHI Not affected
Vulnerability Srbds:                  Not affected
Vulnerability Tsx async abort:        Not affected
memtier_benchmark v=2.5.1 sha=00000000:0 bits=64 libevent=2.1.13-stable openssl=OpenSSL 3.5.6 7 Apr 2026 prometheus=yes
Copyright (C) 2011-2026 Redis Ltd.
This is free software.  You may redistribute copies of it under the terms of
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.
There is NO WARRANTY, to the extent permitted by law.
Redis server v=8.10.1 sha=00000000:1 malloc=jemalloc-5.3.0 bits=64 build=37a21a9634b82552
Valkey server v=8.1.1 sha=00000000:0 malloc=jemalloc-5.3.0 bits=64 build=ad0f3ed608b40934
memcached 1.6.38
SERVER_CPU=2 CLIENT_CPUS=4
REQUESTS_PER_CLIENT=0
KEYS=100000 VALUE_BYTES=32 THREADS=1 CLIENTS=10 PIPELINE=16 TRIALS=3 DURATION_SECONDS=5 WARMUP_SECONDS=2 MEMORY_TRIALS=3 RUN_EXTENDED=1 AUXILIARY_DURATION_SECONDS=2 persistence=disabled
```

</details>
