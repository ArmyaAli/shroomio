#include "net.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shared/config.h"

static void SetStatus(ClientNetState *net, ClientNetStatus status,
                      const char *text) {
  net->status = status;
  snprintf(net->status_text, sizeof(net->status_text), "%s", text);
}

static ENetPacket *CreatePacket(const void *data, size_t size,
                                enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static ENetPacket *CreateProtocolPacket(const void *data, size_t size,
                                        ShroomPacketType type) {
  return CreatePacket(data, size,
                      ShroomPacketTypeUsesReliableDelivery(type)
                          ? ENET_PACKET_FLAG_RELIABLE
                          : 0);
}

static void RecordRTTSample(ClientNetState *net, uint32_t rtt_ms) {
  uint64_t total = 0;

  net->rtt_ms = rtt_ms;
  net->rtt_samples[net->rtt_sample_index] = rtt_ms;
  if (net->rtt_sample_count < SHROOM_CLIENT_RTT_SAMPLE_COUNT) {
    net->rtt_sample_count += 1;
  }
  net->rtt_sample_index =
      (net->rtt_sample_index + 1u) % SHROOM_CLIENT_RTT_SAMPLE_COUNT;

  for (uint32_t index = 0; index < net->rtt_sample_count; ++index) {
    total += net->rtt_samples[index];
  }

  net->rtt_average_ms =
      net->rtt_sample_count > 0 ? (uint32_t)(total / net->rtt_sample_count) : 0;
}

static uint32_t ElapsedMs(uint32_t now_ms, uint32_t then_ms) {
  return now_ms - then_ms;
}

static void ClearStalePendingPing(ClientNetState *net, uint32_t now_ms) {
  if ((net->pending_ping_nonce != 0) &&
      (ElapsedMs(now_ms, net->pending_ping_sent_time_ms) >=
       SHROOM_CLIENT_PING_TIMEOUT_MS)) {
    net->pending_ping_nonce = 0;
  }
}

static void CheckConnectTimeout(ClientNetState *net, uint32_t now_ms) {
  if ((net->status == CLIENT_NET_CONNECTING) &&
      (net->connect_started_ms != 0u) &&
      (ElapsedMs(now_ms, net->connect_started_ms) >=
       SHROOM_CLIENT_CONNECT_TIMEOUT_MS)) {
    SetStatus(net, CLIENT_NET_ERROR, SHROOM_NET_CONNECT_UNREACHABLE_MSG);
    net->connect_started_ms = 0u;
  }
}

static bool CompletePendingPing(ClientNetState *net, uint32_t nonce,
                                uint32_t now_ms) {
  uint32_t rtt_ms;

  if ((net->pending_ping_nonce == 0) || (nonce != net->pending_ping_nonce)) {
    return false;
  }

  rtt_ms = ElapsedMs(now_ms, net->pending_ping_sent_time_ms);
  net->pending_ping_nonce = 0;
  if (rtt_ms >= SHROOM_CLIENT_PING_TIMEOUT_MS) {
    return false;
  }

  RecordRTTSample(net, rtt_ms);
  return true;
}

static void SendPing(ClientNetState *net) {
  ShroomPingPacket packet = {0};

  if (!net->welcome_received || (net->peer == 0) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED) ||
      (net->pending_ping_nonce != 0)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_PING, sizeof(packet));
  packet.nonce = ++net->next_ping_nonce;
  net->pending_ping_nonce = packet.nonce;
  net->pending_ping_sent_time_ms = enet_time_get();

  enet_peer_send(
      net->peer, SHROOM_ENET_CHANNEL_CONTROL,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_PING));
}

static void SendHello(ClientNetState *net) {
  ShroomHelloPacket packet = {0};

  if ((net->peer == 0) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(packet));
  packet.protocol_version = SHROOM_PROTOCOL_VERSION;
  snprintf(packet.name, sizeof(packet.name), "local-client");

  enet_peer_send(
      net->peer, SHROOM_ENET_CHANNEL_CONTROL,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_HELLO));
}

static void SendInput(ClientNetState *net, ShroomVec2 input_direction,
                      bool split_requested, bool eject_requested,
                      ShroomVec2 split_direction, uint32_t focused_entity_id) {
  ShroomInputPacket packet = {0};

  if (!net->welcome_received || !net->match_entry_sent || (net->peer == 0) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_INPUT, sizeof(packet));
  packet.sequence = ++net->last_input_sequence;
  packet.direction_x = input_direction.x;
  packet.direction_y = input_direction.y;
  packet.split_direction_x = split_direction.x;
  packet.split_direction_y = split_direction.y;
  packet.split_requested = split_requested ? 1u : 0u;
  packet.eject_requested = eject_requested ? 1u : 0u;
  packet.focused_entity_id = focused_entity_id;

  enet_peer_send(
      net->peer, SHROOM_ENET_CHANNEL_INPUT,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_INPUT));
}

