#include <enet/enet.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/protocol.h"

#define CLIENT_TIMEOUT_MS 10000u
#define ROUND_ACTIVITY_MS 750u

typedef struct ShutdownClient {
  ENetHost* host;
  ENetPeer* peer;
  uint32_t lobby_id;
  uint32_t player_id;
  uint32_t first_snapshot_ms;
  bool authenticated;
  bool entered;
  bool failed;
} ShutdownClient;

static bool ParsePort(const char* text, uint16_t* port) {
  char* end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);

  if ((text == end) || (*end != '\0') || (parsed == 0u) || (parsed > UINT16_MAX)) {
    return false;
  }
  *port = (uint16_t)parsed;
  return true;
}

static bool SendReliable(ShutdownClient* client, const void* data, size_t size) {
  ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);

  if ((packet == NULL) ||
      (enet_peer_send(client->peer, SHROOM_ENET_CHANNEL_CONTROL, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  return true;
}

static bool SendHello(ShutdownClient* client) {
  ShroomHelloPacket packet = {.protocol_version = SHROOM_PROTOCOL_VERSION};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  snprintf(packet.name, sizeof(packet.name), "Shutdown Probe");
  return SendReliable(client, &packet, sizeof(packet));
}

static bool SendAnonymousAuth(ShutdownClient* client) {
  ShroomAuthRequestPacket packet = {.auth_method = SHROOM_AUTH_ANONYMOUS};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_AUTH_REQUEST, sizeof(packet));
  snprintf(packet.username, sizeof(packet.username), "ShutdownProbe");
  return SendReliable(client, &packet, sizeof(packet));
}

static bool SendLobbyJoin(ShutdownClient* client) {
  ShroomLobbyJoinPacket packet = {.lobby_id = 1u};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN, sizeof(packet));
  return SendReliable(client, &packet, sizeof(packet));
}

static bool SendReadyAndEnter(ShutdownClient* client) {
  ShroomReadyStatePacket ready = {.player_id = client->player_id, .is_ready = 1u};
  ShroomEnterMatchPacket enter = {.lobby_id = client->lobby_id};

  ShroomPacketHeaderInit(&ready.header, SHROOM_PACKET_READY_STATE, sizeof(ready));
  ShroomPacketHeaderInit(&enter.header, SHROOM_PACKET_ENTER_MATCH, sizeof(enter));
  return SendReliable(client, &ready, sizeof(ready)) &&
         SendReliable(client, &enter, sizeof(enter));
}

static bool HandlePacket(ShutdownClient* client, const ENetPacket* packet) {
  const ShroomPacketHeader* header;

  if (packet->dataLength < sizeof(*header)) {
    return false;
  }
  header = (const ShroomPacketHeader*)packet->data;
  switch ((ShroomPacketType)header->type) {
  case SHROOM_PACKET_WELCOME:
    return SendAnonymousAuth(client);
  case SHROOM_PACKET_AUTH_RESPONSE: {
    const ShroomAuthResponsePacket* response = (const ShroomAuthResponsePacket*)packet->data;

    if ((packet->dataLength < sizeof(*response)) ||
        (response->result != SHROOM_AUTH_SUCCESS) || (response->player_id == 0u)) {
      return false;
    }
    client->authenticated = true;
    return SendLobbyJoin(client);
  }
  case SHROOM_PACKET_LOBBY_JOINED: {
    const ShroomLobbyJoinedPacket* joined = (const ShroomLobbyJoinedPacket*)packet->data;

    if ((packet->dataLength < sizeof(*joined)) || (joined->player_id == 0u)) {
      return false;
    }
    client->lobby_id = joined->lobby_id;
    client->player_id = joined->player_id;
    return SendReadyAndEnter(client);
  }
  case SHROOM_PACKET_SNAPSHOT:
    if (client->first_snapshot_ms == 0u) {
      client->first_snapshot_ms = enet_time_get();
    }
    client->entered = true;
    break;
  default:
    break;
  }
  return true;
}

static void Service(ShutdownClient* client) {
  ENetEvent event;
  const int service_result = enet_host_service(client->host, &event, 5u);

  if (service_result < 0) {
    client->failed = true;
  } else if (service_result == 0) {
    return;
  } else if (event.type == ENET_EVENT_TYPE_CONNECT) {
    client->failed = !SendHello(client);
  } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
    client->failed = !HandlePacket(client, event.packet);
    enet_packet_destroy(event.packet);
  } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
    client->failed = true;
  }
}

int main(int argc, char** argv) {
  ShutdownClient client = {0};
  ENetAddress address = {0};
  uint16_t port;
  uint32_t deadline;
  int result = 1;

  if ((argc != 2) || !ParsePort(argv[1], &port)) {
    fprintf(stderr, "usage: %s PORT\n", argv[0]);
    return 2;
  }
  address.port = port;
  if ((enet_initialize() != 0) || (enet_address_set_host(&address, "127.0.0.1") != 0)) {
    return 1;
  }
  client.host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if (client.host == NULL) {
    goto cleanup;
  }
  client.peer = enet_host_connect(client.host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (client.peer == NULL) {
    goto cleanup;
  }

  deadline = enet_time_get() + CLIENT_TIMEOUT_MS;
  while (!client.failed &&
         (!client.entered ||
          ((int32_t)(enet_time_get() - client.first_snapshot_ms) < (int32_t)ROUND_ACTIVITY_MS)) &&
         ((int32_t)(deadline - enet_time_get()) > 0)) {
    Service(&client);
  }
  if (!client.failed && client.authenticated && client.entered) {
    printf("authenticated player entered active round\n");
    result = 0;
  }

cleanup:
  if (client.peer != NULL) {
    enet_peer_disconnect_now(client.peer, 0u);
  }
  if (client.host != NULL) {
    enet_host_destroy(client.host);
  }
  enet_deinitialize();
  return result;
}
