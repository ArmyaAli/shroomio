#include <enet/enet.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "shared/protocol.h"
#include "shared/snapshot_replication.h"

#define FLOOD_PACKET_COUNT 5000u
#define TEST_TIMEOUT_MS 10000u
#define RECOVERY_WAIT_MS 40u
#define RECOVERY_ACK_TIMEOUT_MS 600u

typedef struct FloodClient {
  ENetHost* host;
  ENetPeer* peer;
  uint32_t player_id;
  uint32_t lobby_id;
  uint32_t last_acknowledged_sequence;
  uint32_t pong_nonce;
  float player_x;
  ShroomSnapshotAssembly snapshot_assembly;
  ShroomSnapshotHistory snapshot_history;
  ShroomSnapshotPlayerState snapshot_players[SHROOM_MAX_SNAPSHOT_PLAYERS];
  bool joined;
  bool snapshot_received;
  bool player_position_received;
  bool disconnected;
} FloodClient;

static void SleepMillis(uint32_t milliseconds) {
  struct timespec duration = {
      .tv_sec = (time_t)(milliseconds / 1000u),
      .tv_nsec = (long)(milliseconds % 1000u) * 1000000l,
  };
  nanosleep(&duration, NULL);
}

static bool SendPacket(FloodClient* client, uint8_t channel, const void* data, size_t size) {
  ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
  if ((packet == NULL) || (enet_peer_send(client->peer, channel, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  return true;
}

static bool SendHello(FloodClient* client) {
  ShroomHelloPacket packet = {.protocol_version = SHROOM_PROTOCOL_VERSION};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  snprintf(packet.name, sizeof(packet.name), "Input Flood Test");
  return SendPacket(client, SHROOM_ENET_CHANNEL_CONTROL, &packet, sizeof(packet));
}

static bool SendLobbyJoin(FloodClient* client) {
  ShroomLobbyJoinPacket packet = {.lobby_id = 1u};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN, sizeof(packet));
  return SendPacket(client, SHROOM_ENET_CHANNEL_CONTROL, &packet, sizeof(packet));
}

static bool SendReadyAndEnter(FloodClient* client) {
  ShroomReadyStatePacket ready = {.player_id = client->player_id, .is_ready = 1u};
  ShroomEnterMatchPacket enter = {.lobby_id = client->lobby_id};
  ShroomPacketHeaderInit(&ready.header, SHROOM_PACKET_READY_STATE, sizeof(ready));
  ShroomPacketHeaderInit(&enter.header, SHROOM_PACKET_ENTER_MATCH, sizeof(enter));
  return SendPacket(client, SHROOM_ENET_CHANNEL_CONTROL, &ready, sizeof(ready)) &&
         SendPacket(client, SHROOM_ENET_CHANNEL_CONTROL, &enter, sizeof(enter));
}

static bool SendInput(FloodClient* client, uint32_t sequence, float direction_x) {
  ShroomInputPacket packet = {.sequence = sequence, .direction_x = direction_x};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_INPUT, sizeof(packet));
  return SendPacket(client, SHROOM_ENET_CHANNEL_INPUT, &packet, sizeof(packet));
}

static bool SendPing(FloodClient* client, uint32_t nonce) {
  ShroomPingPacket packet = {.nonce = nonce};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_PING, sizeof(packet));
  return SendPacket(client, SHROOM_ENET_CHANNEL_CONTROL, &packet, sizeof(packet));
}

static bool SendSnapshotAck(FloodClient* client, uint64_t tick) {
  ShroomSnapshotAckPacket packet = {.tick = tick};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_SNAPSHOT_ACK, sizeof(packet));
  return SendPacket(client, SHROOM_ENET_CHANNEL_INPUT, &packet, sizeof(packet));
}

