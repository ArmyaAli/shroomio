#ifndef SHROOM_CLIENT_NET_H
#define SHROOM_CLIENT_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <enet/enet.h>

#include "shared/protocol.h"
#include "shared/net_telemetry.h"
#include "shared/snapshot_replication.h"
#include "shared/vec2.h"
#include "shared/world_replication.h"

#include "chat_cache.h"
#include "input_scheduler.h"

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

typedef void (*ClientNetVoiceFrameHandler)(void* context, const void* data, size_t wire_size);

typedef struct ClientNetState {
  ENetHost* host;
  ENetPeer* peer;
  ClientNetStatus status;
  bool enet_initialized;
  bool welcome_received;
  char player_name[SHROOM_MAX_NAME_LENGTH];
  char server_host[64];
  uint16_t server_port;
  uint16_t server_tick_rate;
  uint16_t snapshot_rate;
  char chat_cache_path[256];
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
  bool snapshot_received;
  uint16_t snapshot_player_count;
  uint16_t spore_count;
  uint16_t powerup_count;
  uint8_t mushroom_species_count;
  bool mushroom_species_catalog_received;
  ShroomSnapshotPlayerState snapshot_players[SHROOM_MAX_SNAPSHOT_PLAYERS];
  ShroomSnapshotAssembly snapshot_assembly;
  ShroomSnapshotHistory snapshot_history;
  ShroomSnapshotSporeState snapshot_spores[SHROOM_MAX_SPORES];
  ShroomSnapshotPowerupState snapshot_powerups[SHROOM_MAX_POWERUPS];
  ShroomWorldReplicationClientState world_replication;
  ShroomMushroomSpeciesEntry mushroom_species[SHROOM_MAX_MUSHROOM_SPECIES];
  ShroomClientInputScheduler input_scheduler;
  float ping_send_accumulator;
  char status_text[64];
  ChatMessage chat_history[SHROOM_CLIENT_CHAT_HISTORY_COUNT];
  uint32_t chat_history_count;
  uint32_t chat_history_head;
  uint32_t chat_unread_count;
  ShroomNetTelemetry telemetry;
  /* Lobby state */
  bool handshake_received; /* set on WELCOME (version ack) */
  bool spectating;
  bool match_entry_sent;
  bool lobby_roster_received;
  bool lobby_match_started;
  uint32_t lobby_id;
  uint64_t chat_history_identity;
  char lobby_name[SHROOM_LOBBY_MAX_NAME_LENGTH];
  uint16_t lobby_max_players;
  float world_width;
  float world_height;
  uint8_t lobby_count;
  ShroomLobbyEntry lobby_list[SHROOM_MAX_LOBBIES];
  uint16_t lobby_roster_count;
  uint32_t lobby_roster_generation;
  uint16_t lobby_roster_chunk_count;
  uint32_t lobby_roster_received_chunks;
  ShroomLobbyRosterEntry lobby_roster[SHROOM_MAX_PARTICIPANTS];
  /* Match timer state */
  uint8_t match_phase;
  uint8_t game_mode;
  float match_time_remaining;
  float objective_target_score;
  uint32_t objective_controller_id;
  bool objective_contested;
  uint32_t podium_player_ids[SHROOM_MATCH_PODIUM_COUNT];
  float podium_masses[SHROOM_MATCH_PODIUM_COUNT];
  ShroomIntermissionStatusPacket intermission;
  bool intermission_received;
  uint32_t consumed_intermission_round_id;
  bool consumed_intermission_round_valid;
  ClientNetVoiceFrameHandler voice_frame_handler;
  void* voice_frame_context;
} ClientNetState;

bool ClientNetInit(ClientNetState* net, const char* host_name, uint16_t port,
                   const char* player_name);
void ClientNetUpdate(ClientNetState* net, ShroomVec2 input_direction, bool split_requested,
                     bool eject_requested, ShroomVec2 split_direction, uint32_t focused_entity_id,
                     float delta_time);
void ClientNetShutdown(ClientNetState* net);
bool ClientNetCanResumeLobbySession(const ClientNetState* net);
const char* ClientNetStatusLabel(const ClientNetState* net);
bool ClientNetSendChat(ClientNetState* net, uint32_t player_id, const char* sender_name,
                       const char* message);
bool ClientNetSendVoiceFrame(ClientNetState* net, const void* data, size_t wire_size);
void ClientNetSetVoiceFrameHandler(ClientNetState* net, ClientNetVoiceFrameHandler handler,
                                   void* context);
void ClientNetSendLobbyListQuery(ClientNetState* net);
void ClientNetSendLobbyJoin(ClientNetState* net, uint32_t lobby_id, bool spectate);
void ClientNetSendLobbyLeave(ClientNetState* net);
void ClientNetSendLobbyCreate(ClientNetState* net, const char* name, uint16_t max_players,
                              ShroomGameMode game_mode);
void ClientNetSendReadyState(ClientNetState* net, bool is_ready);
void ClientNetSendEnterMatch(ClientNetState* net);
void ClientNetSendRematchVote(ClientNetState* net, ShroomRematchVote vote);
void ClientNetConsumeIntermission(ClientNetState* net);

#ifdef TEST_MODE
void ClientNetTestBuildHello(const ClientNetState* net, ShroomHelloPacket* packet);
void ClientNetTestHandleWelcome(ClientNetState* net, const ENetPacket* enet_packet);
bool ClientNetTestCompletePendingPing(ClientNetState* net, uint32_t nonce, uint32_t now_ms);
void ClientNetTestClearStalePendingPing(ClientNetState* net, uint32_t now_ms);
void ClientNetTestCheckConnectTimeout(ClientNetState* net, uint32_t now_ms);
void ClientNetTestHandleSnapshot(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleSporeState(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleWorldState(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleLobbyList(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleLobbyRoster(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleLobbyJoined(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleChat(ClientNetState* net, const ENetPacket* enet_packet);
void ClientNetTestHandleIntermissionStatus(ClientNetState* net, const ENetPacket* enet_packet);
bool ClientNetTestCanSendGameplayInput(const ClientNetState* net);
void ClientNetTestHandleMushroomSpeciesCatalog(ClientNetState* net, const ENetPacket* enet_packet);
#endif

#endif
