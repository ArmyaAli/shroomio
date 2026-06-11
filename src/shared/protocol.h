#ifndef SHROOM_PROTOCOL_H
#define SHROOM_PROTOCOL_H

#include <stdint.h>

#include "config.h"

#define SHROOM_PROTOCOL_VERSION 1u
#define SHROOM_SERVER_PORT 7777u
#define SHROOM_MAX_NAME_LENGTH 32u
#define SHROOM_SERVER_MAX_CLIENTS 128u
#define SHROOM_ENET_CHANNEL_CONTROL 0u
#define SHROOM_ENET_CHANNEL_INPUT 1u
#define SHROOM_ENET_CHANNEL_SNAPSHOT 2u
#define SHROOM_ENET_CHANNEL_COUNT 3u
#define SHROOM_SNAPSHOT_RATE 15u
#define SHROOM_MAX_SNAPSHOT_PLAYERS 128u
#define SHROOM_SPORE_STATE_RATE 5u

typedef enum ShroomPacketType {
  SHROOM_PACKET_HELLO = 1,
  SHROOM_PACKET_WELCOME = 2,
  SHROOM_PACKET_INPUT = 3,
  SHROOM_PACKET_SNAPSHOT = 4,
  SHROOM_PACKET_PING = 5,
  SHROOM_PACKET_PONG = 6,
  SHROOM_PACKET_SPORE_STATE = 7,
} ShroomPacketType;

typedef struct ShroomPacketHeader {
  uint8_t type;
  uint8_t reserved;
  uint16_t size;
} ShroomPacketHeader;

typedef struct ShroomHelloPacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  char name[SHROOM_MAX_NAME_LENGTH];
} ShroomHelloPacket;

typedef struct ShroomWelcomePacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  uint32_t player_id;
  uint32_t entity_id;
  uint16_t server_tick_rate;
  uint16_t snapshot_rate;
  float world_width;
  float world_height;
} ShroomWelcomePacket;

typedef struct ShroomInputPacket {
  ShroomPacketHeader header;
  uint32_t sequence;
  float direction_x;
  float direction_y;
} ShroomInputPacket;

typedef struct ShroomSnapshotPlayerState {
  uint32_t player_id;
  uint32_t entity_id;
  float position_x;
  float position_y;
  float mass;
  float radius;
  uint8_t alive;
  uint8_t is_bot;
  uint16_t reserved;
} ShroomSnapshotPlayerState;

typedef struct ShroomSnapshotPacket {
  ShroomPacketHeader header;
  uint64_t tick;
  uint32_t last_processed_input_sequence;
  uint32_t player_id;
  uint32_t entity_id;
  uint16_t player_count;
  uint16_t reserved;
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS];
} ShroomSnapshotPacket;

typedef struct ShroomPingPacket {
  ShroomPacketHeader header;
  uint32_t nonce;
} ShroomPingPacket;

typedef struct ShroomPongPacket {
  ShroomPacketHeader header;
  uint32_t nonce;
} ShroomPongPacket;

typedef struct ShroomSnapshotSporeState {
  uint32_t entity_id;
  float position_x;
  float position_y;
  uint16_t value;
  uint16_t reserved;
} ShroomSnapshotSporeState;

typedef struct ShroomSporeStatePacket {
  ShroomPacketHeader header;
  uint64_t tick;
  uint16_t spore_count;
  uint16_t reserved;
  ShroomSnapshotSporeState spores[SHROOM_MAX_SPORES];
} ShroomSporeStatePacket;

#endif
