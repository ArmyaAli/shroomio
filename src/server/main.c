#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <enet/enet.h>

#include "shared/protocol.h"
#include "shared/sim.h"

typedef struct ServerSession {
    bool active;
    uint32_t player_id;
    uint32_t last_processed_input_sequence;
    ShroomPlayerState *player;
} ServerSession;

static volatile sig_atomic_t g_running = 1;

static void ConfigureLogging(void) {
    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
}

static void HandleSignal(int signal_number) {
    (void)signal_number;
    g_running = 0;
}

static ShroomVec2 NormalizeInput(ShroomVec2 input) {
    const float length_sqr = ShroomVec2LengthSqr(input);
    float scale;

    if (length_sqr <= 0.0001f) {
        return (ShroomVec2){0};
    }

    scale = 1.0f / __builtin_sqrtf(length_sqr);
    return ShroomVec2Scale(input, scale);
}

static uint64_t GetTimeNanos(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((uint64_t)now.tv_sec * 1000000000ull) + (uint64_t)now.tv_nsec;
}

static void SleepUntil(uint64_t target_time_nanos) {
    struct timespec sleep_time;
    uint64_t now = GetTimeNanos();
    uint64_t delta;

    if (now >= target_time_nanos) {
        return;
    }

    delta = target_time_nanos - now;
    sleep_time.tv_sec = (time_t)(delta / 1000000000ull);
    sleep_time.tv_nsec = (long)(delta % 1000000000ull);
    nanosleep(&sleep_time, 0);
}

static ENetPacket *CreatePacket(const void *data, size_t size, enet_uint32 flags) {
    return enet_packet_create(data, size, flags);
}

static void SendWelcome(ENetPeer *peer, const ServerSession *session, const ShroomWorldState *world) {
    const ShroomWelcomePacket packet = {
        .header = {SHROOM_PACKET_WELCOME, 0, sizeof(ShroomWelcomePacket)},
        .protocol_version = SHROOM_PROTOCOL_VERSION,
        .player_id = session->player_id,
        .entity_id = session->player != 0 ? session->player->entity_id : 0,
        .server_tick_rate = (uint16_t)SHROOM_SERVER_TICK_RATE,
        .snapshot_rate = SHROOM_SNAPSHOT_RATE,
        .world_width = world->width,
        .world_height = world->height,
    };

    enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL, CreatePacket(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE));
}

static void SendPong(ENetPeer *peer, uint32_t nonce) {
    const ShroomPongPacket packet = {
        .header = {SHROOM_PACKET_PONG, 0, sizeof(ShroomPongPacket)},
        .nonce = nonce,
    };

    enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL, CreatePacket(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE));
}

static void SendSnapshot(ENetPeer *peer, const ServerSession *session, const ShroomWorldState *world) {
    ShroomSnapshotPacket packet;
    uint16_t player_count = 0;
    size_t index;

    if ((session == 0) || !session->active || (session->player == 0)) {
        return;
    }

    packet = (ShroomSnapshotPacket){
        .header = {SHROOM_PACKET_SNAPSHOT, 0, sizeof(ShroomSnapshotPacket)},
        .tick = world->tick,
        .last_processed_input_sequence = session->last_processed_input_sequence,
        .player_id = session->player_id,
        .entity_id = session->player->entity_id,
    };

    for (index = 0; (index < world->player_count) && (player_count < SHROOM_MAX_SNAPSHOT_PLAYERS); ++index) {
        const ShroomPlayerState *player = &world->players[index];

        if (!player->alive || (player->mass <= 0.0f)) {
            continue;
        }

        packet.players[player_count++] = (ShroomSnapshotPlayerState){
            .player_id = player->player_id,
            .entity_id = player->entity_id,
            .position_x = player->position.x,
            .position_y = player->position.y,
            .mass = player->mass,
            .radius = player->radius,
            .alive = 1u,
            .is_bot = player->is_bot ? 1u : 0u,
        };
    }

    packet.player_count = player_count;

    enet_peer_send(peer, SHROOM_ENET_CHANNEL_SNAPSHOT, CreatePacket(&packet, sizeof(packet), 0));
}

static void DisconnectSession(ServerSession *session) {
    if ((session == 0) || !session->active) {
        return;
    }

    if (session->player != 0) {
        session->player->alive = false;
        session->player->input_direction = (ShroomVec2){0};
        session->player->mass = 0.0f;
        session->player->radius = 0.0f;
    }

    *session = (ServerSession){0};
}

