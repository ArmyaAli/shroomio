#include "net.h"

#include <stdio.h>
#include <string.h>

#include "shared/config.h"

static void SetStatus(ClientNetState* net, ClientNetStatus status, const char* text) {
  net->status = status;
  snprintf(net->status_text, sizeof(net->status_text), "%s", text);
}

static ENetPacket* CreatePacket(const void* data, size_t size, enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static void SendHello(ClientNetState* net) {
  ShroomHelloPacket packet = {0};

  if ((net->peer == 0) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  packet.header.type = SHROOM_PACKET_HELLO;
  packet.header.size = sizeof(packet);
  packet.protocol_version = SHROOM_PROTOCOL_VERSION;
  snprintf(packet.name, sizeof(packet.name), "local-client");

  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreatePacket(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE));
}

static void SendInput(ClientNetState* net, ShroomVec2 input_direction) {
  ShroomInputPacket packet = {0};

  if (!net->welcome_received || (net->peer == 0) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  packet.header.type = SHROOM_PACKET_INPUT;
  packet.header.size = sizeof(packet);
  packet.sequence = ++net->last_input_sequence;
  packet.direction_x = input_direction.x;
  packet.direction_y = input_direction.y;

  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_INPUT, CreatePacket(&packet, sizeof(packet), 0));
}

static void HandleWelcome(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomWelcomePacket* packet = (const ShroomWelcomePacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  if (packet->protocol_version != SHROOM_PROTOCOL_VERSION) {
    SetStatus(net, CLIENT_NET_ERROR, "Protocol mismatch");
    return;
  }

  net->welcome_received = true;
  net->player_id = packet->player_id;
  net->entity_id = packet->entity_id;
  SetStatus(net, CLIENT_NET_CONNECTED, "Connected");
}

static void HandleSnapshot(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomSnapshotPacket* packet = (const ShroomSnapshotPacket*)enet_packet->data;
  uint16_t player_count;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  net->last_snapshot_tick = packet->tick;
  player_count = packet->player_count;
  if (player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) {
    player_count = SHROOM_MAX_SNAPSHOT_PLAYERS;
  }
  net->snapshot_player_count = player_count;
  if (player_count > 0) {
    memcpy(net->snapshot_players, packet->players,
           (size_t)player_count * sizeof(packet->players[0]));
  }
}

static void HandleSporeState(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomSporeStatePacket* packet = (const ShroomSporeStatePacket*)enet_packet->data;
  uint16_t spore_count;
  size_t payload_size;

  if (enet_packet->dataLength <
      sizeof(ShroomPacketHeader) + sizeof(uint64_t) + sizeof(uint16_t) + sizeof(uint16_t)) {
    return;
  }

  spore_count = packet->spore_count;
  if (spore_count > SHROOM_MAX_SPORES) {
    spore_count = SHROOM_MAX_SPORES;
  }

  payload_size = sizeof(ShroomPacketHeader) + sizeof(uint64_t) + sizeof(uint16_t) +
                 sizeof(uint16_t) + ((size_t)spore_count * sizeof(ShroomSnapshotSporeState));
  if (enet_packet->dataLength < payload_size) {
    return;
  }

  net->spore_count = spore_count;
  if (spore_count > 0) {
    memcpy(net->snapshot_spores, packet->spores,
           (size_t)spore_count * sizeof(ShroomSnapshotSporeState));
  }
}

bool ClientNetInit(ClientNetState* net, const char* host_name, uint16_t port) {
  ENetAddress address = {0};

  *net = (ClientNetState){0};

  if (enet_initialize() != 0) {
    SetStatus(net, CLIENT_NET_ERROR, "ENet init failed");
    return false;
  }
  net->enet_initialized = true;

  net->host = enet_host_create(0, 1, SHROOM_ENET_CHANNEL_COUNT, 0, 0);
  if (net->host == 0) {
    SetStatus(net, CLIENT_NET_ERROR, "Client host create failed");
    enet_deinitialize();
    net->enet_initialized = false;
    return false;
  }

  address.port = port;
  if (enet_address_set_host(&address, host_name) != 0) {
    SetStatus(net, CLIENT_NET_ERROR, "Server address lookup failed");
    enet_host_destroy(net->host);
    net->host = 0;
    enet_deinitialize();
    net->enet_initialized = false;
    return false;
  }

  net->peer = enet_host_connect(net->host, &address, SHROOM_ENET_CHANNEL_COUNT, 0);
  if (net->peer == 0) {
    SetStatus(net, CLIENT_NET_ERROR, "Connect request failed");
    enet_host_destroy(net->host);
    net->host = 0;
    enet_deinitialize();
    net->enet_initialized = false;
    return false;
  }

  SetStatus(net, CLIENT_NET_CONNECTING, "Connecting");
  return true;
}

void ClientNetUpdate(ClientNetState* net, ShroomVec2 input_direction, float delta_time) {
  ENetEvent event;

  if (net->host == 0) {
    return;
  }

  while (enet_host_service(net->host, &event, 0) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      SetStatus(net, CLIENT_NET_CONNECTING, "Handshaking");
      SendHello(net);
      enet_host_flush(net->host);
      break;
    case ENET_EVENT_TYPE_RECEIVE: {
      const ShroomPacketHeader* header = (const ShroomPacketHeader*)event.packet->data;

      if (event.packet->dataLength >= sizeof(ShroomPacketHeader)) {
        switch ((ShroomPacketType)header->type) {
        case SHROOM_PACKET_WELCOME:
          HandleWelcome(net, event.packet);
          break;
        case SHROOM_PACKET_SNAPSHOT:
          HandleSnapshot(net, event.packet);
          break;
        case SHROOM_PACKET_SPORE_STATE:
          HandleSporeState(net, event.packet);
          break;
        case SHROOM_PACKET_PONG:
        case SHROOM_PACKET_PING:
        case SHROOM_PACKET_HELLO:
        case SHROOM_PACKET_INPUT:
        default:
          break;
        }
      }
      enet_packet_destroy(event.packet);
    } break;
    case ENET_EVENT_TYPE_DISCONNECT:
      net->peer = 0;
      net->welcome_received = false;
      SetStatus(net, CLIENT_NET_DISCONNECTED, "Disconnected");
      break;
    case ENET_EVENT_TYPE_NONE:
    default:
      break;
    }
  }

  if (net->peer != 0) {
    net->input_send_accumulator += delta_time;
    while (net->input_send_accumulator >= (1.0f / SHROOM_SERVER_TICK_RATE)) {
      SendInput(net, input_direction);
      net->input_send_accumulator -= 1.0f / SHROOM_SERVER_TICK_RATE;
    }
    enet_host_flush(net->host);
  }
}

void ClientNetShutdown(ClientNetState* net) {
  if (net->peer != 0) {
    enet_peer_disconnect_now(net->peer, 0);
    net->peer = 0;
  }
  if (net->host != 0) {
    enet_host_destroy(net->host);
    net->host = 0;
  }
  if (net->enet_initialized) {
    enet_deinitialize();
    net->enet_initialized = false;
  }
}

const char* ClientNetStatusLabel(const ClientNetState* net) {
  return net->status_text[0] != '\0' ? net->status_text : "Offline";
}
