# Production Runbook

This runbook covers one shroomio dedicated game-server process. Read
[Server Capacity and Deployment Specification](Server-Capacity.md) before sizing a public host.

## Runtime Characteristics

| Property | Value |
|---|---|
| Transport | ENet over UDP; optional CivetWeb HTTPS interface |
| Game port | `7777/udp` by default |
| REST port | `7443/tcp` when a TLS PEM is configured |
| Simulation | 30 Hz |
| Snapshots | 15 Hz by default; configurable from 15-20 Hz |
| Capacity | 256 connected clients across up to 8 lobbies |
| Persistence | SQLite accounts, profiles, and completed-match records |
| Shutdown | Graceful on `SIGINT` or `SIGTERM`; active matches reset |

CLI flags override environment variables, which override defaults:

| Setting | CLI | Environment | Default |
|---|---|---|---|
| Bind address | `--bind ADDRESS` | `SHROOM_SERVER_BIND` | `0.0.0.0` |
| Game port | `--port PORT` | `SHROOM_SERVER_PORT` | `7777` |
| Database | `--database PATH` | `SHROOM_SERVER_DB_PATH` | `shroomio.db` |
| REST bind address | `--rest-bind ADDRESS` | `SHROOM_SERVER_REST_BIND` | `0.0.0.0` |
| REST port | `--rest-port PORT` | `SHROOM_SERVER_REST_PORT` | `7443` |
| REST TLS PEM | `--rest-cert PATH` | `SHROOM_SERVER_REST_CERT` | Disabled |
| Snapshot rate | `--snapshot-rate HZ` | `SHROOM_SERVER_SNAPSHOT_RATE` | `15` |
| Directory port | `--directory-port PORT` | `SHROOM_DIRECTORY_PORT` | `7778` |

## Docker Deployment

Install the Linux vcpkg dependencies once, then build and start Compose:

```bash
make vcpkg-install-linux
docker compose up --build -d server
docker compose ps
docker compose logs -f server
```

Compose publishes `7777/udp` and `7443/tcp`, persists `/data/shroomio.db`, restarts on failure,
allows 15 seconds for graceful shutdown, and runs a protocol-level health probe every 30 seconds.
Back up the named `shroomio-server-data` volume. Override settings under
`services.server.environment`.

REST remains disabled until `SHROOM_SERVER_REST_CERT` names a readable combined PEM containing the
private key followed by the certificate chain. Mount the PEM read-only into the container; never
store it in the image, repository, or data volume. For example, add
`/etc/shroomio/rest.pem:/run/secrets/shroomio-rest.pem:ro` to the service volumes and set
`SHROOM_SERVER_REST_CERT=/run/secrets/shroomio-rest.pem`. Public deployments must use a certificate
issued for their hostname. A self-signed PEM is acceptable only for local testing with `curl -k`.

## systemd Deployment

Build the server and health-check binaries, create an unprivileged account, and install the supplied
unit:

```bash
make server-linux build/tools/shroomio-healthcheck
sudo useradd --system --home /var/lib/shroomio --shell /usr/sbin/nologin shroomio
sudo install -D -m 0755 dist/linux/server/shroomio-server /opt/shroomio/shroomio-server
sudo install -D -m 0755 build/tools/shroomio-healthcheck /opt/shroomio/shroomio-healthcheck
sudo install -D -m 0644 deploy/systemd/shroomio-server.service \
  /etc/systemd/system/shroomio-server.service
sudo install -D -m 0640 deploy/systemd/server.env.example /etc/shroomio/server.env
sudo systemctl daemon-reload
sudo systemctl enable --now shroomio-server
```

Edit `/etc/shroomio/server.env` before public launch. The unit creates `/var/lib/shroomio`, writes
logs to journald, restarts failures after five seconds, and applies basic service hardening. Configure
journald retention centrally, for example `SystemMaxUse=1G` and `MaxRetentionSec=14day`; do not add a
second file logger unless the host's log collector requires one.

## Network and Directory

Allow inbound UDP on the configured game port and TCP on the REST port, then forward both through
NAT. Point an optional DNS `A` record at the public host. Bind addresses remain local IPv4 addresses
such as `0.0.0.0`.

To operate the bounded directory service separately:

```bash
./shroomio-server --directory --bind 0.0.0.0 --directory-port 7778
```

Game servers advertise when `SHROOM_DIRECTORY_HOST`, `SHROOM_DIRECTORY_PORT`, and optionally
`SHROOM_SERVER_NAME` are set. Allow inbound `7778/udp` on the directory host. Registrations expire
15 seconds after their last heartbeat.

## Health and Monitoring

The health binary establishes an ENet connection and validates a server-probe response, including
protocol version, nonce, player count, and capacity:

```bash
./build/tools/shroomio-healthcheck --host 127.0.0.1 --port 7777 --timeout-ms 2000
# healthy players=12 capacity=256
```

Run it from outside the host as well to verify firewall/NAT reachability. The server also emits
60-second health logs containing accepted, stale, and rate-limited inputs, event-budget exhaustion,
and per-lobby player/bot/spectator counts. Collect process CPU/RSS, disk, UDP throughput, packet loss,
and queue depth according to [Server Capacity](Server-Capacity.md). The HTTPS readiness endpoint is
available separately when REST is enabled:

```bash
curl --fail --silent --show-error https://server.example:7443/health
# {"status":"ok","service":"shroomio-server"}
```

The endpoint contains no metrics or player data. REST access logs include method, path, status, and
remote address while recording request bodies only as `body=redacted`.

## Backup, Upgrade, and Restore

Stop the server before copying its SQLite database, or use SQLite's online `.backup` command:

```bash
sqlite3 /var/lib/shroomio/shroomio.db ".backup '/var/backups/shroomio.db'"
```

For upgrades, back up the database, replace the binary or image, start the service, run the UDP
health check, and inspect startup logs. Restore by stopping the service, replacing the database with
a verified backup owned by `shroomio`, then starting and probing again. The schema is pre-production;
review release notes before upgrading because incompatible releases may require a planned reset.

## Troubleshooting

**Health probe fails:** Confirm the process and UDP bind with `systemctl status shroomio-server` and
`ss -uln | grep 7777`, then check `journalctl -u shroomio-server`. Test the probe locally before
testing the public address.

**Clients cannot connect:** Verify UDP firewall and NAT rules, Docker's `7777:7777/udp` mapping, the
advertised hostname, and protocol-version compatibility. TCP port checks do not validate ENet.

**High latency or loss:** Check host egress, ENet packet loss, outgoing queue depth, CPU saturation,
and tick deadlines. Prefer the 15 Hz snapshot setting when bandwidth-constrained; reduce lobby
capacity if voice fanout saturates the link.

**Server will not start:** Check database-directory ownership, SQLite errors, an occupied UDP port,
the REST PEM path and permissions, occupied UDP/TCP ports, and dynamic libraries with
`ldd /opt/shroomio/shroomio-server`.
