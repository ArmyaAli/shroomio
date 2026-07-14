# Production Runbook

This document covers deploying and operating the shroomio dedicated server in production or self-hosted community environments.

## Quickstart

The fastest way to expose a shroomio server on the internet:

```bash
# 1. Clone and build
git clone git@github.com:ArmyaAli/shroomio.git
cd shroomio
make server-linux

# 2. Run (UDP 7777)
./dist/linux/server/shroomio-server

# 3. Open UDP port 7777 in your firewall/router
# 4. Players connect via: <your-public-ip>:7777
```

For Docker:
```bash
docker run -d --name shroomio-server -p 7777:7777/udp --restart unless-stopped shroomio-server:dev
```

See below for full configuration, systemd setup, and troubleshooting.

## Overview

The shroomio server is a headless C binary that runs the game simulation authoritatively and communicates with clients over UDP via ENet. A stock shroomio client can connect to any public server by domain name or IP address when the server's UDP port is reachable from the internet.

### Server Characteristics

| Property | Value |
|----------|-------|
| Protocol | UDP |
| Default port | 7777 |
| Tick rate | 30 Hz (simulation) |
| Snapshot rate | 15 Hz (client updates, every 2nd tick) |
| Max clients | 128 |
| Max simulation players | 128 (18 bots + up to 110 humans) |
| Memory usage | ~3 MB (fixed-size arrays, no dynamic allocation) |
| CPU usage | Very low (simple simulation, no graphics) |

## Deployment

### Release Assets

GitHub Releases include one downloadable bundle per platform with both client and server binaries. Operators who do not want to build from source should download the matching platform asset for their host:

| Host | Release asset |
|---|---|
| Linux x64 | `shroomio-<version>-linux-x64.tar.gz` |
| Windows x64 | `shroomio-<version>-windows-x64.zip` |
| macOS x64 | `shroomio-<version>-macos-x64.tar.gz` |

Each release also includes `SHA256SUMS-v<version>.txt` for verifying downloaded assets.
Release archives preserve a platform-first layout, such as `linux/client/shroomio` and
`linux/server/shroomio-server`.

### Runtime Configuration

The server is self-hostable without recompiling. CLI flags override environment variables; environment variables override defaults.

| Setting | CLI | Environment | Default |
|---|---|---|---|
| Bind address | `--bind ADDRESS` | `SHROOM_SERVER_BIND` | `0.0.0.0` |
| UDP port | `--port PORT` | `SHROOM_SERVER_PORT` | `7777` |
| SQLite database | `--database PATH` | `SHROOM_SERVER_DB_PATH` | `shroomio.db` |
| Directory UDP port | `--directory-port PORT` | `SHROOM_DIRECTORY_PORT` | `7778` |

Examples:

```bash
./dist/linux/server/shroomio-server --bind 0.0.0.0 --port 7777 --database ./shroomio.db
SHROOM_SERVER_PORT=9000 ./dist/linux/server/shroomio-server
```

### Server Directory

Run the bounded directory service on its own UDP port. It stores at most 32 live registrations in
memory and expires a game server 15 seconds after its last heartbeat:

```bash
./dist/linux/server/shroomio-server --directory --bind 0.0.0.0 --directory-port 7778
```

Configure each public game server to advertise every five seconds:

```bash
SHROOM_DIRECTORY_HOST=directory.example.com \
SHROOM_DIRECTORY_PORT=7778 \
SHROOM_SERVER_PUBLIC_HOST=game-1.example.com \
SHROOM_SERVER_ID=1001 \
SHROOM_SERVER_NAME="East Arena" \
./dist/linux/server/shroomio-server --port 7777 --database ./shroomio.db
```

`SHROOM_SERVER_ID` must be a stable nonzero integer for that deployment. When omitted, the server
derives it from the advertised host and game port. Open the directory port to clients and game
servers; the game port remains separate. If `SHROOM_DIRECTORY_HOST` is absent, the server runs
normally without advertising and clients report that no directory is configured.

### Docker (Recommended)

**Build the image:**

```bash
make docker-server
```

This produces the image `shroomio-server:dev`.

**Run the container:**

```bash
make docker-run-server
```

Or manually:

```bash
docker run --rm -p 7777:7777/udp shroomio-server:dev
```

Persist server data and configure it explicitly:

```bash
docker run -d --name shroomio-server \
  -p 7777:7777/udp \
  -e SHROOM_SERVER_BIND=0.0.0.0 \
  -e SHROOM_SERVER_PORT=7777 \
  -e SHROOM_SERVER_DB_PATH=/data/shroomio.db \
  -v shroomio-server-data:/data \
  --restart unless-stopped \
  shroomio-server:dev
```

### Docker Compose

```bash
docker compose up --build server
```

The `compose.yaml` maps host UDP port 7777 to the container and stores server data in the `shroomio-server-data` volume.

### Bare Metal / VM

```bash
# Install dependencies
sudo apt install build-essential

# Clone and build
git clone git@github.com:ArmyaAli/shroomio.git
cd shroomio
make server-linux

# Run
./dist/linux/server/shroomio-server

# Run on a custom UDP port
./dist/linux/server/shroomio-server --port 9000 --database /var/lib/shroomio/shroomio.db
```

## Hosting Requirements

### Network

- **Port**: UDP 7777 must be open to the internet (or to your player network).
- **Firewall**: allow inbound UDP on port 7777 from player IP ranges.
- **NAT/port forwarding**: forward the public UDP port to the host running `shroomio-server`.
- **DNS**: optional. Point an `A` or `AAAA` record such as `play.example.com` at the server's public address, then clients can connect to that hostname. The server's `--bind` value is still a local IP address such as `0.0.0.0` or `127.0.0.1`, not the public DNS name.
- **Bandwidth**: approximately 5–10 KB/s per connected client at 15 Hz snapshot rate (depends on player count in snapshots).
- **Latency**: players will experience jitter proportional to round-trip time. Target <50 ms RTT for LAN play, <100 ms for internet play.

