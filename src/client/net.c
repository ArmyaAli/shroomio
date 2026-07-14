#include "net.h"
#include "results_transition.h"
#include "shared/player_identity.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shared/config.h"
#include "shared/world.h"

static void SetStatus(ClientNetState* net, ClientNetStatus status, const char* text) {
  net->status = status;
  snprintf(net->status_text, sizeof(net->status_text), "%s", text);
}

static void ResetIntermissionLifecycle(ClientNetState* net) {
  memset(&net->intermission, 0, sizeof(net->intermission));
  net->intermission_received = false;
  net->consumed_intermission_round_id = 0u;
  net->consumed_intermission_round_valid = false;
}

static ENetPacket* CreatePacket(const void* data, size_t size, enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static ENetPacket* CreateProtocolPacket(const void* data, size_t size, ShroomPacketType type) {
  return CreatePacket(data, size,
                      ShroomPacketTypeUsesReliableDelivery(type) ? ENET_PACKET_FLAG_RELIABLE : 0);
}

static bool SendPacket(ClientNetState* net, uint8_t channel, ShroomPacketType type,
                       ENetPacket* packet) {
  const size_t bytes = packet != NULL ? packet->dataLength : 0u;

  if ((net == NULL) || (net->peer == NULL) || (packet == NULL) ||
      (enet_peer_send(net->peer, channel, packet) != 0)) {
    ShroomNetTelemetryRecordDrop(net != NULL ? &net->telemetry : NULL, 0u, channel, type, bytes,
                                 enet_time_get());
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  ShroomNetTelemetryRecordSent(&net->telemetry, 0u, channel, type, bytes, enet_time_get());
  return true;
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

static uint32_t ElapsedMs(uint32_t now_ms, uint32_t then_ms) { return now_ms - then_ms; }

static void ClearStalePendingPing(ClientNetState* net, uint32_t now_ms) {
  if ((net->pending_ping_nonce != 0) &&
      (ElapsedMs(now_ms, net->pending_ping_sent_time_ms) >= SHROOM_CLIENT_PING_TIMEOUT_MS)) {
    net->pending_ping_nonce = 0;
  }
}

static void CheckConnectTimeout(ClientNetState* net, uint32_t now_ms) {
  if ((net->status == CLIENT_NET_CONNECTING) && (net->connect_started_ms != 0u) &&
      (ElapsedMs(now_ms, net->connect_started_ms) >= SHROOM_CLIENT_CONNECT_TIMEOUT_MS)) {
    SetStatus(net, CLIENT_NET_ERROR, SHROOM_NET_CONNECT_UNREACHABLE_MSG);
    net->connect_started_ms = 0u;
  }
}

static bool CompletePendingPing(ClientNetState* net, uint32_t nonce, uint32_t now_ms) {
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

  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_PING,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_PING));
}

static void BuildHelloPacket(const ClientNetState* net, ShroomHelloPacket* packet) {
  char sanitized_name[SHROOM_MAX_NAME_LENGTH];
  *packet = (ShroomHelloPacket){0};
  ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_HELLO, sizeof(*packet));
  packet->protocol_version = SHROOM_PROTOCOL_VERSION;
  ShroomSanitizePlayerName(sanitized_name, net->player_name);
  snprintf(packet->name, sizeof(packet->name), "%s",
           sanitized_name[0] != '\0' ? sanitized_name : "Player");
}

static void SendHello(ClientNetState* net) {
  ShroomHelloPacket packet;

  if ((net->peer == 0) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  BuildHelloPacket(net, &packet);

  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_HELLO,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_HELLO));
}

static bool CanSendGameplayInput(const ClientNetState* net) {
  return (net != NULL) && net->welcome_received && net->match_entry_sent &&
         (net->match_phase == SHROOM_MATCH_PHASE_RUNNING) && (net->peer != NULL) &&
         (net->peer->state == ENET_PEER_STATE_CONNECTED);
}

