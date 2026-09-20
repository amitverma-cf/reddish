# Reddish, Redis, Valkey, and Memcached benchmarks

The headline workloads are `reads` (100% GET), `writes` (100% SET), and `mixed` (50% GET, 50% SET). The full suite additionally exercises every implemented command family and compares RSS after equivalent string, list, and hash preloads. Memcached is a simple string-cache baseline: it participates only in GET, SET, and string RSS measurements.

This is a narrow loopback benchmark using 32-byte values, one memtier worker, ten clients, pipeline depth 16, and no timed persistence. RSS includes process and allocator overhead; it is useful for comparison, not an exact per-value allocation measurement.

<!-- generated-results:start -->
## Published results

Aggregated from 1 benchmark run directories.

### System and toolchain

- OS: Ubuntu 24.04
- Kernel: 6.17.0-1022-azure
- CPU: AMD EPYC 9V74 80-Core Processor (4 logical CPUs)
- Memory: 15990 MiB
- Compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0

### Command throughput

| Workload | Server | Samples | Average ops/s | Min ops/s | Max ops/s | Average p99 ms |
|---|---|---:|---:|---:|---:|---:|
| expire_ttl | reddish | 5 | 998569 | 975239 | 1008483 | 0.145 |
| hget | reddish | 5 | 991337 | 977795 | 1000273 | 0.231 |
| hlen | reddish | 5 | 1076446 | 1072236 | 1079821 | 0.213 |
| hset_hdel | reddish | 5 | 965139 | 955940 | 971508 | 0.151 |
| incr | reddish | 5 | 1062943 | 1053134 | 1073205 | 0.218 |
| llen | reddish | 5 | 1069899 | 1063458 | 1076957 | 0.212 |
| lpush_lpop | reddish | 5 | 962952 | 958143 | 967475 | 0.151 |
| mixed | reddish | 5 | 1017560 | 1008537 | 1023435 | 0.229 |
| ping | reddish | 5 | 1311188 | 1292810 | 1323873 | 0.178 |
| reads | reddish | 5 | 1072305 | 1071030 | 1073480 | 0.215 |
| rpush_rpop | reddish | 5 | 960550 | 955159 | 967475 | 0.151 |
| set_del | reddish | 5 | 993475 | 988158 | 1005664 | 0.149 |
| writes | reddish | 5 | 982872 | 978200 | 986963 | 0.239 |
| expire_ttl | redis | 5 | 1001080 | 996216 | 1004530 | 0.146 |
| hget | redis | 5 | 978907 | 975640 | 981271 | 0.239 |
| hlen | redis | 5 | 1050409 | 1029080 | 1063304 | 0.223 |
| hset_hdel | redis | 5 | 970517 | 962718 | 974935 | 0.167 |
| incr | redis | 5 | 1050199 | 1045822 | 1053581 | 0.225 |
| llen | redis | 5 | 1061010 | 1052648 | 1086638 | 0.221 |
| lpush_lpop | redis | 5 | 962342 | 960600 | 963349 | 0.159 |
| mixed | redis | 5 | 1015269 | 1013924 | 1016373 | 0.231 |
| ping | redis | 5 | 1292129 | 1276282 | 1300014 | 0.181 |
| reads | redis | 5 | 1063352 | 1053393 | 1071307 | 0.218 |
| rpush_rpop | redis | 5 | 966155 | 963086 | 971558 | 0.153 |
| set_del | redis | 5 | 981502 | 976706 | 985305 | 0.151 |
| writes | redis | 5 | 980979 | 978571 | 982615 | 0.234 |
| expire_ttl | valkey | 5 | 999762 | 990499 | 1010904 | 0.145 |
| hget | valkey | 5 | 978739 | 971712 | 982574 | 0.236 |
| hlen | valkey | 5 | 1056921 | 1051668 | 1062965 | 0.218 |
| hset_hdel | valkey | 5 | 965749 | 947133 | 976441 | 0.173 |
| incr | valkey | 5 | 1051582 | 1040287 | 1059645 | 0.226 |
| llen | valkey | 5 | 1059050 | 1053732 | 1063241 | 0.221 |
| lpush_lpop | valkey | 5 | 963180 | 959349 | 971949 | 0.162 |
| mixed | valkey | 5 | 1016112 | 1006677 | 1028152 | 0.231 |
| ping | valkey | 5 | 1308223 | 1283520 | 1360859 | 0.181 |
| reads | valkey | 5 | 1072640 | 1056372 | 1086736 | 0.215 |
| rpush_rpop | valkey | 5 | 965199 | 962690 | 966971 | 0.159 |
| set_del | valkey | 5 | 980706 | 973909 | 988953 | 0.151 |
| writes | valkey | 5 | 977748 | 955325 | 991887 | 0.239 |

### Resident memory after preload

RSS includes the process and allocator/runtime overhead. Delta RSS is after-preload RSS minus idle RSS.

| Value shape | Server | Samples | Idle RSS KiB | RSS KiB | Delta RSS KiB | Bytes/key |
|---|---|---:|---:|---:|---:|---:|
| hashes | reddish | 1 | 3748 | 38076 | 34328 | 351.52 |
| lists | reddish | 1 | 3728 | 95100 | 91372 | 935.65 |
| strings | reddish | 1 | 3776 | 27052 | 23276 | 238.35 |
| hashes | redis | 1 | 18056 | 26384 | 8328 | 85.28 |
| lists | redis | 1 | 18120 | 33460 | 15340 | 157.08 |
| strings | redis | 1 | 17996 | 30724 | 12728 | 130.33 |
| hashes | valkey | 1 | 18164 | 26504 | 8340 | 85.40 |
| lists | valkey | 1 | 18128 | 27640 | 9512 | 97.40 |
| strings | valkey | 1 | 18128 | 30756 | 12628 | 129.31 |

<!-- generated-results:end -->