### Compute

- **CPU**: any modern x86_64 CPU. The simulation is single-threaded and lightweight.
- **RAM**: ~64 MB minimum (process uses ~3 MB, overhead for OS and ENet buffers).
- **Disk**: ~5 MB for the binary; no persistent storage required.
- **OS**: Linux (Debian/Ubuntu recommended). The binary links dynamically against glibc and libm only.

### Availability

- The server has no built-in persistence, graceful shutdown, or state recovery. A restart resets the world with fresh bots.
- For minimal downtime, run behind a process supervisor (systemd, Docker restart policy, or a container orchestrator).

## Operational Procedures

### Starting the Server

```bash
# Docker
docker run -d --name shroomio-server -p 7777:7777/udp --restart unless-stopped shroomio-server:dev

# Bare metal (with systemd — see systemd unit section below)
systemctl start shroomio-server
```

### Stopping the Server

The server responds to `SIGINT` and `SIGTERM` and shuts down cleanly:

```bash
# Docker
docker stop shroomio-server

# Bare metal
kill -SIGTERM $(pgrep shroomio-server)
# or
systemctl stop shroomio-server
```

### Health Check

The server prints to stdout on startup:

```
shroomio server listening on 0.0.0.0:7777/udp
```

Verify the server is accepting connections:

```bash
# Check if UDP port is open (server-side)
ss -uln | grep 7777

# Send a test connection from a client:
# use Server Browser -> direct connect -> <server-ip-or-domain>:7777
./dist/linux/client/shroomio
```

No built-in HTTP health endpoint exists. For monitoring, use:
- Docker: `docker ps` to confirm container is running.
- Process: `pgrep shroomio-server` to confirm process exists.
- Logs: check stdout for peer connect/disconnect events.

### Logs

```bash
# Docker
docker logs -f shroomio-server

# systemd
journalctl -u shroomio-server -f

# Bare metal
# Server outputs to stdout; redirect to file if needed:
./dist/linux/server/shroomio-server >> /var/log/shroomio-server.log 2>&1
```

The server logs:
- Peer connect: `peer connected: slot=<N>`
- Peer disconnect: `peer disconnected: slot=<N>`
- Shutdown: `shroomio server shutting down`

### systemd Unit File

Create `/etc/systemd/system/shroomio-server.service`:

```ini
[Unit]
Description=shroomio Dedicated Server
After=network.target

[Service]
Type=simple
ExecStart=/opt/shroomio/dist/linux/server/shroomio-server --bind 0.0.0.0 --port 7777 --database /opt/shroomio/shroomio.db
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal
User=shroomio
Group=shroomio
WorkingDirectory=/opt/shroomio

# Security hardening
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/opt/shroomio
PrivateTmp=yes
RestrictAddressFamilies=AF_INET AF_INET6
RestrictRealtime=yes
MemoryDenyWriteExecute=yes
SystemCallFilter=@system-service

[Install]
WantedBy=multi-user.target
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now shroomio-server
```

## Monitoring and Alerting

### What to Monitor

| Signal | Healthy | Action if unhealthy |
|--------|---------|---------------------|
| Process running | Process exists | Restart via supervisor |
| Port bound | UDP 7777 in LISTEN state | Check network, restart |
| Memory usage | <10 MB RSS | Restart if leaking (unlikely) |
| CPU usage | <5% of one core | Investigate if sustained >50% |
| Client connections | Peer connect events in logs | If zero, check firewall/DNS |

### Simple Health Check Script

```bash
#!/bin/bash
# /opt/shroomio/healthcheck.sh
if ! pgrep -x shroomio-server > /dev/null; then
    echo "CRITICAL: shroomio-server not running"
    exit 2
fi
if ! ss -uln | grep -q 7777; then
    echo "WARNING: UDP port 7777 not bound"
    exit 1
fi
echo "OK: shroomio-server running and port 7777 bound"
exit 0
```

## Troubleshooting

### Client cannot connect

1. **Check firewall**: ensure UDP 7777 is open.
2. **Check binding**: `ss -uln | grep 7777` — should show `0.0.0.0:7777`.
3. **Check Docker port mapping**: `docker port shroomio-server` should show `7777/udp`.
4. **Check client address**: ensure the client is using the correct IP/hostname.
5. **Check server logs**: look for peer connect events.

### Server crashes on startup

1. Check that `dist/linux/server/shroomio-server` exists: `ls -la dist/linux/server/`.
2. Ensure port 7777 is not already in use: `ss -uln | grep 7777`.
3. Check system library availability: `ldd dist/linux/server/shroomio-server`.

### High latency or jitter

- Players will experience rough movement without client-side interpolation (see roadmap milestone 3).
- Reduce tick/snapshot rates by adjusting `SHROOM_SERVER_TICK_RATE` in `config.h` and `SHROOM_SNAPSHOT_RATE` in `protocol.h`, then rebuild.
- Verify server network throughput is adequate for player count.

### Server not logging anything

- The server uses unbuffered stdout/stderr (`setvbuf` with `_IONBF`). Output should appear immediately.
- For Docker: `docker logs -f shroomio-server 2>&1`.
- For systemd: `journalctl -u shroomio-server -f`.

## Upgrading

1. Pull latest code: `git pull origin main`
2. Rebuild: `make server-linux` (or `make docker-server` for Docker)
3. Stop old server
4. Deploy new binary/image
5. Start new server

No migration or data preservation is needed — the server has no persistent state.

## Backup and Restore

Not applicable. The server is fully stateless. A restart creates a fresh world with 18 bots.