static bool SendInput(ClientNetState* net, ShroomVec2 input_direction, bool split_requested,
                      bool eject_requested, ShroomVec2 split_direction,
                      uint32_t focused_entity_id) {
  ShroomInputPacket packet = {0};
  ENetPacket* outbound;
  uint32_t sequence;

  if (!CanSendGameplayInput(net)) {
    return false;
  }

  sequence = net->last_input_sequence == UINT32_MAX ? 1u : net->last_input_sequence + 1u;
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_INPUT, sizeof(packet));
  packet.sequence = sequence;
  packet.direction_x = input_direction.x;
  packet.direction_y = input_direction.y;
  packet.split_direction_x = split_direction.x;
  packet.split_direction_y = split_direction.y;
  packet.split_requested = split_requested ? 1u : 0u;
  packet.eject_requested = eject_requested ? 1u : 0u;
  packet.focused_entity_id = focused_entity_id;

  /* Movement is replaceable state; one-shot actions must survive packet loss. */
  outbound = CreatePacket(&packet, sizeof(packet),
                          (split_requested || eject_requested) ? ENET_PACKET_FLAG_RELIABLE : 0u);
  if (!SendPacket(net, SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT, outbound)) {
    return false;
  }
  net->last_input_sequence = sequence;
  return true;
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
  const size_t min_size = offsetof(ShroomLobbyListPacket, lobbies);
  uint8_t count;

  if (enet_packet->dataLength < min_size) {
    return;
  }

  count = packet->lobby_count;
  if (count > SHROOM_MAX_LOBBIES) {
    count = SHROOM_MAX_LOBBIES;
  }
  if (enet_packet->dataLength < min_size + (size_t)count * sizeof(packet->lobbies[0])) {
    return;
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

  ResetIntermissionLifecycle(net);
  net->lobby_id = packet->lobby_id;
  net->spectating = (packet->spectating != 0);
  net->game_mode = packet->game_mode;
  net->world_width = packet->world_width;
  net->world_height = packet->world_height;
  net->lobby_max_players = packet->max_players;
  net->match_entry_sent = false;
  net->lobby_roster_received = false;
  net->lobby_match_started = false;
  net->lobby_roster_count = 0;
  snprintf(net->lobby_name, sizeof(net->lobby_name), "%s", packet->lobby_name);
  {
    const ShroomChatCacheKey key = {
        .port = net->server_port,
        .lobby_id = net->lobby_id,
    };
    ShroomChatCacheKey context = key;
    snprintf(context.host, sizeof(context.host), "%s", net->server_host);
    memset(net->chat_history, 0, sizeof(net->chat_history));
    net->chat_history_count =
        (uint32_t)ShroomChatCacheLoadContext(net->chat_cache_path, &context, (uint32_t)time(NULL),
                                             net->chat_history, SHROOM_CLIENT_CHAT_HISTORY_COUNT);
    net->chat_history_head = net->chat_history_count % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
    net->chat_unread_count = 0u;
  }

  if (!net->spectating) {
    net->player_id = packet->player_id;
    net->entity_id = packet->entity_id;
    net->welcome_received = true;
  }
}

static void HandleLobbyRoster(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomLobbyRosterPacket* packet = (const ShroomLobbyRosterPacket*)enet_packet->data;
  const size_t min_size = offsetof(ShroomLobbyRosterPacket, players);
  uint16_t count;

  if (enet_packet->dataLength < min_size || packet->lobby_id != net->lobby_id) {
    return;
  }
  count = packet->player_count;
  if (count > SHROOM_MAX_PARTICIPANTS ||
      enet_packet->dataLength < min_size + (size_t)count * sizeof(packet->players[0])) {
    return;
  }
  net->lobby_roster_count = count;
  net->lobby_match_started = packet->match_started != 0;
  net->lobby_roster_received = true;
  if (count > 0) {
    memcpy(net->lobby_roster, packet->players, (size_t)count * sizeof(net->lobby_roster[0]));
  }
}

