#ifndef SHROOM_NET_TELEMETRY_H
#define SHROOM_NET_TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

#define SHROOM_NET_TELEMETRY_MAX_PEERS SHROOM_SERVER_MAX_CLIENTS
#define SHROOM_NET_TELEMETRY_PACKET_TYPES ((size_t)SHROOM_PACKET_INTERMISSION_STATUS + 1u)
#define SHROOM_NET_TELEMETRY_BUCKET_COUNT 20u
#define SHROOM_NET_TELEMETRY_BUCKET_MS 100u
#define SHROOM_NET_TELEMETRY_WINDOW_MS 1000u
#define SHROOM_NET_TELEMETRY_CONGESTION_QUEUE_PACKETS 64u

typedef struct ShroomNetTrafficCount {
  uint64_t packets;
  uint64_t bytes;
} ShroomNetTrafficCount;

typedef struct ShroomNetTrafficStats {
  ShroomNetTrafficCount accepted;
  ShroomNetTrafficCount sent;
  ShroomNetTrafficCount dropped;
} ShroomNetTrafficStats;

typedef struct ShroomNetTelemetryBucket {
  uint64_t epoch;
  bool initialized;
  ShroomNetTrafficStats totals;
  ShroomNetTrafficStats by_channel[SHROOM_ENET_CHANNEL_COUNT];
  ShroomNetTrafficStats by_type[SHROOM_NET_TELEMETRY_PACKET_TYPES];
} ShroomNetTelemetryBucket;

typedef struct ShroomNetPeerTransport {
  uint32_t queue_packets;
  uint32_t queue_high_water;
  uint16_t loss_basis_points;
  bool active;
} ShroomNetPeerTransport;

typedef struct ShroomNetTelemetry {
  ShroomNetTrafficStats totals;
  ShroomNetTrafficStats by_peer[SHROOM_NET_TELEMETRY_MAX_PEERS];
  ShroomNetTrafficStats by_channel[SHROOM_ENET_CHANNEL_COUNT];
  ShroomNetTrafficStats by_type[SHROOM_NET_TELEMETRY_PACKET_TYPES];
  ShroomNetPeerTransport peer_transport[SHROOM_NET_TELEMETRY_MAX_PEERS];
  ShroomNetTelemetryBucket buckets[SHROOM_NET_TELEMETRY_BUCKET_COUNT];
} ShroomNetTelemetry;

typedef struct ShroomNetTelemetryWindow {
  ShroomNetTrafficStats totals;
  ShroomNetTrafficStats by_channel[SHROOM_ENET_CHANNEL_COUNT];
  ShroomNetTrafficStats by_type[SHROOM_NET_TELEMETRY_PACKET_TYPES];
  uint32_t queue_packets;
  uint32_t queue_high_water;
  uint16_t maximum_loss_basis_points;
  uint16_t congested_peers;
  uint16_t active_peers;
} ShroomNetTelemetryWindow;

void ShroomNetTelemetryReset(ShroomNetTelemetry* telemetry);
void ShroomNetTelemetryRecordAccepted(ShroomNetTelemetry* telemetry, size_t peer_index,
                                      uint8_t channel, ShroomPacketType type, size_t bytes,
                                      uint64_t now_ms);
void ShroomNetTelemetryRecordSent(ShroomNetTelemetry* telemetry, size_t peer_index, uint8_t channel,
                                  ShroomPacketType type, size_t bytes, uint64_t now_ms);
void ShroomNetTelemetryRecordDrop(ShroomNetTelemetry* telemetry, size_t peer_index, uint8_t channel,
                                  ShroomPacketType type, size_t bytes, uint64_t now_ms);
void ShroomNetTelemetrySetPeerTransport(ShroomNetTelemetry* telemetry, size_t peer_index,
                                        uint32_t queue_packets, uint16_t loss_basis_points,
                                        bool active);
void ShroomNetTelemetryReadWindow(const ShroomNetTelemetry* telemetry, uint64_t now_ms,
                                  uint32_t window_ms, ShroomNetTelemetryWindow* window);

#endif
