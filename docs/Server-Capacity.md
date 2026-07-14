# Server Capacity and Deployment Specification

This document defines the MVP production envelope for one authoritative shroomio server process.
The server runs a 30 Hz single-threaded simulation, ENet UDP transport, SQLite persistence, and an
unmixed voice relay. It does not encode or mix voice on the server.

## Recommended Host

| Resource | Minimum | Recommended production |
|---|---:|---:|
| CPU | 4 physical cores | 4 cores / 8 threads, modern x86_64 |
| RAM | 8 GB | 16 GB |
| Network | 250 Mbps symmetric | 1 Gbps symmetric |
| Storage | 10 GB SSD | 25 GB SSD with database backups |
| OS | Debian 12 | Debian 12 or Ubuntu 22.04 LTS |

The MVP sizing target is **256 connected participants per process**. Use a dedicated host or a
container with four CPU cores and 8 GB RAM reserved. The extra memory is for the OS, SQLite cache,
ENet queues, logs, and traffic bursts; it is not a claim that the fixed game state consumes gigabytes.

## Reference Measurements

Measurements were collected on 2026-07-14 under WSL2 on an AMD Ryzen 5 7600 (6 cores/12 threads).
The deterministic simulation run used 256 bots for 1,800 ticks:

```bash
python3 scripts/benchmark.py --server ./dist/linux/server/shroomio-server \
  --out-dir /tmp/issue-33-benchmark --ticks 1800 --players 256
```

It reported 0.439 ms average tick time, 1.088 ms worst tick time, and 13,592 KB maximum RSS. The
20 Hz loopback ENet run accepted 7,680 input packets/s and 5,343,500 snapshot payload bytes/s with
no application drops or deadline failures:

```bash
./build/benchmarks/network-benchmark --clients 256 --participants 256 \
  --split-pieces 1 --duration-ms 5000 --snapshot-rate 20 \
  --min-total-message-hz 12800 --max-deadline-failures 5
```

That global worst case is about 44.7 Mbps after adding accepted input payload, before UDP/ENet/IP
overhead. Voice is relayed only within a lobby and is limited to eight active talkers, 16 KB/s each.
Eight talkers relayed to 255 recipients can add roughly 267 Mbps of application payload. Real traffic
is lower because interest management, lobby boundaries, silence, and delta snapshots reduce fanout,
but the original 30 Mbps total budget is not a valid 256-player worst-case target. A 1 Gbps uplink
provides necessary headroom.

These are loopback component measurements, not a full production certification. They exclude WAN
loss, simultaneous SQLite/auth load, and combined gameplay plus voice. Run a native Linux staging
soak before raising a public server to 256 players.

## Launch Gates and Alerts

Run these gates on the release artifact:

```bash
make benchmark
make network-benchmark-test
make server-health-test
make valgrind-test
```

Require a 30-minute staging soak with representative voice and gameplay. Alert when the UDP health
probe fails twice, RSS exceeds 6 GB, disk exceeds 80%, process CPU exceeds 80% of its four-core quota,
or sustained egress exceeds 700 Mbps. Treat a 30 Hz tick over 16.7 ms as a warning and over 33.3 ms
as a missed deadline. Also monitor `input_rate_limited`, `event_budget_exhaustions`, packet loss,
outgoing queue depth, connected players, and per-lobby occupancy from server logs and host metrics.

Scale horizontally with additional advertised server processes before reducing tick or snapshot rate.
If a host is CPU-bound, profile simulation and snapshot serialization; if network-bound, reduce voice
fanout or lobby capacity and verify interest-management effectiveness.
