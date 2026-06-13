#include "net.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shared/config.h"

static void SetStatus(ClientNetState* net, ClientNetStatus status, const char* text) {
  net->status = status;
  snprintf(net->status_text, sizeof(net->status_text), "%s", text);
}

static ENetPacket* CreatePacket(const void* data, size_t size, enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static ENetPacket* CreateProtocolPacket(const void* data, size_t size, ShroomPacketType type) {
  return CreatePacket(data, size,
                      ShroomPacketTypeUsesReliableDelivery(type) ? ENET_PACKET_FLAG_RELIABLE : 0);
}

static void RecordRTTSample(ClientNetState* net, uint32_t rtt_ms) {
  uint64_t total = 0;

  net->rtt_ms = rtt_ms;
  net->rtt_samples[net->rtt_sample_index] = rtt_ms;
  if (net->rtt_sample_count < SHROOM_CLIENT_RTT_SAMPLE_COUNT) {
    net->rtt_sample_count += 1;
  }
  net->rtt_sample_index = (net->rtt_sample_index + 1u) % SHROOM_CLIENT_RTT_SAMPLE_COUNT;

  for (uint32_t index = 0; index < net->rtt_sample_count; ++index) {
    total += net->rtt_samples[index];
  }

  net->rtt_average_ms = net->rtt_sample_count > 0 ? (uint32_t)(total / net->rtt_sample_count) : 0;
}

static void SendPing(ClientNetState* net) {
  ShroomPingPacket packet = {0};

  if (!net->welcome_received || (net->peer == 0) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED) || (net->pending_ping_nonce != 0)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_PING, sizeof(packet));
  packet.nonce = ++net->next_ping_nonce;
  net->pending_ping_nonce = packet.nonce;
  net->pending_ping_sent_time_ms = enet_time_get();

  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_PING));
}

static void SendHello(ClientNetState* net) {
  ShroomHelloPacket packet = {0};

  if ((net->peer == 0) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  packet.protocol_version = SHROOM_PROTOCOL_VERSION;
  snprintf(packet.name, sizeof(packet.name), "local-client");

  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_HELLO));
}

static void SendInput(ClientNetState* net, ShroomVec2 input_direction) {
  ShroomInputPacket packet = {0};

  if (!net->welcome_received || (net->peer == 0) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_INPUT, sizeof(packet));
  packet.sequence = ++net->last_input_sequence;
  packet.direction_x = input_direction.x;
  packet.direction_y = input_direction.y;

  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_INPUT,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_INPUT));
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

  /* WELCOME is now a lightweight version handshake only.
   * welcome_received and player_id are set by LOBBY_JOINED. */
  net->handshake_received = true;
  SetStatus(net, CLIENT_NET_CONNECTED, "Connected");
}

static void HandleLobbyList(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomLobbyListPacket* packet = (const ShroomLobbyListPacket*)enet_packet->data;
  uint8_t count;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  count = packet->lobby_count;
  if (count > SHROOM_MAX_LOBBIES) {
    count = SHROOM_MAX_LOBBIES;
  }
  net->lobby_count = count;
  if (count > 0) {
    memcpy(net->lobby_list, packet->lobbies, (size_t)count * sizeof(net->lobby_list[0]));
  }
}