static void HandleWelcome(ClientNetState *net, const ENetPacket *enet_packet) {
  const ShroomWelcomePacket *packet =
      (const ShroomWelcomePacket *)enet_packet->data;

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

static void HandleLobbyList(ClientNetState *net,
                            const ENetPacket *enet_packet) {
  const ShroomLobbyListPacket *packet =
      (const ShroomLobbyListPacket *)enet_packet->data;
  const size_t min_size = offsetof(ShroomLobbyListPacket, lobbies);
  uint8_t count;

  if (enet_packet->dataLength < min_size) {
    return;
  }

  count = packet->lobby_count;
  if (count > SHROOM_MAX_LOBBIES) {
    count = SHROOM_MAX_LOBBIES;
  }
  if (enet_packet->dataLength <
      min_size + (size_t)count * sizeof(packet->lobbies[0])) {
    return;
  }
  net->lobby_count = count;
  if (count > 0) {
    memcpy(net->lobby_list, packet->lobbies,
           (size_t)count * sizeof(net->lobby_list[0]));
  }
}

static void HandleLobbyJoined(ClientNetState *net,
                              const ENetPacket *enet_packet) {
  const ShroomLobbyJoinedPacket *packet =
      (const ShroomLobbyJoinedPacket *)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  net->lobby_id = packet->lobby_id;
  net->spectating = (packet->spectating != 0);
  net->world_width = packet->world_width;
  net->world_height = packet->world_height;
  net->lobby_max_players = packet->max_players;
  net->match_entry_sent = false;
  net->lobby_roster_received = false;
  net->lobby_match_started = false;
  net->lobby_roster_count = 0;
  snprintf(net->lobby_name, sizeof(net->lobby_name), "%s", packet->lobby_name);

  if (!net->spectating) {
    net->player_id = packet->player_id;
    net->entity_id = packet->entity_id;
    net->welcome_received = true;
  }
}

static void HandleLobbyRoster(ClientNetState *net,
                              const ENetPacket *enet_packet) {
  const ShroomLobbyRosterPacket *packet =
      (const ShroomLobbyRosterPacket *)enet_packet->data;
  const size_t min_size = offsetof(ShroomLobbyRosterPacket, players);
  uint16_t count;

  if (enet_packet->dataLength < min_size || packet->lobby_id != net->lobby_id) {
    return;
  }
  count = packet->player_count;
  if (count > SHROOM_MAX_PLAYERS ||
      enet_packet->dataLength <
          min_size + (size_t)count * sizeof(packet->players[0])) {
    return;
  }
  net->lobby_roster_count = count;
  net->lobby_match_started = packet->match_started != 0;
  net->lobby_roster_received = true;
  if (count > 0) {
    memcpy(net->lobby_roster, packet->players,
           (size_t)count * sizeof(net->lobby_roster[0]));
  }
}

static void HandlePong(ClientNetState *net, const ENetPacket *enet_packet) {
  const ShroomPongPacket *packet = (const ShroomPongPacket *)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  CompletePendingPing(net, packet->nonce, enet_time_get());
}

static void HandleSnapshot(ClientNetState *net, const ENetPacket *enet_packet) {
  const ShroomSnapshotPacket *packet =
      (const ShroomSnapshotPacket *)enet_packet->data;
  uint16_t player_count;
  const size_t min_size = offsetof(ShroomSnapshotPacket, players);

  if (enet_packet->dataLength < min_size) {
    return;
  }

  net->last_snapshot_tick = packet->tick;
  net->last_processed_input_sequence = packet->last_processed_input_sequence;
  net->match_phase = packet->match_phase;
  net->match_time_remaining = packet->match_time_remaining;
  for (uint32_t i = 0; i < SHROOM_MATCH_PODIUM_COUNT; ++i) {
    net->podium_player_ids[i] = packet->podium_player_ids[i];
    net->podium_masses[i] = packet->podium_masses[i];
  }
  player_count = packet->player_count;
  if (player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) {
    player_count = SHROOM_MAX_SNAPSHOT_PLAYERS;
  }
  if (enet_packet->dataLength <
      min_size + (size_t)player_count * sizeof(packet->players[0])) {
    return;
  }
  net->snapshot_player_count = player_count;
  if (player_count > 0) {
    memcpy(net->snapshot_players, packet->players,
           (size_t)player_count * sizeof(packet->players[0]));
  }
}

static void HandleChat(ClientNetState *net, const ENetPacket *enet_packet) {
  const ShroomChatPacket *packet;
  ChatMessage *slot;
  size_t msg_len;

  if (enet_packet->dataLength < sizeof(ShroomChatPacket)) {
    return;
  }

  packet = (const ShroomChatPacket *)enet_packet->data;

  slot = &net->chat_history[net->chat_history_head %
                            SHROOM_CLIENT_CHAT_HISTORY_COUNT];
  slot->sender_id = packet->sender_id;
  slot->timestamp_sec = (uint32_t)time(NULL);
  snprintf(slot->sender_name, sizeof(slot->sender_name), "%s",
           packet->sender_name);

  msg_len = sizeof(packet->message);
  memcpy(slot->message, packet->message, msg_len);
  slot->message[sizeof(slot->message) - 1u] = '\0';

  net->chat_history_head =
      (net->chat_history_head + 1u) % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
  if (net->chat_history_count < SHROOM_CLIENT_CHAT_HISTORY_COUNT) {
    net->chat_history_count += 1u;
  }
  net->chat_unread_count += 1u;
}

static void HandleSporeState(ClientNetState *net,
                             const ENetPacket *enet_packet) {
  const ShroomSporeStatePacket *packet =
      (const ShroomSporeStatePacket *)enet_packet->data;
  uint16_t total_spore_count;
  uint16_t chunk_start_index;
  size_t chunk_count;
  size_t payload_size;
  const size_t min_size = offsetof(ShroomSporeStatePacket, spores);

  if (enet_packet->dataLength < min_size) {
    return;
  }

  total_spore_count = packet->spore_count;
  if (total_spore_count > SHROOM_MAX_SPORES) {
    total_spore_count = SHROOM_MAX_SPORES;
  }

  payload_size = enet_packet->dataLength - min_size;
  if ((payload_size % sizeof(ShroomSnapshotSporeState)) != 0u) {
    return;
  }
  chunk_count = payload_size / sizeof(ShroomSnapshotSporeState);
  chunk_start_index = packet->reserved;
  if (total_spore_count == 0u) {
    if ((chunk_start_index != 0u) || (chunk_count != 0u)) {
      return;
    }
    net->spore_count = 0u;
    memset(net->snapshot_spores, 0, sizeof(net->snapshot_spores));
    return;
  }
  if ((chunk_start_index >= SHROOM_MAX_SPORES) ||
      (chunk_start_index >= total_spore_count)) {
    return;
  }
  if ((size_t)chunk_start_index + chunk_count > SHROOM_MAX_SPORES) {
    chunk_count = SHROOM_MAX_SPORES - chunk_start_index;
  }
  if ((size_t)chunk_start_index + chunk_count > total_spore_count) {
    chunk_count = (size_t)total_spore_count - chunk_start_index;
  }

  net->spore_count = total_spore_count;
  if (chunk_start_index == 0u) {
    memset(net->snapshot_spores, 0, sizeof(net->snapshot_spores));
  }
  if (chunk_count > 0u) {
    memcpy(&net->snapshot_spores[chunk_start_index], packet->spores,
           chunk_count * sizeof(ShroomSnapshotSporeState));
  }
}

static void HandlePowerupState(ClientNetState *net,
                               const ENetPacket *enet_packet) {
  const ShroomPowerupStatePacket *packet =
      (const ShroomPowerupStatePacket *)enet_packet->data;
  uint16_t powerup_count;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  powerup_count = packet->powerup_count;
  if (powerup_count > SHROOM_MAX_POWERUPS) {
    powerup_count = SHROOM_MAX_POWERUPS;
  }

  net->powerup_count = powerup_count;
  if (powerup_count > 0) {
    memcpy(net->snapshot_powerups, packet->powerups,
           (size_t)powerup_count * sizeof(ShroomSnapshotPowerupState));
  }
}

static void HandleMushroomSpeciesCatalog(ClientNetState *net,
                                         const ENetPacket *enet_packet) {
  const ShroomMushroomSpeciesCatalogPacket *packet =
      (const ShroomMushroomSpeciesCatalogPacket *)enet_packet->data;
  const size_t min_size = offsetof(ShroomMushroomSpeciesCatalogPacket, species);
  uint8_t species_count;

  if (enet_packet->dataLength < min_size) {
    return;
  }

  species_count = packet->species_count;
  if (species_count > SHROOM_MAX_MUSHROOM_SPECIES) {
    species_count = SHROOM_MAX_MUSHROOM_SPECIES;
  }
  if (enet_packet->dataLength <
      min_size + (size_t)species_count * sizeof(packet->species[0])) {
    return;
  }

  net->mushroom_species_count = species_count;
  net->mushroom_species_catalog_received = true;
  if (species_count > 0) {
    memcpy(net->mushroom_species, packet->species,
           (size_t)species_count * sizeof(net->mushroom_species[0]));
    for (uint8_t index = 0; index < species_count; ++index) {
      net->mushroom_species[index]
          .name[sizeof(net->mushroom_species[index].name) - 1u] = '\0';
      net->mushroom_species[index]
          .description[sizeof(net->mushroom_species[index].description) - 1u] =
          '\0';
    }
  }
}

bool ClientNetInit(ClientNetState *net, const char *host_name, uint16_t port) {
  ENetAddress address = {0};

  if ((net->peer != 0) || (net->host != 0) || net->enet_initialized) {
    ClientNetShutdown(net);
  }
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
    SetStatus(net, CLIENT_NET_ERROR, SHROOM_NET_CONNECT_UNREACHABLE_MSG);
    enet_host_destroy(net->host);
    net->host = 0;
    enet_deinitialize();
    net->enet_initialized = false;
    return false;
  }

  net->peer =
      enet_host_connect(net->host, &address, SHROOM_ENET_CHANNEL_COUNT, 0);
  if (net->peer == 0) {
    SetStatus(net, CLIENT_NET_ERROR, SHROOM_NET_CONNECT_UNREACHABLE_MSG);
    enet_host_destroy(net->host);
    net->host = 0;
    enet_deinitialize();
    net->enet_initialized = false;
    return false;
  }

  net->connect_started_ms = enet_time_get();
  SetStatus(net, CLIENT_NET_CONNECTING, "Connecting");
  return true;
}

