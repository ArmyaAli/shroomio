#ifndef SHROOM_CLIENT_NET_H
#define SHROOM_CLIENT_NET_H

#include <stdbool.h>
#include <stdint.h>

#include <enet/enet.h>

#include "shared/protocol.h"
#include "shared/vec2.h"

typedef enum ClientNetStatus {
    CLIENT_NET_DISCONNECTED = 0,
    CLIENT_NET_CONNECTING = 1,
    CLIENT_NET_CONNECTED = 2,
    CLIENT_NET_ERROR = 3,
} ClientNetStatus;

typedef struct ClientNetState {
    ENetHost *host;
    ENetPeer *peer;
    ClientNetStatus status;
    bool enet_initialized;
    bool welcome_received;
    uint32_t player_id;
    uint32_t entity_id;
    uint32_t last_input_sequence;
    uint64_t last_snapshot_tick;
    uint16_t snapshot_player_count;
    ShroomSnapshotPlayerState snapshot_players[SHROOM_MAX_SNAPSHOT_PLAYERS];
    float input_send_accumulator;
    char status_text[64];
} ClientNetState;

bool ClientNetInit(ClientNetState *net, const char *host_name, uint16_t port);
void ClientNetUpdate(ClientNetState *net, ShroomVec2 input_direction, float delta_time);
void ClientNetShutdown(ClientNetState *net);
const char *ClientNetStatusLabel(const ClientNetState *net);

#endif
