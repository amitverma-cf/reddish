# Measured results

The latest local WSL throughput measurement used source revision `7528187ba3a3a2deea6c87834095fa1e89585bd7`, 100,000 keys, 32-byte values, one worker, ten clients, pipeline 16, and three trials of 300,000 requests per client. Persistence was disabled for every server; server and client were bound to different physical cores.

| Workload | Reddish mean ops/s | Redis mean ops/s | Valkey mean ops/s |
|---|---:|---:|---:|
| GET | 334,980 | 375,316 | 322,436 |
| SET | 323,000 | 373,221 | 310,205 |
| Mixed | 310,402 | 377,662 | 314,973 |

WSL trial variance was large: Reddish GET ranged from 178,761 to 487,038 ops/s. These means do not establish a stable ranking, and Reddish did not beat Redis in this run. Earlier snapshot-based results are superseded and should not be used to describe the current WAL implementation.

Raw local data stays in the ignored local directory. For reproducible published throughput, memory samples, hashes, tool revisions, and profiles, download the artifact for the desired commit from [Benchmark artifacts](https://github.com/amitverma-cf/reddish/actions/workflows/benchmark.yml). A workflow run is a measurement only when it completes successfully; starting it does not establish a result.

See [benchmark setup and measurement definitions](README.md).