void ClientNetUpdate(ClientNetState *net, ShroomVec2 input_direction,
                     bool split_requested, bool eject_requested,
                     ShroomVec2 split_direction, uint32_t focused_entity_id,
                     float delta_time) {
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
      const ShroomPacketHeader *header =
          (const ShroomPacketHeader *)event.packet->data;

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
        case SHROOM_PACKET_POWERUP_STATE:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandlePowerupState(net, event.packet);
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
        case SHROOM_PACKET_LOBBY_ROSTER:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleLobbyRoster(net, event.packet);
          }
          break;
        case SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleMushroomSpeciesCatalog(net, event.packet);
          }
          break;
        case SHROOM_PACKET_LOBBY_CREATED:
          /* Handled by the lobby browser screen via net->lobby_count refresh.
           */
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
      net->match_entry_sent = false;
      SetStatus(net, CLIENT_NET_DISCONNECTED, "Disconnected");
      break;
    case ENET_EVENT_TYPE_NONE:
    default:
      break;
    }
  }

  CheckConnectTimeout(net, enet_time_get());

  if (net->peer != 0) {
    ClearStalePendingPing(net, enet_time_get());
    net->input_send_accumulator += delta_time;
    while (net->input_send_accumulator >= (1.0f / SHROOM_SERVER_TICK_RATE)) {
      SendInput(net, input_direction, split_requested, eject_requested,
                split_direction, focused_entity_id);
      /* Action flags are one-shot — clear after the first packet in this
       * update. */
      split_requested = false;
      eject_requested = false;
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

#ifdef TEST_MODE
bool ClientNetTestCompletePendingPing(ClientNetState *net, uint32_t nonce,
                                      uint32_t now_ms) {
  return CompletePendingPing(net, nonce, now_ms);
}

void ClientNetTestClearStalePendingPing(ClientNetState *net, uint32_t now_ms) {
  ClearStalePendingPing(net, now_ms);
}

void ClientNetTestCheckConnectTimeout(ClientNetState *net, uint32_t now_ms) {
  CheckConnectTimeout(net, now_ms);
}

void ClientNetTestHandleSnapshot(ClientNetState *net,
                                 const ENetPacket *enet_packet) {
  HandleSnapshot(net, enet_packet);
}

void ClientNetTestHandleSporeState(ClientNetState *net,
                                   const ENetPacket *enet_packet) {
  HandleSporeState(net, enet_packet);
}

void ClientNetTestHandleLobbyList(ClientNetState *net,
                                  const ENetPacket *enet_packet) {
  HandleLobbyList(net, enet_packet);
}

void ClientNetTestHandleLobbyRoster(ClientNetState *net,
                                    const ENetPacket *enet_packet) {
  HandleLobbyRoster(net, enet_packet);
}

bool ClientNetTestCanSendGameplayInput(const ClientNetState *net) {
  return net != NULL && net->welcome_received && net->match_entry_sent;
}

void ClientNetTestHandleMushroomSpeciesCatalog(ClientNetState *net,
                                               const ENetPacket *enet_packet) {
  HandleMushroomSpeciesCatalog(net, enet_packet);
}
#endif

void ClientNetShutdown(ClientNetState *net) {
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
  *net = (ClientNetState){0};
}

const char *ClientNetStatusLabel(const ClientNetState *net) {
  return net->status_text[0] != '\0' ? net->status_text : "Offline";
}

void ClientNetSendLobbyListQuery(ClientNetState *net) {
  ShroomPacketHeader packet = {0};

  if ((net == NULL) || (net->peer == NULL) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet, SHROOM_PACKET_LOBBY_LIST_QUERY,
                         sizeof(packet));
  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet),
                                      SHROOM_PACKET_LOBBY_LIST_QUERY));
}

