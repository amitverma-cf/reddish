# Reddish, Redis, Valkey, and Memcached benchmarks

The headline workloads are `reads` (100% GET), `writes` (100% SET), and `mixed` (50% GET, 50% SET). The full suite additionally exercises every implemented command family and compares RSS after equivalent string, list, and hash preloads. Memcached is a simple string-cache baseline: it participates only in GET, SET, and string RSS measurements.

This is a narrow loopback benchmark using 32-byte values, one memtier worker, ten clients, pipeline depth 16, and no timed persistence. RSS includes process and allocator overhead; it is useful for comparison, not an exact per-value allocation measurement.

<!-- generated-results:start -->
## Published results

Aggregated from 1 benchmark run directories.

### System and toolchain

- OS: Ubuntu 24.04
- Kernel: 6.17.0-1022-azure
- CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz (4 logical CPUs)
- Memory: 15989 MiB
- Compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0

### Command throughput

| Workload | Server | Samples | Average ops/s | Min ops/s | Max ops/s | Average p99 ms |
|---|---|---:|---:|---:|---:|---:|
| reads | memcached | 5 | 842504 | 836623 | 846889 | 0.300 |
| writes | memcached | 5 | 788265 | 785347 | 792647 | 0.397 |
| expire_ttl | reddish | 5 | 998131 | 990995 | 1004891 | 0.156 |
| hget | reddish | 5 | 978170 | 940303 | 990115 | 0.257 |
| hlen | reddish | 5 | 1115735 | 1108225 | 1125584 | 0.217 |
| hset_hdel | reddish | 5 | 953888 | 945551 | 963940 | 0.167 |
| incr | reddish | 5 | 1096442 | 1089414 | 1108678 | 0.242 |
| llen | reddish | 5 | 1109103 | 1078573 | 1122403 | 0.221 |
| lpush_lpop | reddish | 5 | 944960 | 932950 | 948783 | 0.167 |
| mixed | reddish | 5 | 1045191 | 1032838 | 1059756 | 0.244 |
| ping | reddish | 5 | 1505138 | 1480147 | 1528539 | 0.164 |
| reads | reddish | 5 | 1136178 | 1129062 | 1140112 | 0.217 |
| rpush_rpop | reddish | 5 | 942147 | 936722 | 946127 | 0.167 |
| set_del | reddish | 5 | 982797 | 966292 | 1001516 | 0.159 |
| writes | reddish | 5 | 995744 | 990000 | 1000300 | 0.255 |
| expire_ttl | redis | 5 | 1013889 | 1005363 | 1020323 | 0.159 |
| hget | redis | 5 | 979592 | 973822 | 986252 | 0.253 |
| hlen | redis | 5 | 1094255 | 1087956 | 1104937 | 0.231 |
| hset_hdel | redis | 5 | 965888 | 952929 | 971799 | 0.167 |
| incr | redis | 5 | 1091947 | 1083867 | 1094699 | 0.226 |
| llen | redis | 5 | 1097275 | 1091017 | 1102102 | 0.226 |
| lpush_lpop | redis | 5 | 965497 | 950876 | 980767 | 0.165 |
| mixed | redis | 5 | 1063133 | 1055875 | 1075108 | 0.228 |
| ping | redis | 5 | 1516337 | 1508947 | 1519879 | 0.157 |
| reads | redis | 5 | 1141104 | 1132804 | 1151498 | 0.212 |
| rpush_rpop | redis | 5 | 962519 | 954352 | 971014 | 0.167 |
| set_del | redis | 5 | 989136 | 982392 | 1000449 | 0.154 |
| writes | redis | 5 | 1009228 | 1003192 | 1016547 | 0.239 |
| expire_ttl | valkey | 5 | 1010422 | 1000217 | 1024773 | 0.159 |
| hget | valkey | 5 | 975163 | 964716 | 989280 | 0.265 |
| hlen | valkey | 5 | 1096467 | 1080903 | 1114511 | 0.225 |
| hset_hdel | valkey | 5 | 974775 | 965978 | 982713 | 0.162 |
| incr | valkey | 5 | 1088204 | 1077050 | 1102693 | 0.233 |
| llen | valkey | 5 | 1096229 | 1087637 | 1107185 | 0.225 |
| lpush_lpop | valkey | 5 | 954453 | 898679 | 979377 | 0.164 |
| mixed | valkey | 5 | 1061154 | 1056829 | 1065058 | 0.233 |
| ping | valkey | 5 | 1525439 | 1514647 | 1542343 | 0.157 |
| reads | valkey | 5 | 1121046 | 1111529 | 1135173 | 0.223 |
| rpush_rpop | valkey | 5 | 969645 | 954555 | 981511 | 0.162 |
| set_del | valkey | 5 | 989094 | 980352 | 995213 | 0.159 |
| writes | valkey | 5 | 1000958 | 992725 | 1006285 | 0.244 |

### Resident memory after preload

RSS includes the process and allocator/runtime overhead. Delta RSS is after-preload RSS minus idle RSS.

| Value shape | Server | Samples | Idle RSS KiB | RSS KiB | Delta RSS KiB | Bytes/key |
|---|---|---:|---:|---:|---:|---:|
| strings | memcached | 1 | 4888 | 18316 | 13428 | 137.50 |
| hashes | reddish | 1 | 3732 | 38060 | 34328 | 351.52 |
| lists | reddish | 1 | 3744 | 95116 | 91372 | 935.65 |
| strings | reddish | 1 | 3784 | 27088 | 23304 | 238.63 |
| hashes | redis | 1 | 18024 | 26348 | 8324 | 85.24 |
| lists | redis | 1 | 17988 | 33288 | 15300 | 156.67 |
| strings | redis | 1 | 18000 | 30744 | 12744 | 130.50 |
| hashes | valkey | 1 | 18136 | 26540 | 8404 | 86.06 |
| lists | valkey | 1 | 18156 | 27580 | 9424 | 96.50 |
| strings | valkey | 1 | 18060 | 30752 | 12692 | 129.97 |

<!-- generated-results:end -->
