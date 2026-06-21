# Benchmarking harness

Use the local benchmark harness to collect repeatable server scaling numbers before optimizing.

```bash
make benchmark
```

The default run builds `dist/shroomio-server`, then runs deterministic bot-only scenarios for
1, 8, and 32 simulated players over 600 ticks. Artifacts are written to `build/benchmarks/`:

- `summary.csv`: one row per scenario for branch-to-branch comparison.
- `server_bots_<players>.csv`: raw output from each server benchmark run.

Useful options:

```bash
python3 scripts/benchmark.py --ticks 1200 --players 1,8,32,64
python3 scripts/benchmark.py --server ./dist/shroomio-server --out-dir /tmp/shroomio-bench
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
