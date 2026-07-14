#include <enet/enet.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/protocol.h"

#define PROBE_TIMEOUT_MS 10000u
#define SAMPLE_DURATION_MS 2000u

typedef struct SnapshotProbe {
  ENetHost* host;
  ENetPeer* peer;
  uint16_t expected_rate;
  uint32_t lobby_id;
  uint32_t player_id;
  uint32_t first_snapshot_ms;
  uint32_t last_snapshot_ms;
  uint32_t snapshot_count;
  uint64_t last_snapshot_tick;
  bool welcome_received;
  bool joined;
  bool failed;
} SnapshotProbe;

static bool ParseUint16(const char* text, uint16_t* value) {
  char* end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);
  if ((text == end) || (*end != '\0') || (parsed == 0u) || (parsed > UINT16_MAX)) {
    return false;
  }
  *value = (uint16_t)parsed;
  return true;
}

static bool SendReliable(SnapshotProbe* probe, const void* data, size_t size) {
  ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
  if ((packet == NULL) ||
      (enet_peer_send(probe->peer, SHROOM_ENET_CHANNEL_CONTROL, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  return true;
}

static bool SendHello(SnapshotProbe* probe) {
  ShroomHelloPacket packet = {.protocol_version = SHROOM_PROTOCOL_VERSION};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  snprintf(packet.name, sizeof(packet.name), "Snapshot Rate Probe");
  return SendReliable(probe, &packet, sizeof(packet));
}

static bool SendLobbyJoin(SnapshotProbe* probe) {
  ShroomLobbyJoinPacket packet = {.lobby_id = 1u};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN, sizeof(packet));
  return SendReliable(probe, &packet, sizeof(packet));
}

static bool SendReadyAndEnter(SnapshotProbe* probe) {
  ShroomReadyStatePacket ready = {.player_id = probe->player_id, .is_ready = 1u};
  ShroomEnterMatchPacket enter = {.lobby_id = probe->lobby_id};
  ShroomPacketHeaderInit(&ready.header, SHROOM_PACKET_READY_STATE, sizeof(ready));
  ShroomPacketHeaderInit(&enter.header, SHROOM_PACKET_ENTER_MATCH, sizeof(enter));
  return SendReliable(probe, &ready, sizeof(ready)) &&
         SendReliable(probe, &enter, sizeof(enter));
}

static bool ValidateCadence(const SnapshotProbe* probe, uint16_t tick_rate,
                            uint16_t snapshot_rate, const char* packet_name) {
  if ((tick_rate == 0u) || (snapshot_rate != probe->expected_rate)) {
    fprintf(stderr, "%s advertised tick=%u snapshot=%u, expected snapshot=%u\n", packet_name,
            tick_rate, snapshot_rate, probe->expected_rate);
    return false;
  }
  return true;
}

static bool HandlePacket(SnapshotProbe* probe, const ENetPacket* packet) {
  const ShroomPacketHeader* header;
  if (packet->dataLength < sizeof(*header)) {
    return false;
  }
  header = (const ShroomPacketHeader*)packet->data;
  switch ((ShroomPacketType)header->type) {
  case SHROOM_PACKET_WELCOME: {
    const ShroomWelcomePacket* welcome = (const ShroomWelcomePacket*)packet->data;
    if ((packet->dataLength < sizeof(*welcome)) ||
        !ValidateCadence(probe, welcome->server_tick_rate, welcome->snapshot_rate, "WELCOME")) {
      return false;
    }
    probe->welcome_received = true;
    return SendLobbyJoin(probe);
  }
  case SHROOM_PACKET_LOBBY_JOINED: {
    const ShroomLobbyJoinedPacket* joined = (const ShroomLobbyJoinedPacket*)packet->data;
    if ((packet->dataLength < sizeof(*joined)) ||
        !ValidateCadence(probe, joined->server_tick_rate, joined->snapshot_rate,
                         "LOBBY_JOINED")) {
      return false;
    }
    probe->lobby_id = joined->lobby_id;
    probe->player_id = joined->player_id;
    probe->joined = true;
    return SendReadyAndEnter(probe);
  }
  case SHROOM_PACKET_SNAPSHOT: {
    const ShroomSnapshotPacket* snapshot = (const ShroomSnapshotPacket*)packet->data;
    const uint32_t now = enet_time_get();
    if ((packet->dataLength < offsetof(ShroomSnapshotPacket, players)) ||
        ((probe->snapshot_count > 0u) && (snapshot->tick <= probe->last_snapshot_tick))) {
      break;
    }
    if (probe->snapshot_count == 0u) {
      probe->first_snapshot_ms = now;
    }
    probe->last_snapshot_tick = snapshot->tick;
    probe->last_snapshot_ms = now;
    probe->snapshot_count += 1u;
  } break;
  default:
    break;
  }
  return true;
}

static void Service(SnapshotProbe* probe, uint32_t wait_ms) {
  ENetEvent event;
  const int result = enet_host_service(probe->host, &event, wait_ms);
  if (result < 0) {
    probe->failed = true;
    return;
  }
  if (result == 0) {
    return;
  }
  if (event.type == ENET_EVENT_TYPE_CONNECT) {
    probe->failed = !SendHello(probe);
  } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
    probe->failed = !HandlePacket(probe, event.packet);
    enet_packet_destroy(event.packet);
  } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
    probe->failed = true;
  }
}

int main(int argc, char** argv) {
  SnapshotProbe probe = {0};
  ENetAddress address = {0};
  uint16_t port;
  uint16_t expected_rate;
  uint32_t deadline;
  uint32_t expected_count;
  int result = 1;

  if ((argc != 3) || !ParseUint16(argv[1], &port) || !ParseUint16(argv[2], &expected_rate)) {
    fprintf(stderr, "usage: %s PORT EXPECTED_RATE\n", argv[0]);
    return 2;
  }
  probe.expected_rate = expected_rate;
  address.port = port;
  if ((enet_initialize() != 0) || (enet_address_set_host(&address, "127.0.0.1") != 0)) {
    return 1;
  }
  probe.host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if (probe.host == NULL) {
    goto cleanup;
  }
  probe.peer = enet_host_connect(probe.host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (probe.peer == NULL) {
    goto cleanup;
  }

  deadline = enet_time_get() + PROBE_TIMEOUT_MS;
  while (!probe.failed &&
         ((probe.snapshot_count == 0u) ||
          ((int32_t)(enet_time_get() - probe.first_snapshot_ms) < (int32_t)SAMPLE_DURATION_MS)) &&
         ((int32_t)(deadline - enet_time_get()) > 0)) {
    Service(&probe, 5u);
  }
  expected_count = ((uint32_t)expected_rate * SAMPLE_DURATION_MS) / 1000u + 1u;
  if (!probe.failed && probe.welcome_received && probe.joined &&
      (probe.snapshot_count >= expected_count - 2u) &&
      (probe.snapshot_count <= expected_count + 2u)) {
    printf("snapshot cadence passed: advertised=%u observed=%u over %ums\n", expected_rate,
           probe.snapshot_count, SAMPLE_DURATION_MS);
    result = 0;
  } else {
    fprintf(stderr, "snapshot cadence failed: expected=%u observed=%u elapsed=%u\n",
            expected_count, probe.snapshot_count,
            probe.snapshot_count > 0u ? probe.last_snapshot_ms - probe.first_snapshot_ms : 0u);
  }

cleanup:
  if (probe.peer != NULL) {
    enet_peer_disconnect_now(probe.peer, 0u);
  }
  if (probe.host != NULL) {
    enet_host_destroy(probe.host);
  }
  enet_deinitialize();
  return result;
}
