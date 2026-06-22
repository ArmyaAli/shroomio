# Runtime timing instrumentation

Shroomio includes lightweight, opt-in timing instrumentation for profiling builds.

Enable it with:

```bash
SHROOM_PROFILE=1 ./dist/linux/client/shroomio
SHROOM_PROFILE=1 ./dist/linux/server/shroomio-server
```

When enabled, the client and server print CSV-like `profile,...` log lines roughly every five
seconds. When `SHROOM_PROFILE` is unset, `0`, `false`, or `FALSE`, timing collection is skipped for
normal builds.

## Client fields

Client samples use a rolling 120-sample window and reset peak values after each log line:

- `frame_avg_ms`, `frame_peak_ms`: measured game-screen update plus draw time.
- `update_avg_ms`, `update_peak_ms`: `GameUpdate` time.
- `draw_avg_ms`, `draw_peak_ms`: `GameDraw` time.
- `net_avg_ms`, `net_peak_ms`: time spent inside `ClientNetUpdate`.
- `snapshot_prediction_avg_ms`, `snapshot_prediction_peak_ms`: snapshot apply plus local prediction
  reconciliation work.

Example:

```text
profile,client,frame_avg_ms=1.812,frame_peak_ms=3.104,update_avg_ms=0.412,...
```

## Server fields

Server samples use a rolling 120-sample window and reset peak values after each log line:

- `tick_avg_ms`, `tick_peak_ms`: total server loop tick work before sleeping.
- `enet_avg_ms`, `enet_peak_ms`: ENet event processing time.
- `simulation_avg_ms`, `simulation_peak_ms`: world simulation time.
- `broadcast_avg_ms`, `broadcast_peak_ms`: snapshot, spore, and powerup broadcast time.

Example:

```text
profile,server,tick_avg_ms=0.731,tick_peak_ms=1.445,enet_avg_ms=0.060,...
```

These fields are intended as inputs for the benchmark harness and profiling runbook.