static void HandleHelloPacket(ENetPeer *peer, ServerSession *session, ShroomWorldState *world, const ENetPacket *enet_packet, uint32_t *next_player_id) {
    const ShroomHelloPacket *packet = (const ShroomHelloPacket *)enet_packet->data;

    if (enet_packet->dataLength < sizeof(*packet)) {
        return;
    }
    if (packet->protocol_version != SHROOM_PROTOCOL_VERSION) {
        enet_peer_disconnect(peer, 0);
        return;
    }
    if (session->active) {
        return;
    }

    session->player = ShroomWorldSpawnPlayer(world, (*next_player_id)++, false);
    if (session->player == 0) {
        enet_peer_disconnect(peer, 0);
        return;
    }

    session->active = true;
    session->player_id = session->player->player_id;
    SendWelcome(peer, session, world);
}

static void HandleInputPacket(ServerSession *session, const ENetPacket *enet_packet) {
    const ShroomInputPacket *packet = (const ShroomInputPacket *)enet_packet->data;

    if ((session == 0) || !session->active || (session->player == 0)) {
        return;
    }
    if (enet_packet->dataLength < sizeof(*packet)) {
        return;
    }

    session->last_processed_input_sequence = packet->sequence;
    ShroomPlayerSetInput(session->player, NormalizeInput((ShroomVec2){packet->direction_x, packet->direction_y}));
}

static void HandlePacket(ENetPeer *peer, ServerSession *session, ShroomWorldState *world, const ENetPacket *enet_packet, uint32_t *next_player_id) {
    const ShroomPacketHeader *header;

    if ((enet_packet == 0) || (enet_packet->dataLength < sizeof(ShroomPacketHeader))) {
        return;
    }

    header = (const ShroomPacketHeader *)enet_packet->data;
    switch ((ShroomPacketType)header->type) {
        case SHROOM_PACKET_HELLO:
            HandleHelloPacket(peer, session, world, enet_packet, next_player_id);
            break;
        case SHROOM_PACKET_INPUT:
            HandleInputPacket(session, enet_packet);
            break;
        case SHROOM_PACKET_PING:
            if (enet_packet->dataLength >= sizeof(ShroomPingPacket)) {
                SendPong(peer, ((const ShroomPingPacket *)enet_packet->data)->nonce);
            }
            break;
        default:
            break;
    }
}

int main(void) {
    const uint64_t tick_interval_nanos = 1000000000ull / (uint64_t)SHROOM_SERVER_TICK_RATE;
    const uint64_t snapshot_interval_ticks = (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_SNAPSHOT_RATE);
    ENetAddress address = {0};
    ENetHost *host;
    ShroomWorldState world;
    ServerSession sessions[SHROOM_SERVER_MAX_CLIENTS] = {0};
    uint32_t next_player_id = 1;
    uint64_t next_tick_time;

    ConfigureLogging();
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    if (enet_initialize() != 0) {
        fputs("failed to initialize ENet\n", stderr);
        return 1;
    }

    ShroomWorldInit(&world);
    while (world.player_count < SHROOM_BOT_COUNT) {
        ShroomWorldSpawnPlayer(&world, next_player_id++, true);
    }

    address.host = ENET_HOST_ANY;
    address.port = SHROOM_SERVER_PORT;
    host = enet_host_create(&address, SHROOM_SERVER_MAX_CLIENTS, SHROOM_ENET_CHANNEL_COUNT, 0, 0);
    if (host == 0) {
        fputs("failed to create ENet host\n", stderr);
        enet_deinitialize();
        return 1;
    }

    printf("shroomio server listening on UDP %u\n", SHROOM_SERVER_PORT);
    next_tick_time = GetTimeNanos();

    while (g_running) {
        ENetEvent event;

        while (enet_host_service(host, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    if (event.peer->incomingPeerID < SHROOM_SERVER_MAX_CLIENTS) {
                        event.peer->data = &sessions[event.peer->incomingPeerID];
                        printf("peer connected: slot=%u\n", (unsigned)event.peer->incomingPeerID);
                    } else {
                        enet_peer_disconnect(event.peer, 0);
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    HandlePacket(
                        event.peer,
                        (ServerSession *)event.peer->data,
                        &world,
                        event.packet,
                        &next_player_id
                    );
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("peer disconnected: slot=%u\n", (unsigned)event.peer->incomingPeerID);
                    DisconnectSession((ServerSession *)event.peer->data);
                    event.peer->data = 0;
                    break;
                case ENET_EVENT_TYPE_NONE:
                default:
                    break;
            }
        }

        ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);
        if ((snapshot_interval_ticks > 0) && ((world.tick % snapshot_interval_ticks) == 0)) {
            size_t index;

            for (index = 0; index < host->peerCount; ++index) {
                if (host->peers[index].state == ENET_PEER_STATE_CONNECTED) {
                    SendSnapshot(&host->peers[index], (ServerSession *)host->peers[index].data, &world);
                }
            }
            enet_host_flush(host);
        }

        next_tick_time += tick_interval_nanos;
        SleepUntil(next_tick_time);
    }

    enet_host_destroy(host);
    enet_deinitialize();
    puts("shroomio server shutting down");
    return 0;
}
