# Benchmarking harness

Use the local benchmark harness to collect repeatable server scaling numbers before optimizing.

```bash
make benchmark
```

The default run builds `dist/linux/server/shroomio-server`, then runs deterministic bot-only scenarios for
1, 8, and 32 simulated players over 600 ticks. Artifacts are written to `build/benchmarks/`:

- `summary.csv`: one row per scenario for branch-to-branch comparison.
- `server_bots_<players>.csv`: raw output from each server benchmark run.

Useful options:

```bash
python3 scripts/benchmark.py --ticks 1200 --players 1,8,32,64
python3 scripts/benchmark.py --server ./dist/linux/server/shroomio-server --out-dir /tmp/shroomio-bench
```

## Output fields

- `scenario`: benchmark scenario name.
- `players`: simulated bot/player count.
- `ticks`: simulated server ticks.
- `elapsed_ms`: elapsed benchmark time reported by the server benchmark mode.
- `avg_tick_ms`, `worst_tick_ms`: average and worst simulation tick time.
- `estimated_packets`, `estimated_bytes`: estimated snapshot/spore/powerup packet volume.
- `rtt_ms`: reserved for future live-client benchmark scenarios; bot-only mode reports `0`.
- `cpu_time_ms`: server benchmark CPU-time proxy for the scenario.
- `memory_kb`, `max_rss_kb`: memory fields for local comparison. `max_rss_kb` is recorded by the
  Python harness from child process resource usage.
- `harness_elapsed_ms`: wall-clock time measured by the Python harness.

## Comparing branches

Run the same command on both branches and compare `summary.csv` values. Attach the CSV artifacts to
performance PRs/issues when making optimization claims.

## Real ENet loopback benchmark

Use the networking harness when evaluating packet throughput or queue pressure:

```bash
make network-benchmark
make network-benchmark NETWORK_BENCH_CLIENTS=1,64 NETWORK_BENCH_DURATION_MS=5000
make network-benchmark NETWORK_BENCH_CLIENTS=256 NETWORK_BENCH_SPLIT_PIECES=4
```

The default run creates real ENet loopback connections for 1, 64, and 256 clients. It writes raw
scenario CSVs and `summary.csv` under `build/benchmarks/network/`. The harness sends client input at
30 Hz and server snapshots at 15 Hz, then reports messages/s and payload bytes/s from packets actually
accepted by the opposite ENet host. It also reports rejected sends/packets, outgoing queue high-water,
network-loop tick deadline failures, and byte reduction against encoding every frame as a keyframe.
The gate fails unless production delta traffic is smaller than that in-run keyframe-only control. An
intentionally impossible threshold is run last to verify that threshold failures return a nonzero status.

Default pass thresholds are at least 20 accepted inputs/client/s, 10 accepted snapshots/client/s,
at most 1% application-level drops, and at most five 33.3 ms network-loop deadline failures. Override
them through `scripts/network_benchmark.py` when a slower reference role has documented limits. These
are regression gates, not internet latency or packet-loss claims; loopback cannot model either.

### Scale assumptions

- One process owns both ENet hosts to isolate serialization, delivery, dispatch, and queue cost.
- Every client is an active participant, including the canonical 256-client scenario.
- Every connected client receives snapshots. Snapshot payloads contain
  `participants * split_pieces` entities, bounded by 256 participants and four pieces each.
- Inputs and snapshots use their production unreliable channels and production wire structs.
  Player frames use component-masked keyframe/delta records in unreliable-unsequenced chunks.
- `make network-benchmark-test` runs 256 active clients at 30 Hz input against both 15 Hz and 20 Hz
  snapshot schedules. The 20 Hz scenario must schedule at least 12,800 logical input/snapshot
  messages/s, while both scenarios enforce accepted-packet, congestion, and tick-deadline limits.
  Unit coverage separately verifies maximum 1,024-entity snapshot chunking stays within the
  1,200-byte application MTU budget.
- Simulation, SQLite, rendering, authentication, and WAN behavior are intentionally excluded. Use
  `make benchmark` and production profiling separately for those costs.

Record the execution role with every attached CSV:

```bash
uname -srm
lscpu | sed -n '1,18p'
```

The initial reference run used WSL2 Linux x86_64 on an AMD Ryzen 5 7600 (6 cores/12 threads). Compare
branches on the same hardware, power profile, client counts, duration, and split-piece setting.
