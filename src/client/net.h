#ifndef SHROOM_CLIENT_NET_H
#define SHROOM_CLIENT_NET_H

#include <stdbool.h>
#include <stdint.h>

#include <enet/enet.h>

#include "shared/protocol.h"
#include "shared/vec2.h"

typedef struct ChatMessage {
  uint32_t sender_id;
  uint32_t timestamp_sec; /* local wall-clock time at receive */
  char sender_name[SHROOM_MAX_NAME_LENGTH];
  char message[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
} ChatMessage;

#define SHROOM_CLIENT_PING_INTERVAL_SECONDS 1.0f
#define SHROOM_CLIENT_PING_TIMEOUT_MS 2000u
/* How long the client waits for the server to accept the UDP connection (and
 * finish the WELCOME handshake) before showing a friendly error instead of
 * spinning on the connecting modal. */
#define SHROOM_CLIENT_CONNECT_TIMEOUT_MS 5000u
/* Friendly message shown when Play Online / Quick Play cannot reach a server. */
#define SHROOM_NET_CONNECT_UNREACHABLE_MSG "Unable to connect to that server, does it exist?"
#define SHROOM_CLIENT_RTT_SAMPLE_COUNT 10u
#define SHROOM_CLIENT_CHAT_HISTORY_COUNT 50u
typedef enum ClientNetStatus {
  CLIENT_NET_DISCONNECTED = 0,
  CLIENT_NET_CONNECTING = 1,
  CLIENT_NET_CONNECTED = 2,
  CLIENT_NET_ERROR = 3,
} ClientNetStatus;

typedef struct ClientNetState {
  ENetHost* host;
  ENetPeer* peer;
  ClientNetStatus status;
  bool enet_initialized;
  bool welcome_received;
  uint32_t player_id;
  uint32_t entity_id;
  uint32_t last_input_sequence;
  uint32_t last_processed_input_sequence;
  uint32_t next_ping_nonce;
  uint32_t pending_ping_nonce;
  uint32_t pending_ping_sent_time_ms;
  uint32_t connect_started_ms;
  uint32_t rtt_ms;
  uint32_t rtt_average_ms;
  uint32_t rtt_samples[SHROOM_CLIENT_RTT_SAMPLE_COUNT];
  uint32_t rtt_sample_count;
  uint32_t rtt_sample_index;
  uint64_t last_snapshot_tick;
  uint16_t snapshot_player_count;
  uint16_t spore_count;
  uint16_t powerup_count;
  uint8_t mushroom_species_count;
  bool mushroom_species_catalog_received;
  ShroomSnapshotPlayerState snapshot_players[SHROOM_MAX_SNAPSHOT_PLAYERS];
  ShroomSnapshotSporeState snapshot_spores[SHROOM_MAX_SPORES];
  ShroomSnapshotPowerupState snapshot_powerups[SHROOM_MAX_POWERUPS];
  ShroomMushroomSpeciesEntry mushroom_species[SHROOM_MAX_MUSHROOM_SPECIES];
  float input_send_accumulator;
  float ping_send_accumulator;
  char status_text[64];
  ChatMessage chat_history[SHROOM_CLIENT_CHAT_HISTORY_COUNT];
  uint32_t chat_history_count;
  uint32_t chat_history_head;
  uint32_t chat_unread_count;
  /* Lobby state */
  bool handshake_received; /* set on WELCOME (version ack) */
  bool spectating;
  bool match_entry_sent;
  bool lobby_roster_received;
  bool lobby_match_started;
  uint32_t lobby_id;
  char lobby_name[SHROOM_LOBBY_MAX_NAME_LENGTH];
  uint16_t lobby_max_players;
  float world_width;
  float world_height;
  uint8_t lobby_count;
  ShroomLobbyEntry lobby_list[SHROOM_MAX_LOBBIES];
  uint16_t lobby_roster_count;
  ShroomLobbyRosterEntry lobby_roster[SHROOM_MAX_PLAYERS];
  /* Match timer state */
  uint8_t match_phase;
  float match_time_remaining;
  uint32_t podium_player_ids[SHROOM_MATCH_PODIUM_COUNT];
  float podium_masses[SHROOM_MATCH_PODIUM_COUNT];
} ClientNetState;

bool ClientNetInit(ClientNetState* net, const char* host_name, uint16_t port);
void ClientNetUpdate(ClientNetState* net, ShroomVec2 input_direction, bool split_requested,
                     bool eject_requested, ShroomVec2 split_direction, uint32_t focused_entity_id,
                     float delta_time);
void ClientNetShutdown(ClientNetState* net);
bool ClientNetCanResumeLobbySession(const ClientNetState* net);
const char* ClientNetStatusLabel(const ClientNetState* net);
bool ClientNetSendChat(ClientNetState* net, uint32_t player_id, const char* sender_name,
                       const char* message);
void ClientNetSendLobbyListQuery(ClientNetState* net);
void ClientNetSendLobbyJoin(ClientNetState* net, uint32_t lobby_id, bool spectate);
void ClientNetSendLobbyLeave(ClientNetState* net);
void ClientNetSendLobbyCreate(ClientNetState* net, const char* name, uint16_t max_players);
void ClientNetSendReadyState(ClientNetState* net, bool is_ready);
void ClientNetSendEnterMatch(ClientNetState* net);

#ifdef TEST_MODE
bool ClientNetTestCompletePendingPing(ClientNetState* net, uint32_t nonce, uint32_t now_ms);
void ClientNetTestClearStalePendingPing(ClientNetState* net, uint32_t now_ms);
void ClientNetTestCheckConnectTimeout(ClientNetState* net, uint32_t now_ms);
void ClientNetTestHandleSnapshot(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleSporeState(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleLobbyList(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleLobbyRoster(ClientNetState* net, const ENetPacket* enet_packet);
bool ClientNetTestCanSendGameplayInput(const ClientNetState* net);
void ClientNetTestHandleMushroomSpeciesCatalog(ClientNetState* net, const ENetPacket* enet_packet);
#endif

#endif