static void HandlePong(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomPongPacket* packet = (const ShroomPongPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  CompletePendingPing(net, packet->nonce, enet_time_get());
}

static void HandleIntermissionStatus(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomIntermissionStatusPacket* packet =
      (const ShroomIntermissionStatusPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  if ((net->consumed_intermission_round_valid &&
       !ShroomIntermissionRoundIsNewer(packet->round_id, net->consumed_intermission_round_id)) ||
      (net->intermission_received && packet->round_id != net->intermission.round_id &&
       !ShroomIntermissionRoundIsNewer(packet->round_id, net->intermission.round_id))) {
    return;
  }
  net->intermission = *packet;
  net->intermission_received = true;
}

static void HandleSnapshot(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomSnapshotPacket* packet = (const ShroomSnapshotPacket*)enet_packet->data;
  uint16_t player_count;
  const size_t min_size = offsetof(ShroomSnapshotPacket, players);

  if (enet_packet->dataLength < min_size) {
    return;
  }
  player_count = packet->player_count;
  if (player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) {
    player_count = SHROOM_MAX_SNAPSHOT_PLAYERS;
  }
  if (enet_packet->dataLength < min_size + (size_t)player_count * sizeof(packet->players[0])) {
    return;
  }
  if (net->snapshot_received && (packet->tick <= net->last_snapshot_tick)) {
    return;
  }

  net->last_snapshot_tick = packet->tick;
  net->snapshot_received = true;
  net->last_processed_input_sequence = packet->last_processed_input_sequence;
  net->match_phase = packet->match_phase;
  net->game_mode = packet->game_mode;
  net->match_time_remaining = packet->match_time_remaining;
  net->objective_target_score = packet->objective_target_score;
  net->objective_controller_id = packet->objective_controller_id;
  net->objective_contested = packet->objective_contested != 0u;
  for (uint32_t i = 0; i < SHROOM_MATCH_PODIUM_COUNT; ++i) {
    net->podium_player_ids[i] = packet->podium_player_ids[i];
    net->podium_masses[i] = packet->podium_masses[i];
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

  ChatMessage incoming = {.sender_id = packet->sender_id, .timestamp_sec = (uint32_t)time(NULL)};
  memcpy(incoming.sender_name, packet->sender_name, sizeof(incoming.sender_name));
  incoming.sender_name[sizeof(incoming.sender_name) - 1u] = '\0';
  ShroomChatCacheSanitizeText(incoming.sender_name, sizeof(incoming.sender_name),
                              incoming.sender_name);

  msg_len = sizeof(packet->message);
  memcpy(incoming.message, packet->message, msg_len);
  incoming.message[sizeof(incoming.message) - 1u] = '\0';
  ShroomChatCacheSanitizeText(incoming.message, sizeof(incoming.message), incoming.message);
  if ((incoming.sender_name[0] == '\0') || (incoming.message[0] == '\0')) {
    return;
  }
  for (uint32_t index = 0u; index < net->chat_history_count; ++index) {
    const uint32_t history_index = (net->chat_history_head + SHROOM_CLIENT_CHAT_HISTORY_COUNT -
                                    net->chat_history_count + index) %
                                   SHROOM_CLIENT_CHAT_HISTORY_COUNT;
    if (ShroomChatCacheMessagesDuplicate(&net->chat_history[history_index], &incoming)) {
      return;
    }
  }

  slot = &net->chat_history[net->chat_history_head % SHROOM_CLIENT_CHAT_HISTORY_COUNT];
  *slot = incoming;

  net->chat_history_head = (net->chat_history_head + 1u) % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
  if (net->chat_history_count < SHROOM_CLIENT_CHAT_HISTORY_COUNT) {
    net->chat_history_count += 1u;
  }
  net->chat_unread_count += 1u;
  if ((net->lobby_id != 0u) && (net->server_host[0] != '\0')) {
    ShroomChatCacheKey key = {.port = net->server_port, .lobby_id = net->lobby_id};
    snprintf(key.host, sizeof(key.host), "%s", net->server_host);
    ShroomChatCacheStoreMessage(net->chat_cache_path, &key, &incoming, incoming.timestamp_sec);
  }
}

static void HandleSporeState(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomSporeStatePacket* packet = (const ShroomSporeStatePacket*)enet_packet->data;
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
  if ((chunk_start_index >= SHROOM_MAX_SPORES) || (chunk_start_index >= total_spore_count)) {
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

static void HandlePowerupState(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomPowerupStatePacket* packet = (const ShroomPowerupStatePacket*)enet_packet->data;
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

static void HandleMushroomSpeciesCatalog(ClientNetState* net, const ENetPacket* enet_packet) {
  const ShroomMushroomSpeciesCatalogPacket* packet =
      (const ShroomMushroomSpeciesCatalogPacket*)enet_packet->data;
  const size_t min_size = offsetof(ShroomMushroomSpeciesCatalogPacket, species);
  uint8_t species_count;

  if (enet_packet->dataLength < min_size) {
    return;
  }

  species_count = packet->species_count;
  if (species_count > SHROOM_MAX_MUSHROOM_SPECIES) {
    species_count = SHROOM_MAX_MUSHROOM_SPECIES;
  }
  if (enet_packet->dataLength < min_size + (size_t)species_count * sizeof(packet->species[0])) {
    return;
  }

  net->mushroom_species_count = species_count;
  net->mushroom_species_catalog_received = true;
  if (species_count > 0) {
    memcpy(net->mushroom_species, packet->species,
           (size_t)species_count * sizeof(net->mushroom_species[0]));
    for (uint8_t index = 0; index < species_count; ++index) {
      net->mushroom_species[index].name[sizeof(net->mushroom_species[index].name) - 1u] = '\0';
      net->mushroom_species[index]
          .description[sizeof(net->mushroom_species[index].description) - 1u] = '\0';
    }
  }
}

bool ClientNetInit(ClientNetState* net, const char* host_name, uint16_t port,
                   const char* player_name) {
  ENetAddress address = {0};

  if ((net->peer != 0) || (net->host != 0) || net->enet_initialized) {
    ClientNetShutdown(net);
  }
  *net = (ClientNetState){0};
  ShroomSanitizePlayerName(net->player_name, player_name);
  if (net->player_name[0] == '\0') {
    snprintf(net->player_name, sizeof(net->player_name), "%s", "Player");
  }
  snprintf(net->server_host, sizeof(net->server_host), "%s", host_name != NULL ? host_name : "");
  net->server_port = port;
  snprintf(net->chat_cache_path, sizeof(net->chat_cache_path), "%s",
           SHROOM_CHAT_CACHE_DEFAULT_PATH);

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

  net->peer = enet_host_connect(net->host, &address, SHROOM_ENET_CHANNEL_COUNT, 0);
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

void ClientNetUpdate(ClientNetState* net, ShroomVec2 input_direction, bool split_requested,
                     bool eject_requested, ShroomVec2 split_direction, uint32_t focused_entity_id,
                     float delta_time) {
  ENetEvent event;
  ShroomClientScheduledActions actions;

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
        const ShroomPacketType type = (ShroomPacketType)header->type;
        const size_t minimum_size = ShroomPacketTypeMinimumSize(type);
        if ((minimum_size == 0u) || (event.packet->dataLength < minimum_size) ||
            !ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
          ShroomNetTelemetryRecordDrop(&net->telemetry, 0u, event.channelID, type,
                                       event.packet->dataLength, enet_time_get());
          enet_packet_destroy(event.packet);
          break;
        }
        ShroomNetTelemetryRecordAccepted(&net->telemetry, 0u, event.channelID, type,
                                         event.packet->dataLength, enet_time_get());
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
        case SHROOM_PACKET_INTERMISSION_STATUS:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleIntermissionStatus(net, event.packet);
          }
          break;
        case SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG:
          if (ShroomPacketHeaderUsesExpectedChannel(header, event.channelID)) {
            HandleMushroomSpeciesCatalog(net, event.packet);
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
        case SHROOM_PACKET_REMATCH_VOTE:
        default:
          break;
        }
      } else {
        ShroomNetTelemetryRecordDrop(&net->telemetry, 0u, event.channelID, (ShroomPacketType)0,
                                     event.packet->dataLength, enet_time_get());
      }
      enet_packet_destroy(event.packet);
    } break;
    case ENET_EVENT_TYPE_DISCONNECT:
      net->peer = 0;
      net->welcome_received = false;
      net->match_entry_sent = false;
      ShroomClientInputSchedulerPause(&net->input_scheduler);
      SetStatus(net, CLIENT_NET_DISCONNECTED, "Disconnected");
      break;
    case ENET_EVENT_TYPE_NONE:
    default:
      break;
    }
  }

  if (net->peer != NULL) {
    const size_t queued = enet_list_size(&net->peer->outgoingCommands) +
                          enet_list_size(&net->peer->outgoingSendReliableCommands);
    const uint32_t bounded_queue = queued > UINT32_MAX ? UINT32_MAX : (uint32_t)queued;
    const uint16_t loss_basis_points =
        (uint16_t)(((uint64_t)net->peer->packetLoss * 10000u) / ENET_PEER_PACKET_LOSS_SCALE);
    ShroomNetTelemetrySetPeerTransport(&net->telemetry, 0u, bounded_queue, loss_basis_points, true);
  } else {
    ShroomNetTelemetrySetPeerTransport(&net->telemetry, 0u, 0u, 0u, false);
  }

  CheckConnectTimeout(net, enet_time_get());

  if (net->peer != 0) {
    ClearStalePendingPing(net, enet_time_get());
    if (CanSendGameplayInput(net)) {
      ShroomClientInputSchedulerQueueActions(&net->input_scheduler, split_requested,
                                             eject_requested, split_direction, focused_entity_id);
      if (ShroomClientInputSchedulerPrepare(&net->input_scheduler, delta_time, &actions)) {
        const bool has_action = actions.split_requested || actions.eject_requested;
        if (SendInput(net, input_direction, actions.split_requested, actions.eject_requested,
                      has_action ? actions.direction : split_direction,
                      has_action ? actions.focused_entity_id : focused_entity_id)) {
          ShroomClientInputSchedulerCommit(&net->input_scheduler, &actions);
        }
      }
    } else {
      ShroomClientInputSchedulerPause(&net->input_scheduler);
    }
    net->ping_send_accumulator += delta_time;
    while (net->ping_send_accumulator >= SHROOM_CLIENT_PING_INTERVAL_SECONDS) {
      SendPing(net);
      net->ping_send_accumulator -= SHROOM_CLIENT_PING_INTERVAL_SECONDS;
    }
    enet_host_flush(net->host);
  }
}

void ClientNetSendRematchVote(ClientNetState* net, ShroomRematchVote vote) {
  ShroomRematchVotePacket packet;

  if ((net == NULL) || (net->peer == NULL) || !net->intermission_received ||
      !net->intermission.can_vote || net->intermission.resolved) {
    return;
  }
  packet = (ShroomRematchVotePacket){.round_id = net->intermission.round_id, .vote = (uint8_t)vote};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_REMATCH_VOTE, sizeof(packet));
  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_REMATCH_VOTE,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_REMATCH_VOTE));
}

