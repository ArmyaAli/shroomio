# Profiling runbook

Use this runbook when a benchmark, CI gate, or player report shows a performance problem. Capture
evidence first, then optimize.

## 1. Build profiling binaries

Prefer release binaries for realistic timing, then use debug tools only when you need symbols or
allocation detail.

```bash
make linux
make server
```

Optional runtime timing logs:

```bash
SHROOM_PROFILE=1 ./dist/linux/server/shroomio-server
SHROOM_PROFILE=1 ./dist/linux/client/shroomio
```

See `docs/Profiling-Instrumentation.md` for the `profile,...` fields.

## 2. Establish a repeatable baseline

Run the benchmark harness before and after a suspected change:

```bash
make benchmark
python3 scripts/benchmark.py --ticks 1200 --players 1,8,32,64
```

Attach `build/benchmarks/summary.csv` to optimization PRs/issues. Compare:

- `avg_tick_ms` and `worst_tick_ms`
- `estimated_packets` and `estimated_bytes`
- `harness_elapsed_ms` and `max_rss_kb`

## 3. CPU profiling

### Server CPU with `perf`

Run a server or benchmark under `perf`:

```bash
perf record -g -- ./dist/linux/server/shroomio-server --benchmark --benchmark-ticks 3000 --benchmark-bots 64
perf report
```

Look for hot paths in:

- `ShroomWorldStep`
- bot decision code in `src/shared/sim.c`
- snapshot serialization and send loops in `src/server/main.c`
- ENet event handling in `HandlePacket`

### Client CPU with `perf`

Start the client under `perf` while connected to a local server:

```bash
perf record -g -- ./dist/linux/client/shroomio
perf report
```

Use `SHROOM_PROFILE=1` alongside `perf` to correlate frame spikes with:

- `GameUpdate`
- `GameDraw`
- `ClientNetUpdate`
- snapshot apply / local prediction reconciliation

## 4. Frame-time spike workflow

1. Run the client with `SHROOM_PROFILE=1`.
2. Reproduce the spike.
3. Note the largest `frame_peak_ms`, `update_peak_ms`, and `draw_peak_ms` log lines.
4. If `update_peak_ms` is high, inspect simulation, network, snapshot, or prediction work.
5. If `draw_peak_ms` is high, inspect HUD, particles, player rendering, and culling.
6. Capture a `perf record -g` session around the same scenario.

## 5. Memory profiling

### Valgrind leak and invalid-access checks

Use the existing gate first:

```bash
make valgrind-test
```

For targeted server runs:

```bash
valgrind --leak-check=full --track-origins=yes ./dist/linux/server/shroomio-server --smoke-test \
  --bind 127.0.0.1 --port 37777 --database /tmp/shroomio-profile.db
```

### Massif heap profiling

Use Massif when RSS or allocation growth is suspicious:

```bash
valgrind --tool=massif --massif-out-file=build/massif-server.out \
  ./dist/linux/server/shroomio-server --benchmark --benchmark-ticks 3000 --benchmark-bots 64
ms_print build/massif-server.out
```

Investigate unexpected allocations in hot paths; gameplay and protocol code should generally use
fixed-size buffers.

## 6. Network profiling

Capture UDP traffic while running a local server/client pair:

```bash
sudo tcpdump -i lo udp port 7777 -w build/shroomio-local.pcap
```

Open the capture in Wireshark and inspect:

- packets per second
- bytes per second
- packet size distribution
- burstiness around snapshot and spore broadcasts

Correlate capture timing with server `profile,server,...` logs and benchmark `estimated_packets` /
`estimated_bytes` values.

## 7. Sharing results

When filing or closing a performance issue, include:

- branch/commit SHA
- platform and CPU model
- exact command(s) run
- `build/benchmarks/summary.csv` if applicable
- relevant `profile,...` log excerpts
- `perf report`, Massif output, or packet capture summaries
- a short conclusion identifying the likely hot path
