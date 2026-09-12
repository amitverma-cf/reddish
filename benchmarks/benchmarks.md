# Redis, Valkey, and reddish benchmarks

The default workloads are `reads` (100% GET), `writes` (100% SET), and `mixed` (50% GET, 50% SET).

This is a narrow loopback benchmark using 32-byte values, one memtier worker, ten clients, pipeline depth 16, and no timed persistence.

<!-- generated-results:start -->
## Published results

Aggregated from 1 local run directories.

### System and toolchain

- OS: Ubuntu 24.04
- Kernel: 6.17.0-1022-azure
- CPU: Intel(R) Xeon(R) 6973P-C (4 logical CPUs)
- Memory: 15989 MiB
- Compiler: c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0

| Workload | Server | Samples | Average ops/s | Min ops/s | Max ops/s | Average p99 ms |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 5 | 1013665 | 995442 | 1039477 | 0.250 |
| reads | reddish | 5 | 1047998 | 991632 | 1078783 | 0.247 |
| writes | reddish | 5 | 1168995 | 1133068 | 1209807 | 0.241 |
| mixed | redis | 5 | 1403914 | 1393676 | 1415246 | 0.175 |
| reads | redis | 5 | 1456393 | 1444394 | 1471974 | 0.167 |
| writes | redis | 5 | 1311833 | 1305396 | 1318980 | 0.183 |
| mixed | valkey | 5 | 1411307 | 1398253 | 1427062 | 0.173 |
| reads | valkey | 5 | 1495485 | 1478506 | 1513892 | 0.161 |
| writes | valkey | 5 | 1326952 | 1312802 | 1339846 | 0.181 |

<!-- generated-results:end -->
