#include "net_telemetry.h"

#include <limits.h>
#include <string.h>

typedef enum ShroomNetTrafficDirection {
  SHROOM_NET_TRAFFIC_ACCEPTED = 0,
  SHROOM_NET_TRAFFIC_SENT,
  SHROOM_NET_TRAFFIC_DROPPED,
} ShroomNetTrafficDirection;

static size_t TypeIndex(ShroomPacketType type) {
  const size_t index = (size_t)type;
  return index < SHROOM_NET_TELEMETRY_PACKET_TYPES ? index : 0u;
}

static void AddCount(ShroomNetTrafficCount* count, size_t bytes) {
  count->packets += 1u;
  count->bytes += (uint64_t)bytes;
}

static ShroomNetTrafficCount* SelectCount(ShroomNetTrafficStats* stats,
                                          ShroomNetTrafficDirection direction) {
  switch (direction) {
  case SHROOM_NET_TRAFFIC_SENT:
    return &stats->sent;
  case SHROOM_NET_TRAFFIC_DROPPED:
    return &stats->dropped;
  case SHROOM_NET_TRAFFIC_ACCEPTED:
  default:
    return &stats->accepted;
  }
}

static ShroomNetTelemetryBucket* CurrentBucket(ShroomNetTelemetry* telemetry, uint64_t now_ms) {
  const uint64_t epoch = now_ms / SHROOM_NET_TELEMETRY_BUCKET_MS;
  ShroomNetTelemetryBucket* bucket = &telemetry->buckets[epoch % SHROOM_NET_TELEMETRY_BUCKET_COUNT];

  if (!bucket->initialized || (bucket->epoch != epoch)) {
    *bucket = (ShroomNetTelemetryBucket){.epoch = epoch, .initialized = true};
  }
  return bucket;
}

static void Record(ShroomNetTelemetry* telemetry, size_t peer_index, uint8_t channel,
                   ShroomPacketType type, size_t bytes, uint64_t now_ms,
                   ShroomNetTrafficDirection direction) {
  ShroomNetTelemetryBucket* bucket;
  const size_t type_index = TypeIndex(type);

  if (telemetry == NULL) {
    return;
  }
  bucket = CurrentBucket(telemetry, now_ms);
  AddCount(SelectCount(&telemetry->totals, direction), bytes);
  AddCount(SelectCount(&bucket->totals, direction), bytes);
  if (peer_index < SHROOM_NET_TELEMETRY_MAX_PEERS) {
    AddCount(SelectCount(&telemetry->by_peer[peer_index], direction), bytes);
  }
  if (channel < SHROOM_ENET_CHANNEL_COUNT) {
    AddCount(SelectCount(&telemetry->by_channel[channel], direction), bytes);
    AddCount(SelectCount(&bucket->by_channel[channel], direction), bytes);
  }
  AddCount(SelectCount(&telemetry->by_type[type_index], direction), bytes);
  AddCount(SelectCount(&bucket->by_type[type_index], direction), bytes);
}

static void AddTraffic(ShroomNetTrafficCount* destination, const ShroomNetTrafficCount* source) {
  destination->packets += source->packets;
  destination->bytes += source->bytes;
}

static void AddStats(ShroomNetTrafficStats* destination, const ShroomNetTrafficStats* source) {
  AddTraffic(&destination->accepted, &source->accepted);
  AddTraffic(&destination->sent, &source->sent);
  AddTraffic(&destination->dropped, &source->dropped);
}

void ShroomNetTelemetryReset(ShroomNetTelemetry* telemetry) {
  if (telemetry != NULL) {
    memset(telemetry, 0, sizeof(*telemetry));
  }
}

void ShroomNetTelemetryRecordAccepted(ShroomNetTelemetry* telemetry, size_t peer_index,
                                      uint8_t channel, ShroomPacketType type, size_t bytes,
                                      uint64_t now_ms) {
  Record(telemetry, peer_index, channel, type, bytes, now_ms, SHROOM_NET_TRAFFIC_ACCEPTED);
}

