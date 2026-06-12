#ifndef SHROOM_PROTOCOL_H
#define SHROOM_PROTOCOL_H

#include <stdint.h>

#include "config.h"

#define SHROOM_PROTOCOL_VERSION 1u
#define SHROOM_SERVER_PORT 7777u
#define SHROOM_MAX_NAME_LENGTH 32u
#define SHROOM_MAX_PASSWORD_LENGTH 64u
#define SHROOM_AUTH_TOKEN_LENGTH 64u
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
  SHROOM_PACKET_AUTH_REQUEST = 8,
  SHROOM_PACKET_AUTH_RESPONSE = 9,
} ShroomPacketType;

typedef enum ShroomAuthMethod {
  SHROOM_AUTH_ANONYMOUS = 0,
  SHROOM_AUTH_PASSWORD = 1,
  SHROOM_AUTH_DISCORD = 2,
} ShroomAuthMethod;

typedef enum ShroomAuthResult {
  SHROOM_AUTH_SUCCESS = 0,
  SHROOM_AUTH_INVALID_CREDENTIALS = 1,
  SHROOM_AUTH_USERNAME_TAKEN = 2,
  SHROOM_AUTH_INVALID_TOKEN = 3,
  SHROOM_AUTH_TOKEN_EXPIRED = 4,
  SHROOM_AUTH_DATABASE_ERROR = 5,
  SHROOM_AUTH_INVALID_INPUT = 6,
} ShroomAuthResult;

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

typedef struct ShroomAuthRequestPacket {
  ShroomPacketHeader header;
  uint8_t auth_method;
  uint8_t is_register;
  uint16_t reserved;
  char username[SHROOM_MAX_NAME_LENGTH];
  char password[SHROOM_MAX_PASSWORD_LENGTH];
  char token[SHROOM_AUTH_TOKEN_LENGTH];
} ShroomAuthRequestPacket;

typedef struct ShroomAuthResponsePacket {
  ShroomPacketHeader header;
  uint8_t result;
  uint8_t reserved[3];
  uint32_t player_id;
  char token[SHROOM_AUTH_TOKEN_LENGTH];
  char message[64];
} ShroomAuthResponsePacket;

#endif