static bool HandleReceive(FloodClient* client, const ENetPacket* packet) {
  const ShroomPacketHeader* header;

  if (packet->dataLength < sizeof(ShroomPacketHeader)) {
    return true;
  }
  header = (const ShroomPacketHeader*)packet->data;
  switch ((ShroomPacketType)header->type) {
  case SHROOM_PACKET_WELCOME:
    return SendLobbyJoin(client);
  case SHROOM_PACKET_LOBBY_JOINED: {
    const ShroomLobbyJoinedPacket* joined;
    if (packet->dataLength < sizeof(*joined)) {
      return false;
    }
    joined = (const ShroomLobbyJoinedPacket*)packet->data;
    client->player_id = joined->player_id;
    client->lobby_id = joined->lobby_id;
    client->joined = true;
    return SendReadyAndEnter(client);
  }
  case SHROOM_PACKET_SNAPSHOT: {
    const ShroomSnapshotPacket* snapshot;
    ShroomSnapshotAssemblyResult result;
    ShroomSnapshotFrameMetadata metadata;
    uint16_t player_count = 0u;
    const size_t minimum_size = offsetof(ShroomSnapshotPacket, payload);
    if (packet->dataLength < minimum_size) {
      return false;
    }
    snapshot = (const ShroomSnapshotPacket*)packet->data;
    result = ShroomSnapshotAssemblyPush(&client->snapshot_assembly, snapshot, packet->dataLength,
                                        &client->snapshot_history, &metadata,
                                        client->snapshot_players, &player_count);
    if (result == SHROOM_SNAPSHOT_ASSEMBLY_REJECTED) {
      return false;
    }
    if (result == SHROOM_SNAPSHOT_ASSEMBLY_PENDING) {
      break;
    }
    client->last_acknowledged_sequence = metadata.last_processed_input_sequence;
    client->snapshot_received = true;
    if (!SendSnapshotAck(client, metadata.tick)) {
      return false;
    }
    for (uint16_t index = 0u; index < player_count; ++index) {
      if ((client->snapshot_players[index].player_id == client->player_id) &&
          client->snapshot_players[index].alive) {
        client->player_x = client->snapshot_players[index].position_x;
        client->player_position_received = true;
        break;
      }
    }
  } break;
  case SHROOM_PACKET_PONG: {
    const ShroomPongPacket* pong;
    if (packet->dataLength < sizeof(*pong)) {
      return false;
    }
    pong = (const ShroomPongPacket*)packet->data;
    client->pong_nonce = pong->nonce;
  } break;
  default:
    break;
  }
  return true;
}

static bool Service(FloodClient* client, uint32_t wait_ms) {
  ENetEvent event;
  const int result = enet_host_service(client->host, &event, wait_ms);

  if (result < 0) {
    return false;
  }
  if (result == 0) {
    return true;
  }
  switch (event.type) {
  case ENET_EVENT_TYPE_CONNECT:
    return SendHello(client);
  case ENET_EVENT_TYPE_RECEIVE: {
    const bool handled = HandleReceive(client, event.packet);
    enet_packet_destroy(event.packet);
    return handled;
  }
  case ENET_EVENT_TYPE_DISCONNECT:
    client->disconnected = true;
    return false;
  case ENET_EVENT_TYPE_NONE:
  default:
    return true;
  }
}

static bool WaitForLobby(FloodClient* client, uint32_t timeout_ms) {
  const uint32_t deadline = enet_time_get() + timeout_ms;
  while (!client->disconnected && (!client->joined || !client->snapshot_received) &&
         ((int32_t)(deadline - enet_time_get()) > 0)) {
    if (!Service(client, 5u)) {
      return false;
    }
  }
  return client->joined && client->snapshot_received;
}

static bool WaitForAck(FloodClient* client, uint32_t sequence, uint32_t timeout_ms) {
  const uint32_t deadline = enet_time_get() + timeout_ms;
  while (!client->disconnected && (client->last_acknowledged_sequence != sequence) &&
         ((int32_t)(deadline - enet_time_get()) > 0)) {
    if (!Service(client, 5u)) {
      return false;
    }
  }
  return client->last_acknowledged_sequence == sequence;
}

static bool PumpFor(FloodClient* client, uint32_t duration_ms) {
  const uint32_t deadline = enet_time_get() + duration_ms;
  while (!client->disconnected && ((int32_t)(deadline - enet_time_get()) > 0)) {
    if (!Service(client, 5u)) {
      return false;
    }
  }
  return !client->disconnected;
}

static bool ParsePort(const char* text, uint16_t* port) {
  char* end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);
  if ((text == end) || (*end != '\0') || (parsed == 0u) || (parsed > UINT16_MAX)) {
    return false;
  }
  *port = (uint16_t)parsed;
  return true;
}

