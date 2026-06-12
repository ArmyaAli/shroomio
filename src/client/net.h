#ifndef SHROOM_CLIENT_NET_H
#define SHROOM_CLIENT_NET_H

#include <stdbool.h>
#include <stdint.h>

#include <enet/enet.h>

#include "shared/protocol.h"
#include "shared/vec2.h"

#define SHROOM_CLIENT_PING_INTERVAL_SECONDS 1.0f
#define SHROOM_CLIENT_RTT_SAMPLE_COUNT 10u
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
  uint32_t rtt_ms;
  uint32_t rtt_average_ms;
  uint32_t rtt_samples[SHROOM_CLIENT_RTT_SAMPLE_COUNT];
  uint32_t rtt_sample_count;
  uint32_t rtt_sample_index;
  uint64_t last_snapshot_tick;
  uint16_t snapshot_player_count;
  uint16_t spore_count;
  ShroomSnapshotPlayerState snapshot_players[SHROOM_MAX_SNAPSHOT_PLAYERS];
  ShroomSnapshotSporeState snapshot_spores[SHROOM_MAX_SPORES];
  float input_send_accumulator;
  float ping_send_accumulator;
  char status_text[64];
} ClientNetState;

bool ClientNetInit(ClientNetState* net, const char* host_name, uint16_t port);
void ClientNetUpdate(ClientNetState* net, ShroomVec2 input_direction, float delta_time);
void ClientNetShutdown(ClientNetState* net);
const char* ClientNetStatusLabel(const ClientNetState* net);

#endif
