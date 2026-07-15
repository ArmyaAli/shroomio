#include <enet/enet.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared/protocol.h"

#define PROBE_TIMEOUT_MS 10000u
#define PROBE_SETTLE_MS 500u
#define PROBE_SENDER_SENTINEL "PRIVATE_SENDER_560"
#define PROBE_MESSAGE_SENTINEL "PRIVATE_MESSAGE_560"

typedef struct ChatLogProbe {
  ENetHost* host;
  ENetPeer* peer;
  uint32_t accepted_count;
  uint32_t completed_at_ms;
  bool failed;
} ChatLogProbe;

static bool ParsePort(const char* text, uint16_t* port) {
  char* end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);

  if ((text == end) || (*end != '\0') || (parsed == 0u) || (parsed > UINT16_MAX)) {
    return false;
  }
  *port = (uint16_t)parsed;
  return true;
}

static bool SendReliable(ChatLogProbe* probe, enet_uint8 channel, const void* data, size_t size) {
  ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);

  if ((packet == NULL) || (enet_peer_send(probe->peer, channel, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  return true;
}

static bool SendHello(ChatLogProbe* probe) {
  ShroomHelloPacket packet = {.protocol_version = SHROOM_PROTOCOL_VERSION};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  snprintf(packet.name, sizeof(packet.name), "%s", PROBE_SENDER_SENTINEL);
  return SendReliable(probe, SHROOM_ENET_CHANNEL_CONTROL, &packet, sizeof(packet));
}

static bool SendAnonymousAuth(ChatLogProbe* probe) {
  ShroomAuthRequestPacket packet = {.auth_method = SHROOM_AUTH_ANONYMOUS};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_AUTH_REQUEST, sizeof(packet));
  snprintf(packet.username, sizeof(packet.username), "ChatLogProbe");
  return SendReliable(probe, SHROOM_ENET_CHANNEL_CONTROL, &packet, sizeof(packet));
}

static bool SendLobbyJoin(ChatLogProbe* probe) {
  ShroomLobbyJoinPacket packet = {.lobby_id = 1u};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN, sizeof(packet));
  return SendReliable(probe, SHROOM_ENET_CHANNEL_CONTROL, &packet, sizeof(packet));
}

static bool SendChatBurst(ChatLogProbe* probe) {
  for (uint32_t index = 0u; index < SHROOM_CHAT_RATE_LIMIT_COUNT + 4u; ++index) {
    ShroomChatPacket packet = {0};

    ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_CHAT, sizeof(packet));
    snprintf(packet.sender_name, sizeof(packet.sender_name), "%s", PROBE_SENDER_SENTINEL);
    snprintf(packet.message, sizeof(packet.message), "%s_%u", PROBE_MESSAGE_SENTINEL, index);
    if (!SendReliable(probe, SHROOM_ENET_CHANNEL_CHAT, &packet, sizeof(packet))) {
      return false;
    }
  }
  return true;
}

static bool HandlePacket(ChatLogProbe* probe, const ENetPacket* packet) {
  const ShroomPacketHeader* header;

  if (packet->dataLength < sizeof(*header)) {
    return false;
  }
  header = (const ShroomPacketHeader*)packet->data;
  switch ((ShroomPacketType)header->type) {
  case SHROOM_PACKET_WELCOME:
    return SendAnonymousAuth(probe);
  case SHROOM_PACKET_AUTH_RESPONSE: {
    const ShroomAuthResponsePacket* response = (const ShroomAuthResponsePacket*)packet->data;

    if ((packet->dataLength < sizeof(*response)) || (response->result != SHROOM_AUTH_SUCCESS)) {
      return false;
    }
    return SendLobbyJoin(probe);
  }
  case SHROOM_PACKET_LOBBY_JOINED:
    if (packet->dataLength < sizeof(ShroomLobbyJoinedPacket)) {
      return false;
    }
    return SendChatBurst(probe);
  case SHROOM_PACKET_CHAT: {
    const ShroomChatPacket* chat = (const ShroomChatPacket*)packet->data;

    if ((packet->dataLength < sizeof(*chat)) ||
        (strstr(chat->message, PROBE_MESSAGE_SENTINEL) == NULL)) {
      return false;
    }
    probe->accepted_count += 1u;
    if (probe->accepted_count == SHROOM_CHAT_RATE_LIMIT_COUNT) {
      probe->completed_at_ms = enet_time_get();
    }
    break;
  }
  default:
    break;
  }
  return true;
}

static void Service(ChatLogProbe* probe) {
  ENetEvent event;
  const int result = enet_host_service(probe->host, &event, 5u);

  if (result < 0) {
    probe->failed = true;
  } else if (result == 0) {
    return;
  } else if (event.type == ENET_EVENT_TYPE_CONNECT) {
    probe->failed = !SendHello(probe);
  } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
    probe->failed = !HandlePacket(probe, event.packet);
    enet_packet_destroy(event.packet);
  } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
    probe->failed = true;
  }
}

int main(int argc, char** argv) {
  ChatLogProbe probe = {0};
  ENetAddress address = {0};
  uint16_t port;
  uint32_t started_at;
  int result = EXIT_FAILURE;

  if ((argc != 2) || !ParsePort(argv[1], &port)) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    return EXIT_FAILURE;
  }
  if (enet_initialize() != 0) {
    return EXIT_FAILURE;
  }
  probe.host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if ((probe.host == NULL) || (enet_address_set_host(&address, "127.0.0.1") != 0)) {
    goto cleanup;
  }
  address.port = port;
  probe.peer = enet_host_connect(probe.host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (probe.peer == NULL) {
    goto cleanup;
  }

  started_at = enet_time_get();
  while (!probe.failed && ((enet_time_get() - started_at) < PROBE_TIMEOUT_MS)) {
    Service(&probe);
    if ((probe.completed_at_ms != 0u) &&
        ((enet_time_get() - probe.completed_at_ms) >= PROBE_SETTLE_MS)) {
      result = EXIT_SUCCESS;
      break;
    }
  }
  if (result != EXIT_SUCCESS) {
    fprintf(stderr, "chat log probe failed: accepted=%u\n", probe.accepted_count);
  }

cleanup:
  if (probe.host != NULL) {
    enet_host_destroy(probe.host);
  }
  enet_deinitialize();
  return result;
}
