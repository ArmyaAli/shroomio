#include <enet/enet.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/protocol.h"

#define CLIENT_TIMEOUT_MS 3000u

typedef struct AuthCollisionClient {
  ENetHost* host;
  ENetPeer* peer;
  const char* username;
  bool received_rejection;
  bool failed;
} AuthCollisionClient;

static bool ParsePort(const char* text, uint16_t* port) {
  char* end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);

  if ((text == end) || (*end != '\0') || (parsed == 0u) || (parsed > UINT16_MAX)) {
    return false;
  }
  *port = (uint16_t)parsed;
  return true;
}

static bool SendReliable(AuthCollisionClient* client, const void* data, size_t size) {
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

static bool SendHello(AuthCollisionClient* client) {
  ShroomHelloPacket packet = {.protocol_version = SHROOM_PROTOCOL_VERSION};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  snprintf(packet.name, sizeof(packet.name), "Auth Collision Probe");
  return SendReliable(client, &packet, sizeof(packet));
}

static bool SendAnonymousAuth(AuthCollisionClient* client) {
  ShroomAuthRequestPacket packet = {.auth_method = SHROOM_AUTH_ANONYMOUS};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_AUTH_REQUEST, sizeof(packet));
  snprintf(packet.username, sizeof(packet.username), "%s", client->username);
  return SendReliable(client, &packet, sizeof(packet));
}

static bool HandlePacket(AuthCollisionClient* client, const ENetPacket* packet) {
  const ShroomPacketHeader* header;

  if (packet->dataLength < sizeof(*header)) {
    return false;
  }
  header = (const ShroomPacketHeader*)packet->data;
  if (header->type == SHROOM_PACKET_WELCOME) {
    return SendAnonymousAuth(client);
  }
  if (header->type == SHROOM_PACKET_AUTH_RESPONSE) {
    const ShroomAuthResponsePacket* response = (const ShroomAuthResponsePacket*)packet->data;

    if (packet->dataLength < sizeof(*response)) {
      return false;
    }
    client->received_rejection = response->result == SHROOM_AUTH_USERNAME_TAKEN &&
                                 response->player_id == 0u && response->token[0] == '\0';
    return client->received_rejection;
  }
  return true;
}

static void Service(AuthCollisionClient* client) {
  ENetEvent event;
  const int service_result = enet_host_service(client->host, &event, 10u);

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
  AuthCollisionClient client = {0};
  ENetAddress address = {0};
  uint16_t port;
  uint32_t deadline;
  int result = 1;

  if ((argc != 3) || !ParsePort(argv[1], &port) || (argv[2][0] == '\0') ||
      (strlen(argv[2]) >= SHROOM_MAX_NAME_LENGTH)) {
    fprintf(stderr, "usage: %s PORT REGISTERED_USERNAME\n", argv[0]);
    return 2;
  }
  client.username = argv[2];
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
  while (!client.failed && !client.received_rejection &&
         ((int32_t)(deadline - enet_time_get()) > 0)) {
    Service(&client);
  }
  if (!client.failed && client.received_rejection) {
    printf("anonymous collision rejected for registered username '%s'\n", client.username);
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
