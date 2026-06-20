# Redis, Valkey, and reddish benchmarks

The default workloads are `reads` (100% GET), `writes` (100% SET), and `mixed` (50% GET, 50% SET).

This is a narrow loopback benchmark using 32-byte values, one memtier worker, ten clients, pipeline depth 16, and no timed persistence.

<!-- generated-results:start -->
## Published results

### System and toolchain

- OS: Debian GNU/Linux 13
- Kernel: 6.6.87.2-microsoft-standard-WSL2
- CPU: AMD Ryzen 7 5700U with Radeon Graphics (16 logical CPUs)
- Memory: 7604 MiB
- Compiler: c++ (Debian 14.2.0-19) 14.2.0

| Workload | Server | Samples | Average ops/s | Min ops/s | Max ops/s | Average p99 ms |
|---|---|---:|---:|---:|---:|---:|
| mixed | reddish | 10 | 473979 | 459805 | 489691 | 0.991 |
| reads | reddish | 10 | 504320 | 471087 | 550139 | 0.949 |
| writes | reddish | 10 | 453750 | 436928 | 466272 | 1.016 |
| mixed | redis | 10 | 452830 | 437157 | 472584 | 1.026 |
| reads | redis | 10 | 445933 | 433620 | 462252 | 0.973 |
| writes | redis | 10 | 432564 | 422995 | 444896 | 1.063 |
| mixed | valkey | 10 | 441026 | 426455 | 454771 | 1.026 |
| reads | valkey | 10 | 471577 | 449406 | 480743 | 0.977 |
| writes | valkey | 10 | 403523 | 391480 | 411223 | 1.077 |

<!-- generated-results:end -->