void ShroomNetTelemetryRecordSent(ShroomNetTelemetry* telemetry, size_t peer_index, uint8_t channel,
                                  ShroomPacketType type, size_t bytes, uint64_t now_ms) {
  Record(telemetry, peer_index, channel, type, bytes, now_ms, SHROOM_NET_TRAFFIC_SENT);
}

void ShroomNetTelemetryRecordDrop(ShroomNetTelemetry* telemetry, size_t peer_index, uint8_t channel,
                                  ShroomPacketType type, size_t bytes, uint64_t now_ms) {
  Record(telemetry, peer_index, channel, type, bytes, now_ms, SHROOM_NET_TRAFFIC_DROPPED);
}

void ShroomNetTelemetrySetPeerTransport(ShroomNetTelemetry* telemetry, size_t peer_index,
                                        uint32_t queue_packets, uint16_t loss_basis_points,
                                        bool active) {
  ShroomNetPeerTransport* peer;

  if ((telemetry == NULL) || (peer_index >= SHROOM_NET_TELEMETRY_MAX_PEERS)) {
    return;
  }
  peer = &telemetry->peer_transport[peer_index];
  if (!active) {
    *peer = (ShroomNetPeerTransport){0};
    return;
  }
  if (!peer->active) {
    peer->queue_high_water = 0u;
  }
  peer->queue_packets = queue_packets;
  if (queue_packets > peer->queue_high_water) {
    peer->queue_high_water = queue_packets;
  }
  peer->loss_basis_points = loss_basis_points > 10000u ? 10000u : loss_basis_points;
  peer->active = true;
}

void ShroomNetTelemetryReadWindow(const ShroomNetTelemetry* telemetry, uint64_t now_ms,
                                  uint32_t window_ms, ShroomNetTelemetryWindow* window) {
  uint64_t bucket_count;
  const uint64_t current_epoch = now_ms / SHROOM_NET_TELEMETRY_BUCKET_MS;

  if (window == NULL) {
    return;
  }
  *window = (ShroomNetTelemetryWindow){0};
  if (telemetry == NULL) {
    return;
  }
  bucket_count =
      ((uint64_t)window_ms + SHROOM_NET_TELEMETRY_BUCKET_MS - 1u) / SHROOM_NET_TELEMETRY_BUCKET_MS;
  if (bucket_count > SHROOM_NET_TELEMETRY_BUCKET_COUNT) {
    bucket_count = SHROOM_NET_TELEMETRY_BUCKET_COUNT;
  }
  for (size_t index = 0u; index < SHROOM_NET_TELEMETRY_BUCKET_COUNT; ++index) {
    const ShroomNetTelemetryBucket* bucket = &telemetry->buckets[index];
    if (!bucket->initialized || (bucket->epoch > current_epoch) ||
        ((current_epoch - bucket->epoch) >= bucket_count)) {
      continue;
    }
    AddStats(&window->totals, &bucket->totals);
    for (size_t channel = 0u; channel < SHROOM_ENET_CHANNEL_COUNT; ++channel) {
      AddStats(&window->by_channel[channel], &bucket->by_channel[channel]);
    }
    for (size_t type = 0u; type < SHROOM_NET_TELEMETRY_PACKET_TYPES; ++type) {
      AddStats(&window->by_type[type], &bucket->by_type[type]);
    }
  }
  for (size_t index = 0u; index < SHROOM_NET_TELEMETRY_MAX_PEERS; ++index) {
    const ShroomNetPeerTransport* peer = &telemetry->peer_transport[index];
    if (!peer->active) {
      continue;
    }
    ++window->active_peers;
    if (UINT32_MAX - window->queue_packets < peer->queue_packets) {
      window->queue_packets = UINT32_MAX;
    } else {
      window->queue_packets += peer->queue_packets;
    }
    if (UINT32_MAX - window->queue_high_water < peer->queue_high_water) {
      window->queue_high_water = UINT32_MAX;
    } else {
      window->queue_high_water += peer->queue_high_water;
    }
    if (peer->loss_basis_points > window->maximum_loss_basis_points) {
      window->maximum_loss_basis_points = peer->loss_basis_points;
    }
    if (peer->queue_packets >= SHROOM_NET_TELEMETRY_CONGESTION_QUEUE_PACKETS) {
      ++window->congested_peers;
    }
  }
}