void ClientNetConsumeIntermission(ClientNetState* net) {
  if ((net == NULL) || !net->intermission_received) {
    return;
  }
  net->consumed_intermission_round_id = net->intermission.round_id;
  net->consumed_intermission_round_valid = true;
  net->intermission_received = false;
}

#ifdef TEST_MODE
void ClientNetTestBuildHello(const ClientNetState* net, ShroomHelloPacket* packet) {
  if ((net != NULL) && (packet != NULL)) {
    BuildHelloPacket(net, packet);
  }
}
bool ClientNetTestCompletePendingPing(ClientNetState* net, uint32_t nonce, uint32_t now_ms) {
  return CompletePendingPing(net, nonce, now_ms);
}

void ClientNetTestClearStalePendingPing(ClientNetState* net, uint32_t now_ms) {
  ClearStalePendingPing(net, now_ms);
}

void ClientNetTestCheckConnectTimeout(ClientNetState* net, uint32_t now_ms) {
  CheckConnectTimeout(net, now_ms);
}

void ClientNetTestHandleSnapshot(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleSnapshot(net, enet_packet);
}

void ClientNetTestHandleSporeState(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleSporeState(net, enet_packet);
}

void ClientNetTestHandleLobbyList(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleLobbyList(net, enet_packet);
}