static void HandleLobbyJoined(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomLobbyJoinedPacket* packet = (const ShroomLobbyJoinedPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  net->lobby_id = packet->lobby_id;
  net->spectating = (packet->spectating != 0);
  net->world_width = packet->world_width;
  net->world_height = packet->world_height;

  if (!net->spectating) {
    net->player_id = packet->player_id;
    net->entity_id = packet->entity_id;
    net->welcome_received = true;
  }
}

static void HandlePong(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomPongPacket* packet = (const ShroomPongPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  if ((net->pending_ping_nonce == 0) || (packet->nonce != net->pending_ping_nonce)) {
    return;
  }

  RecordRTTSample(net, enet_time_get() - net->pending_ping_sent_time_ms);
  net->pending_ping_nonce = 0;
}

static void HandleSnapshot(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomSnapshotPacket* packet = (const ShroomSnapshotPacket*)enet_packet->data;
  uint16_t player_count;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  net->last_snapshot_tick = packet->tick;
  net->last_processed_input_sequence = packet->last_processed_input_sequence;
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

static void HandleChat(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomChatPacket* packet;
  ChatMessage* slot;
  size_t msg_len;

  if (enet_packet->dataLength < sizeof(ShroomChatPacket)) {
    return;
  }

  packet = (const ShroomChatPacket*)enet_packet->data;

  slot = &net->chat_history[net->chat_history_head % SHROOM_CLIENT_CHAT_HISTORY_COUNT];
  slot->sender_id = packet->sender_id;
  slot->timestamp_sec = (uint32_t)time(NULL);
  snprintf(slot->sender_name, sizeof(slot->sender_name), "%s", packet->sender_name);

  msg_len = sizeof(packet->message);
  memcpy(slot->message, packet->message, msg_len);
  slot->message[sizeof(slot->message) - 1u] = '\0';

  net->chat_history_head = (net->chat_history_head + 1u) % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
  if (net->chat_history_count < SHROOM_CLIENT_CHAT_HISTORY_COUNT) {
    net->chat_history_count += 1u;
  }
  net->chat_unread_count += 1u;
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
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleWelcome(net, event.packet);
          }
          break;
        case SHROOM_PACKET_SNAPSHOT:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleSnapshot(net, event.packet);
          }
          break;
        case SHROOM_PACKET_SPORE_STATE:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleSporeState(net, event.packet);
          }
          break;
        case SHROOM_PACKET_PONG:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandlePong(net, event.packet);
          }
          break;
        case SHROOM_PACKET_CHAT:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleChat(net, event.packet);
          }
          break;
        case SHROOM_PACKET_LOBBY_LIST:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleLobbyList(net, event.packet);
          }
          break;
        case SHROOM_PACKET_LOBBY_JOINED:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleLobbyJoined(net, event.packet);
          }
          break;
        case SHROOM_PACKET_LOBBY_CREATED:
          /* Handled by the lobby browser screen via net->lobby_count refresh. */
          break;
        case SHROOM_PACKET_PING:
        case SHROOM_PACKET_HELLO:
        case SHROOM_PACKET_INPUT:
        case SHROOM_PACKET_AUTH_REQUEST:
        case SHROOM_PACKET_AUTH_RESPONSE:
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
    net->ping_send_accumulator += delta_time;
    while (net->ping_send_accumulator >= SHROOM_CLIENT_PING_INTERVAL_SECONDS) {
      SendPing(net);
      net->ping_send_accumulator -= SHROOM_CLIENT_PING_INTERVAL_SECONDS;
    }
    enet_host_flush(net->host);
  }
}

void ClientNetShutdown(ClientNetState* net) {
  if (net->peer != 0) {
    enet_peer_disconnect_now(net->peer, 0);
    net->peer = 0;
  }
  net->pending_ping_nonce = 0;
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

void ClientNetSendLobbyListQuery(ClientNetState* net) {
  ShroomPacketHeader packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet, SHROOM_PACKET_LOBBY_LIST_QUERY, sizeof(packet));
  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_LIST_QUERY));
}

void ClientNetSendLobbyJoin(ClientNetState* net, uint32_t lobby_id, bool spectate) {
  ShroomLobbyJoinPacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN, sizeof(packet));
  packet.lobby_id = lobby_id;
  packet.spectate = spectate ? 1u : 0u;
  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_JOIN));
}

void ClientNetSendLobbyLeave(ClientNetState* net) {
  ShroomLobbyLeavePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_LEAVE, sizeof(packet));
  packet.lobby_id = net->lobby_id;
  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_LEAVE));
  net->welcome_received = false;
  net->lobby_id = 0;
  net->spectating = false;
}

void ClientNetSendLobbyCreate(ClientNetState* net, const char* name, uint16_t max_players) {
  ShroomLobbyCreatePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_CREATE, sizeof(packet));
  if (name != NULL && name[0] != '\0') {
    snprintf(packet.name, sizeof(packet.name), "%s", name);
  }
  packet.max_players = max_players;
  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_CREATE));
}

bool ClientNetSendChat(ClientNetState* net, uint32_t player_id, const char* sender_name,
                       const char* message) {
  ShroomChatPacket packet = {0};

  if ((net == NULL) || (message == NULL) || (message[0] == '\0') || !net->welcome_received ||
      (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return false;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_CHAT, sizeof(packet));
  packet.sender_id = player_id;
  snprintf(packet.sender_name, sizeof(packet.sender_name), "%s",
           (sender_name != NULL) ? sender_name : "");
  snprintf(packet.message, sizeof(packet.message), "%s", message);

  return enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CHAT,
                        CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_CHAT)) == 0;
}