int main(int argc, char** argv) {
  FloodClient client = {0};
  ENetAddress address = {.port = SHROOM_SERVER_PORT};
  uint32_t flood_first = 102u;
  uint32_t flood_last = flood_first + FLOOD_PACKET_COUNT - 1u;
  uint32_t limited_ack;
  float position_before_latest;
  int result = 1;

  if ((argc == 3) && (strcmp(argv[1], "--port") == 0)) {
    if (!ParsePort(argv[2], &address.port)) {
      fprintf(stderr, "invalid port\n");
      return 2;
    }
  } else if (argc != 1) {
    fprintf(stderr, "usage: %s [--port PORT]\n", argv[0]);
    return 2;
  }
  if ((enet_initialize() != 0) || (enet_address_set_host(&address, "127.0.0.1") != 0)) {
    fprintf(stderr, "failed to initialize ENet\n");
    return 1;
  }
  client.host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if (client.host == NULL) {
    goto cleanup;
  }
  client.peer = enet_host_connect(client.host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if ((client.peer == NULL) || !WaitForLobby(&client, TEST_TIMEOUT_MS)) {
    fprintf(stderr, "failed to enter test lobby\n");
    goto cleanup;
  }
  if (!SendInput(&client, 100u, 1.0f) || !WaitForAck(&client, 100u, 2000u)) {
    fprintf(stderr, "initial input was not acknowledged\n");
    goto cleanup;
  }
  if (!SendInput(&client, 100u, -1.0f) || !SendInput(&client, 99u, -1.0f) ||
      !SendInput(&client, 101u, 1.0f) || !WaitForAck(&client, 101u, 2000u)) {
    fprintf(stderr, "forward input was starved by stale input\n");
    goto cleanup;
  }

  for (uint32_t sequence = flood_first; sequence <= flood_last; ++sequence) {
    if (!SendInput(&client, sequence, 1.0f)) {
      fprintf(stderr, "failed to queue flood packet %u\n", sequence);
      goto cleanup;
    }
  }
  if (!SendPing(&client, 0x499u)) {
    goto cleanup;
  }
  enet_host_flush(client.host);
  if (!PumpFor(&client, 600u) || (client.last_acknowledged_sequence >= flood_last)) {
    fprintf(stderr, "input flood was not limited: ack=%u flood_last=%u\n",
            client.last_acknowledged_sequence, flood_last);
    goto cleanup;
  }
  if ((client.pong_nonce != 0x499u) && !PumpFor(&client, 2000u)) {
    fprintf(stderr, "server stopped responding during flood\n");
    goto cleanup;
  }
  if (client.pong_nonce != 0x499u) {
    fprintf(stderr, "server did not answer ping during flood\n");
    goto cleanup;
  }
  limited_ack = client.last_acknowledged_sequence;

  if (!PumpFor(&client, RECOVERY_WAIT_MS)) {
    goto cleanup;
  }
  position_before_latest = client.player_x;
  if (!SendInput(&client, 10000u, -1.0f) || !WaitForAck(&client, 10000u, RECOVERY_ACK_TIMEOUT_MS) ||
      !PumpFor(&client, 250u)) {
    fprintf(stderr, "latest input remained starved after %u ms recovery boundary\n",
            RECOVERY_WAIT_MS);
    goto cleanup;
  }
  if (!client.player_position_received || !(client.player_x < position_before_latest)) {
    fprintf(stderr, "latest movement was acknowledged but not applied: before=%.2f after=%.2f\n",
            position_before_latest, client.player_x);
    goto cleanup;
  }

  printf("input flood integration passed: flood_ack=%u latest_ack=%u recovery_ms=%u "
         "latest_dx=%.2f\n",
         limited_ack, 10000u, RECOVERY_WAIT_MS, client.player_x - position_before_latest);
  result = 0;

cleanup:
  if (client.peer != NULL) {
    enet_peer_disconnect_now(client.peer, 0u);
  }
  if (client.host != NULL) {
    enet_host_destroy(client.host);
  }
  enet_deinitialize();
  SleepMillis(1u);
  return result;
}