void ClientNetTestHandleLobbyRoster(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleLobbyRoster(net, enet_packet);
}

void ClientNetTestHandleLobbyJoined(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleLobbyJoined(net, enet_packet);
}

void ClientNetTestHandleChat(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleChat(net, enet_packet);
}

void ClientNetTestHandleIntermissionStatus(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleIntermissionStatus(net, enet_packet);
}

bool ClientNetTestCanSendGameplayInput(const ClientNetState* net) {
  return CanSendGameplayInput(net);
}

void ClientNetTestHandleMushroomSpeciesCatalog(ClientNetState* net, const ENetPacket* enet_packet) {
  HandleMushroomSpeciesCatalog(net, enet_packet);
}
#endif

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
  *net = (ClientNetState){0};
}

bool ClientNetCanResumeLobbySession(const ClientNetState* net) {
  if ((net == NULL) || !net->enet_initialized || (net->host == NULL) || (net->peer == NULL) ||
      (net->status != CLIENT_NET_CONNECTED) || (net->lobby_id == 0u) || !net->match_entry_sent) {
    return false;
  }

  if (net->spectating) {
    return true;
  }

  return net->welcome_received && (net->player_id != 0u) && (net->entity_id != 0u);
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
  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_LIST_QUERY,
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
  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_JOIN,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_JOIN));
}