void ClientNetSendLobbyJoin(ClientNetState *net, uint32_t lobby_id,
                            bool spectate) {
  ShroomLobbyJoinPacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN,
                         sizeof(packet));
  packet.lobby_id = lobby_id;
  packet.spectate = spectate ? 1u : 0u;
  enet_peer_send(
      net->peer, SHROOM_ENET_CHANNEL_CONTROL,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_JOIN));
}

void ClientNetSendLobbyLeave(ClientNetState *net) {
  ShroomLobbyLeavePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_LEAVE,
                         sizeof(packet));
  packet.lobby_id = net->lobby_id;
  enet_peer_send(
      net->peer, SHROOM_ENET_CHANNEL_CONTROL,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_LEAVE));
  net->welcome_received = false;
  net->match_entry_sent = false;
  net->lobby_roster_received = false;
  net->lobby_match_started = false;
  net->lobby_roster_count = 0;
  net->lobby_id = 0;
  net->spectating = false;
}

void ClientNetSendLobbyCreate(ClientNetState *net, const char *name,
                              uint16_t max_players) {
  ShroomLobbyCreatePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_CREATE,
                         sizeof(packet));
  if (name != NULL && name[0] != '\0') {
    snprintf(packet.name, sizeof(packet.name), "%s", name);
  }
  packet.max_players = max_players;
  enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet),
                                      SHROOM_PACKET_LOBBY_CREATE));
}