void ClientNetSendLobbyLeave(ClientNetState* net) {
  ShroomLobbyLeavePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_LEAVE, sizeof(packet));
  packet.lobby_id = net->lobby_id;
  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_LEAVE,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_LEAVE));
  net->welcome_received = false;
  net->match_entry_sent = false;
  net->lobby_roster_received = false;
  net->lobby_match_started = false;
  net->lobby_roster_count = 0;
  net->lobby_id = 0;
  net->spectating = false;
  ResetIntermissionLifecycle(net);
}

void ClientNetSendLobbyCreate(ClientNetState* net, const char* name, uint16_t max_players,
                              ShroomGameMode game_mode) {
  ShroomLobbyCreatePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED)) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_CREATE, sizeof(packet));
  if (name != NULL && name[0] != '\0') {
    snprintf(packet.name, sizeof(packet.name), "%s", name);
  }
  packet.max_players = max_players;
  packet.game_mode = (uint8_t)game_mode;
  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_CREATE,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_CREATE));
}

void ClientNetSendReadyState(ClientNetState* net, bool is_ready) {
  ShroomReadyStatePacket packet = {0};

  if ((net == NULL) || (net->peer == NULL) || (net->peer->state != ENET_PEER_STATE_CONNECTED) ||
      !net->welcome_received) {
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_READY_STATE, sizeof(packet));
  packet.player_id = net->player_id;
  packet.is_ready = is_ready ? 1 : 0;
  SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_READY_STATE,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_READY_STATE));
}

void ClientNetSendEnterMatch(ClientNetState* net) {
  ShroomEnterMatchPacket packet = {0};

  if (net == NULL || (!net->welcome_received && !net->spectating) || net->lobby_id == 0) {
    return;
  }
  if (net->peer == NULL || net->peer->state != ENET_PEER_STATE_CONNECTED) {
#ifdef TEST_MODE
    net->match_entry_sent = true;
#endif
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_ENTER_MATCH, sizeof(packet));
  packet.lobby_id = net->lobby_id;
  if (SendPacket(net, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_ENTER_MATCH,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_ENTER_MATCH))) {
    net->match_entry_sent = true;
    enet_host_flush(net->host);
  }
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

  return SendPacket(net, SHROOM_ENET_CHANNEL_CHAT, SHROOM_PACKET_CHAT,
                    CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_CHAT));
}