void ClientNetSendReadyState(ClientNetState *net, bool is_ready) {
  ShroomReadyStatePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED) ||
      !net->welcome_received) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_READY_STATE,
                         sizeof(packet));
  packet.player_id = net->player_id;
  packet.is_ready = is_ready ? 1 : 0;
  enet_peer_send(
      net->peer, SHROOM_ENET_CHANNEL_CONTROL,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_READY_STATE));
}

void ClientNetSendEnterMatch(ClientNetState *net) {
  ShroomEnterMatchPacket packet = {0};

  if (net == NULL || (!net->welcome_received && !net->spectating) ||
      net->lobby_id == 0) {
    return;
  }
  if (net->peer == NULL || net->peer->state != ENET_PEER_STATE_CONNECTED) {
#ifdef TEST_MODE
    net->match_entry_sent = true;
#endif
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_ENTER_MATCH,
                         sizeof(packet));
  packet.lobby_id = net->lobby_id;
  if (enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CONTROL,
                     CreateProtocolPacket(&packet, sizeof(packet),
                                          SHROOM_PACKET_ENTER_MATCH)) == 0) {
    net->match_entry_sent = true;
    enet_host_flush(net->host);
  }
}

bool ClientNetSendChat(ClientNetState *net, uint32_t player_id,
                       const char *sender_name, const char *message) {
  ShroomChatPacket packet = {0};

  if ((net == NULL) || (message == NULL) || (message[0] == '\0') ||
      !net->welcome_received || (net->peer == NULL) ||
      (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return false;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_CHAT, sizeof(packet));
  packet.sender_id = player_id;
  snprintf(packet.sender_name, sizeof(packet.sender_name), "%s",
           (sender_name != NULL) ? sender_name : "");
  snprintf(packet.message, sizeof(packet.message), "%s", message);

  return enet_peer_send(net->peer, SHROOM_ENET_CHANNEL_CHAT,
                        CreateProtocolPacket(&packet, sizeof(packet),
                                             SHROOM_PACKET_CHAT)) == 0;
}
