#ifndef SHROOM_PROTOCOL_H
#define SHROOM_PROTOCOL_H

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "intermission.h"

#define SHROOM_PROTOCOL_VERSION 18u
#define SHROOM_SERVER_PORT 7777u
#define SHROOM_DIRECTORY_PORT 7778u
#define SHROOM_DIRECTORY_PROTOCOL_VERSION 1u
#define SHROOM_DIRECTORY_MAX_ENTRIES 32u
#define SHROOM_DIRECTORY_ENTRIES_PER_PACKET 7u
#define SHROOM_DIRECTORY_SERVER_NAME_LENGTH 48u
#define SHROOM_DIRECTORY_HOST_LENGTH 64u
#define SHROOM_MAX_UNRELIABLE_PACKET_SIZE 1200u
#define SHROOM_MAX_PASSWORD_LENGTH 64u
#define SHROOM_AUTH_TOKEN_LENGTH 64u
/* Includes participant and spectator peers across all lobbies. */
#define SHROOM_SERVER_MAX_CLIENTS 256u
#define SHROOM_SNAPSHOT_RATE 15u
#define SHROOM_SNAPSHOT_RATE_MIN 15u
#define SHROOM_SNAPSHOT_RATE_MAX 20u
#define SHROOM_MAX_SNAPSHOT_PLAYERS SHROOM_MAX_PLAYER_ENTITIES
#define SHROOM_SNAPSHOT_PAYLOAD_SIZE 1100u
#define SHROOM_SNAPSHOT_MAX_CHUNKS 128u
#define SHROOM_SNAPSHOT_KEYFRAME_TICKS 30u
#define SHROOM_SPORE_STATE_RATE 5u
#define SHROOM_WORLD_REPLICATION_RATE SHROOM_SPORE_STATE_RATE
#define SHROOM_WORLD_REPLICATION_KEYFRAME_TICKS 30u
#define SHROOM_POWERUP_EFFECT_SPEED 0x0001u
#define SHROOM_POWERUP_EFFECT_SHIELD 0x0002u
#define SHROOM_POWERUP_EFFECT_MAGNET 0x0004u
#define SHROOM_POWERUP_EFFECT_DECAY_IMMUNE 0x0008u
#define SHROOM_LATENCY_WARNING_MS 150u
#define SHROOM_LATENCY_UNPLAYABLE_MS 200u
#define SHROOM_CHAT_MAX_MESSAGE_LENGTH 200u
#define SHROOM_CHAT_RATE_LIMIT_COUNT 5u
#define SHROOM_CHAT_RATE_LIMIT_WINDOW_MS 10000u
#define SHROOM_VOICE_MAX_PAYLOAD_SIZE 512u
#define SHROOM_VOICE_FLAG_START 0x01u
#define SHROOM_VOICE_FLAG_END 0x02u
#define SHROOM_VOICE_FLAG_MASK (SHROOM_VOICE_FLAG_START | SHROOM_VOICE_FLAG_END)
#define SHROOM_MAX_MUSHROOM_SPECIES 10u
#define SHROOM_MUSHROOM_SPECIES_NAME_LENGTH 32u
#define SHROOM_MUSHROOM_SPECIES_DESCRIPTION_LENGTH 128u

typedef enum ShroomPacketChannel {
  SHROOM_ENET_CHANNEL_CONTROL = 0u,
  SHROOM_ENET_CHANNEL_SNAPSHOT = 1u,
  SHROOM_ENET_CHANNEL_INPUT = 2u,
  SHROOM_ENET_CHANNEL_CHAT = 3u,
  SHROOM_ENET_CHANNEL_VOICE = 4u,
  SHROOM_ENET_CHANNEL_COUNT = 5u,
} ShroomPacketChannel;

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
  SHROOM_PACKET_CHAT = 10,
  SHROOM_PACKET_LOBBY_LIST_QUERY = 11,
  SHROOM_PACKET_LOBBY_LIST = 12,
  SHROOM_PACKET_LOBBY_JOIN = 13,
  SHROOM_PACKET_LOBBY_JOINED = 14,
  SHROOM_PACKET_LOBBY_LEAVE = 15,
  SHROOM_PACKET_LOBBY_CREATE = 16,
  SHROOM_PACKET_LOBBY_CREATED = 17,
  SHROOM_PACKET_POWERUP_STATE = 18,
  SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG = 19,
  SHROOM_PACKET_VOICE_FRAME = 20,
  SHROOM_PACKET_READY_STATE = 21,
  SHROOM_PACKET_ENTER_MATCH = 22,
  SHROOM_PACKET_LOBBY_ROSTER = 23,
  SHROOM_PACKET_REMATCH_VOTE = 24,
  SHROOM_PACKET_INTERMISSION_STATUS = 25,
  SHROOM_PACKET_DIRECTORY_HEARTBEAT = 26,
  SHROOM_PACKET_DIRECTORY_QUERY = 27,
  SHROOM_PACKET_DIRECTORY_LIST = 28,
  SHROOM_PACKET_WORLD_STATE = 29,
  SHROOM_PACKET_SNAPSHOT_ACK = 30,
  SHROOM_PACKET_SERVER_PROBE = 31,
  SHROOM_PACKET_SERVER_PROBE_RESPONSE = 32,
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
  float split_direction_x;
  float split_direction_y;
  uint8_t split_requested; /* 1 = player pressed split this tick */
  uint8_t eject_requested; /* 1 = player requested mass ejection this tick */
  uint8_t reserved[2];
  uint32_t focused_entity_id; /* entity_id of the piece being controlled; 0 = primary */
} ShroomInputPacket;

typedef struct ShroomSnapshotPlayerState {
  uint32_t player_id;
  uint32_t entity_id;
  float position_x;
  float position_y;
  float mass;
  float radius;
  char name[SHROOM_MAX_NAME_LENGTH];
  uint8_t alive;
  uint8_t is_bot;
  uint16_t effect_flags;
  uint8_t team_id;
  uint8_t piece_index;
  uint16_t round_spores;
  uint8_t round_kills;
  uint8_t life_generation;
  float objective_score;
} ShroomSnapshotPlayerState;

typedef enum ShroomSnapshotRecordOperation {
  SHROOM_SNAPSHOT_RECORD_SPAWN = 1,
  SHROOM_SNAPSHOT_RECORD_UPDATE = 2,
  SHROOM_SNAPSHOT_RECORD_DESPAWN = 3,
} ShroomSnapshotRecordOperation;

#define SHROOM_SNAPSHOT_COMPONENT_IDENTITY 0x0001u
#define SHROOM_SNAPSHOT_COMPONENT_TRANSFORM 0x0002u
#define SHROOM_SNAPSHOT_COMPONENT_MASS 0x0004u
#define SHROOM_SNAPSHOT_COMPONENT_EFFECTS 0x0008u
#define SHROOM_SNAPSHOT_COMPONENT_STATS 0x0010u
#define SHROOM_SNAPSHOT_COMPONENT_ALL 0x001fu
#define SHROOM_SNAPSHOT_FLAG_KEYFRAME 0x01u

typedef struct ShroomSnapshotRecordHeader {
  uint32_t entity_id;
  uint16_t component_mask;
  uint8_t operation;
  uint8_t size;
} ShroomSnapshotRecordHeader;

typedef struct ShroomSnapshotPacket {
  ShroomPacketHeader header;
  uint64_t tick;
  uint64_t baseline_tick;
  uint32_t last_processed_input_sequence;
  uint32_t player_id;
  uint32_t entity_id;
  uint16_t total_player_count;
  uint16_t chunk_index;
  uint16_t chunk_count;
  uint16_t record_count;
  uint16_t payload_size;
  uint8_t flags;
  uint8_t match_phase;
  uint8_t game_mode;
  uint8_t reserved;
  float match_time_remaining;
  float objective_target_score;
  uint32_t objective_controller_id;
  uint8_t objective_contested;
  uint8_t objective_reserved[3];
  uint32_t podium_player_ids[SHROOM_MATCH_PODIUM_COUNT];
  float podium_masses[SHROOM_MATCH_PODIUM_COUNT];
  uint8_t payload[SHROOM_SNAPSHOT_PAYLOAD_SIZE];
} ShroomSnapshotPacket;

typedef struct ShroomSnapshotAckPacket {
  ShroomPacketHeader header;
  uint64_t tick;
} ShroomSnapshotAckPacket;

#if defined(__cplusplus)
static_assert(sizeof(ShroomSnapshotPacket) <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
              "snapshot chunk must fit the unreliable MTU budget");
#else
_Static_assert(sizeof(ShroomSnapshotPacket) <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
               "snapshot chunk must fit the unreliable MTU budget");
#endif

typedef struct ShroomPingPacket {
  ShroomPacketHeader header;
  uint32_t nonce;
} ShroomPingPacket;

typedef struct ShroomPongPacket {
  ShroomPacketHeader header;
  uint32_t nonce;
} ShroomPongPacket;

typedef struct ShroomServerProbePacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  uint32_t generation;
  uint32_t nonce;
} ShroomServerProbePacket;

typedef struct ShroomServerProbeResponsePacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  uint32_t generation;
  uint32_t nonce;
  uint16_t player_count;
  uint16_t capacity;
} ShroomServerProbeResponsePacket;

typedef struct ShroomDirectoryServerEntry {
  uint64_t server_id;
  char name[SHROOM_DIRECTORY_SERVER_NAME_LENGTH];
  char host[SHROOM_DIRECTORY_HOST_LENGTH];
  uint16_t port;
  uint16_t player_count;
  uint16_t capacity;
  uint16_t reserved;
} ShroomDirectoryServerEntry;

typedef struct ShroomDirectoryHeartbeatPacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  ShroomDirectoryServerEntry server;
} ShroomDirectoryHeartbeatPacket;

typedef struct ShroomDirectoryQueryPacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  uint32_t generation;
} ShroomDirectoryQueryPacket;

typedef struct ShroomDirectoryListPacket {
  ShroomPacketHeader header;
  uint32_t protocol_version;
  uint32_t generation;
  uint8_t chunk_index;
  uint8_t chunk_count;
  uint8_t entry_count;
  uint8_t reserved;
  ShroomDirectoryServerEntry entries[SHROOM_DIRECTORY_ENTRIES_PER_PACKET];
} ShroomDirectoryListPacket;

#define SHROOM_DIRECTORY_LIST_HEADER_SIZE offsetof(ShroomDirectoryListPacket, entries)
#define SHROOM_DIRECTORY_LIST_PACKET_SIZE(count)                                                   \
  (SHROOM_DIRECTORY_LIST_HEADER_SIZE + ((size_t)(count) * sizeof(ShroomDirectoryServerEntry)))

#if defined(__cplusplus)
static_assert(sizeof(ShroomDirectoryListPacket) <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
              "directory list chunk must fit the unreliable MTU budget");
#else
_Static_assert(sizeof(ShroomDirectoryListPacket) <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
               "directory list chunk must fit the unreliable MTU budget");
#endif

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
  uint16_t spore_count; /* total active spores across all chunks */
  uint16_t reserved;    /* chunk start index for spore-state packets */
  ShroomSnapshotSporeState spores[SHROOM_MAX_SPORES];
} ShroomSporeStatePacket;

typedef struct ShroomSnapshotPowerupState {
  uint32_t entity_id;
  float position_x;
  float position_y;
  uint8_t type;
  uint8_t active;
  uint16_t reserved;
} ShroomSnapshotPowerupState;

typedef struct ShroomPowerupStatePacket {
  ShroomPacketHeader header;
  uint64_t tick;
  uint16_t powerup_count;
  uint16_t reserved;
  ShroomSnapshotPowerupState powerups[SHROOM_MAX_POWERUPS];
} ShroomPowerupStatePacket;

typedef enum ShroomWorldEntityKind {
  SHROOM_WORLD_ENTITY_SPORE = 1,
  SHROOM_WORLD_ENTITY_POWERUP = 2,
} ShroomWorldEntityKind;

typedef enum ShroomWorldRecordOperation {
  SHROOM_WORLD_RECORD_SPAWN = 1,
  SHROOM_WORLD_RECORD_UPDATE = 2,
  SHROOM_WORLD_RECORD_REMOVE = 3,
} ShroomWorldRecordOperation;

#define SHROOM_WORLD_STATE_FLAG_KEYFRAME 0x01u

typedef struct ShroomWorldStateRecord {
  uint32_t entity_id;
  float position_x;
  float position_y;
  uint16_t value;
  uint8_t entity_kind;
  uint8_t operation;
  uint8_t powerup_type;
  uint8_t reserved[3];
} ShroomWorldStateRecord;

typedef struct ShroomWorldStatePacket {
  ShroomPacketHeader header;
  uint64_t tick;
  uint16_t chunk_index;
  uint16_t chunk_count;
  uint8_t flags;
  uint8_t record_count;
  uint16_t reserved;
  ShroomWorldStateRecord records[1];
} ShroomWorldStatePacket;

typedef struct ShroomMushroomSpeciesEntry {
  uint8_t species_id;
  uint8_t pattern_id;
  uint8_t rarity_tier;
  uint8_t reserved;
  uint32_t cap_color_rgba;
  char name[SHROOM_MUSHROOM_SPECIES_NAME_LENGTH];
  char description[SHROOM_MUSHROOM_SPECIES_DESCRIPTION_LENGTH];
} ShroomMushroomSpeciesEntry;

typedef struct ShroomMushroomSpeciesCatalogPacket {
  ShroomPacketHeader header;
  uint8_t species_count;
  uint8_t reserved[3];
  ShroomMushroomSpeciesEntry species[SHROOM_MAX_MUSHROOM_SPECIES];
} ShroomMushroomSpeciesCatalogPacket;

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

/* One entry in a lobby list response. */
typedef struct ShroomLobbyEntry {
  uint32_t lobby_id;
  char name[SHROOM_LOBBY_MAX_NAME_LENGTH];
  uint16_t player_count;
  uint16_t bot_count;
  uint16_t max_players;
  uint16_t spectator_count;
  uint8_t is_dynamic;
  uint8_t game_mode;
  uint8_t reserved[2];
} ShroomLobbyEntry;

typedef struct ShroomLobbyListPacket {
  ShroomPacketHeader header;
  uint8_t lobby_count;
  uint8_t reserved[3];
  ShroomLobbyEntry lobbies[SHROOM_MAX_LOBBIES];
} ShroomLobbyListPacket;

typedef struct ShroomLobbyJoinPacket {
  ShroomPacketHeader header;
  uint32_t lobby_id;
  uint8_t spectate; /* 0 = play, 1 = spectate */
  uint8_t reserved[3];
} ShroomLobbyJoinPacket;

typedef struct ShroomLobbyJoinedPacket {
  ShroomPacketHeader header;
  uint32_t lobby_id;
  uint64_t chat_history_identity;
  uint32_t player_id;
  uint32_t entity_id;
  uint8_t spectating;
  uint8_t game_mode;
  uint8_t reserved[2];
  char lobby_name[SHROOM_LOBBY_MAX_NAME_LENGTH];
  uint16_t server_tick_rate;
  uint16_t snapshot_rate;
  uint16_t max_players;
  uint16_t reserved16;
  float world_width;
  float world_height;
} ShroomLobbyJoinedPacket;

typedef struct ShroomLobbyLeavePacket {
  ShroomPacketHeader header;
  uint32_t lobby_id;
} ShroomLobbyLeavePacket;

typedef struct ShroomLobbyCreatePacket {
  ShroomPacketHeader header;
  char name[SHROOM_LOBBY_MAX_NAME_LENGTH]; /* empty = auto-name */
  uint16_t max_players;
  uint8_t game_mode;
  uint8_t reserved;
} ShroomLobbyCreatePacket;

typedef struct ShroomLobbyCreatedPacket {
  ShroomPacketHeader header;
  uint32_t lobby_id;
  char name[SHROOM_LOBBY_MAX_NAME_LENGTH];
} ShroomLobbyCreatedPacket;

/* Sent client->server; server validates, then broadcasts to all peers. */
typedef struct ShroomChatPacket {
  ShroomPacketHeader header;
  uint32_t sender_id;
  uint64_t message_id;   /* authoritative server ID; zero in client requests */
  uint32_t timestamp_sec; /* authoritative server timestamp; zero in client requests */
  char sender_name[SHROOM_MAX_NAME_LENGTH];
  char message[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
} ShroomChatPacket;

typedef struct ShroomVoiceFramePacket {
  ShroomPacketHeader header;
  uint32_t sender_id; /* ignored client->server; authoritative in relayed frames */
  uint32_t stream_id; /* nonzero sender-local stream generation */
  uint32_t timestamp; /* Opus 48 kHz sample clock, wraps naturally */
  uint16_t sequence;  /* modulo-16-bit frame sequence */
  uint16_t payload_size;
  uint8_t flags;
  uint8_t reserved[3];
#if defined(__cplusplus)
  /* C++ translation units use a one-byte trailing placeholder for header compatibility. */
  uint8_t payload[1];
#else
  /* Opaque encoded Opus bytes. The server never decodes this payload. */
  uint8_t payload[];
#endif
} ShroomVoiceFramePacket;

#define SHROOM_VOICE_FRAME_HEADER_SIZE offsetof(ShroomVoiceFramePacket, payload)
#define SHROOM_VOICE_FRAME_MAX_SIZE (SHROOM_VOICE_FRAME_HEADER_SIZE + SHROOM_VOICE_MAX_PAYLOAD_SIZE)

#if defined(__cplusplus)
static_assert(SHROOM_VOICE_FRAME_MAX_SIZE <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
              "voice frames must fit the unreliable packet budget");
#else
_Static_assert(SHROOM_VOICE_FRAME_MAX_SIZE <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
               "voice frames must fit the unreliable packet budget");
#endif

typedef struct ShroomReadyStatePacket {
  ShroomPacketHeader header;
  uint32_t player_id;
  uint8_t is_ready;
  uint8_t reserved[3];
} ShroomReadyStatePacket;

typedef struct ShroomEnterMatchPacket {
  ShroomPacketHeader header;
  uint32_t lobby_id;
} ShroomEnterMatchPacket;

typedef struct ShroomLobbyRosterEntry {
  uint32_t player_id;
  uint8_t is_spectator;
  uint8_t is_ready;
  uint8_t entered_match;
  uint8_t reserved;
} ShroomLobbyRosterEntry;

#define SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET 128u
#define SHROOM_LOBBY_ROSTER_MAX_CHUNKS                                                             \
  ((SHROOM_MAX_PARTICIPANTS + SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET - 1u) /                       \
   SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET)

typedef struct ShroomLobbyRosterPacket {
  ShroomPacketHeader header;
  uint32_t lobby_id;
  uint32_t generation;
  uint16_t total_player_count;
  uint16_t chunk_index;
  uint16_t chunk_count;
  uint16_t entry_count;
  uint8_t match_started;
  uint8_t reserved[3];
  ShroomLobbyRosterEntry players[SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET];
} ShroomLobbyRosterPacket;

#define SHROOM_LOBBY_ROSTER_PACKET_SIZE(count)                                                     \
  (offsetof(ShroomLobbyRosterPacket, players) + (size_t)(count) * sizeof(ShroomLobbyRosterEntry))

#if defined(__cplusplus)
static_assert(sizeof(ShroomLobbyRosterPacket) <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
              "roster chunks must fit the application MTU");
#else
_Static_assert(sizeof(ShroomLobbyRosterPacket) <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
               "roster chunks must fit the application MTU");
#endif

typedef struct ShroomRematchVotePacket {
  ShroomPacketHeader header;
  uint32_t round_id;
  uint8_t vote;
  uint8_t reserved[3];
} ShroomRematchVotePacket;

typedef struct ShroomIntermissionStatusPacket {
  ShroomPacketHeader header;
  uint32_t round_id;
  float seconds_remaining;
  uint16_t eligible_count;
  uint16_t play_again_votes;
  uint16_t return_to_lobby_votes;
  uint16_t spectate_votes;
  uint8_t resolved;
  uint8_t decision;
  uint8_t your_vote;
  uint8_t can_vote;
} ShroomIntermissionStatusPacket;

#define SHROOM_PACKET_METADATA(X)                                                                  \
  X(SHROOM_PACKET_HELLO, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomHelloPacket))             \
  X(SHROOM_PACKET_WELCOME, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomWelcomePacket))         \
  X(SHROOM_PACKET_INPUT, SHROOM_ENET_CHANNEL_INPUT, false, sizeof(ShroomInputPacket))              \
  X(SHROOM_PACKET_SNAPSHOT, SHROOM_ENET_CHANNEL_SNAPSHOT, false,                                   \
    offsetof(ShroomSnapshotPacket, payload))                                                       \
  X(SHROOM_PACKET_SNAPSHOT_ACK, SHROOM_ENET_CHANNEL_INPUT, false, sizeof(ShroomSnapshotAckPacket)) \
  X(SHROOM_PACKET_PING, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomPingPacket))               \
  X(SHROOM_PACKET_PONG, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomPongPacket))               \
  X(SHROOM_PACKET_SERVER_PROBE, SHROOM_ENET_CHANNEL_CONTROL, true,                                 \
    sizeof(ShroomServerProbePacket))                                                               \
  X(SHROOM_PACKET_SERVER_PROBE_RESPONSE, SHROOM_ENET_CHANNEL_CONTROL, true,                        \
    sizeof(ShroomServerProbeResponsePacket))                                                       \
  X(SHROOM_PACKET_SPORE_STATE, SHROOM_ENET_CHANNEL_SNAPSHOT, false,                                \
    offsetof(ShroomSporeStatePacket, spores))                                                      \
  X(SHROOM_PACKET_AUTH_REQUEST, SHROOM_ENET_CHANNEL_CONTROL, true,                                 \
    sizeof(ShroomAuthRequestPacket))                                                               \
  X(SHROOM_PACKET_AUTH_RESPONSE, SHROOM_ENET_CHANNEL_CONTROL, true,                                \
    sizeof(ShroomAuthResponsePacket))                                                              \
  X(SHROOM_PACKET_CHAT, SHROOM_ENET_CHANNEL_CHAT, true, sizeof(ShroomChatPacket))                  \
  X(SHROOM_PACKET_VOICE_FRAME, SHROOM_ENET_CHANNEL_VOICE, false, SHROOM_VOICE_FRAME_HEADER_SIZE)   \
  X(SHROOM_PACKET_LOBBY_LIST_QUERY, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomPacketHeader)) \
  X(SHROOM_PACKET_LOBBY_LIST, SHROOM_ENET_CHANNEL_CONTROL, true,                                   \
    offsetof(ShroomLobbyListPacket, lobbies))                                                      \
  X(SHROOM_PACKET_LOBBY_JOIN, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomLobbyJoinPacket))    \
  X(SHROOM_PACKET_LOBBY_JOINED, SHROOM_ENET_CHANNEL_CONTROL, true,                                 \
    sizeof(ShroomLobbyJoinedPacket))                                                               \
  X(SHROOM_PACKET_LOBBY_LEAVE, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomLobbyLeavePacket))  \
  X(SHROOM_PACKET_LOBBY_CREATE, SHROOM_ENET_CHANNEL_CONTROL, true,                                 \
    sizeof(ShroomLobbyCreatePacket))                                                               \
  X(SHROOM_PACKET_LOBBY_CREATED, SHROOM_ENET_CHANNEL_CONTROL, true,                                \
    sizeof(ShroomLobbyCreatedPacket))                                                              \
  X(SHROOM_PACKET_POWERUP_STATE, SHROOM_ENET_CHANNEL_SNAPSHOT, false,                              \
    offsetof(ShroomPowerupStatePacket, powerups))                                                  \
  X(SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG, SHROOM_ENET_CHANNEL_CONTROL, true,                     \
    offsetof(ShroomMushroomSpeciesCatalogPacket, species))                                         \
  X(SHROOM_PACKET_READY_STATE, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomReadyStatePacket))  \
  X(SHROOM_PACKET_ENTER_MATCH, SHROOM_ENET_CHANNEL_CONTROL, true, sizeof(ShroomEnterMatchPacket))  \
  X(SHROOM_PACKET_LOBBY_ROSTER, SHROOM_ENET_CHANNEL_CONTROL, true,                                 \
    offsetof(ShroomLobbyRosterPacket, players))                                                    \
  X(SHROOM_PACKET_REMATCH_VOTE, SHROOM_ENET_CHANNEL_CONTROL, true,                                 \
    sizeof(ShroomRematchVotePacket))                                                               \
  X(SHROOM_PACKET_INTERMISSION_STATUS, SHROOM_ENET_CHANNEL_CONTROL, true,                          \
    sizeof(ShroomIntermissionStatusPacket))                                                        \
  X(SHROOM_PACKET_DIRECTORY_HEARTBEAT, SHROOM_ENET_CHANNEL_CONTROL, true,                          \
    sizeof(ShroomDirectoryHeartbeatPacket))                                                        \
  X(SHROOM_PACKET_DIRECTORY_QUERY, SHROOM_ENET_CHANNEL_CONTROL, true,                              \
    sizeof(ShroomDirectoryQueryPacket))                                                            \
  X(SHROOM_PACKET_DIRECTORY_LIST, SHROOM_ENET_CHANNEL_CONTROL, true,                               \
    SHROOM_DIRECTORY_LIST_HEADER_SIZE)                                                             \
  X(SHROOM_PACKET_WORLD_STATE, SHROOM_ENET_CHANNEL_SNAPSHOT, false,                                \
    offsetof(ShroomWorldStatePacket, records))

static inline uint8_t ShroomPacketTypeToChannel(ShroomPacketType type) {
  switch (type) {
#define SHROOM_PACKET_CHANNEL_CASE(packet_type, channel, reliable, minimum_size)                   \
  case packet_type:                                                                                \
    return channel;
    SHROOM_PACKET_METADATA(SHROOM_PACKET_CHANNEL_CASE)
#undef SHROOM_PACKET_CHANNEL_CASE
  default:
    return SHROOM_ENET_CHANNEL_CONTROL;
  }
}

static inline bool ShroomPacketTypeUsesReliableDelivery(ShroomPacketType type) {
  switch (type) {
#define SHROOM_PACKET_RELIABLE_CASE(packet_type, channel, reliable, minimum_size)                  \
  case packet_type:                                                                                \
    return reliable;
    SHROOM_PACKET_METADATA(SHROOM_PACKET_RELIABLE_CASE)
#undef SHROOM_PACKET_RELIABLE_CASE
  default:
    return false;
  }
}

static inline bool ShroomPacketTypeUsesUnsequencedDelivery(ShroomPacketType type) {
  return type == SHROOM_PACKET_SNAPSHOT;
}

static inline uint16_t ShroomPacketTypeMinimumSize(ShroomPacketType type) {
  switch (type) {
#define SHROOM_PACKET_MINIMUM_SIZE_CASE(packet_type, channel, reliable, minimum_size)              \
  case packet_type:                                                                                \
    return (uint16_t)(minimum_size);
    SHROOM_PACKET_METADATA(SHROOM_PACKET_MINIMUM_SIZE_CASE)
#undef SHROOM_PACKET_MINIMUM_SIZE_CASE
  default:
    return sizeof(ShroomPacketHeader);
  }
}

static inline void ShroomPacketHeaderInit(ShroomPacketHeader* header, ShroomPacketType type,
                                          uint16_t size) {
  if (header == 0) {
    return;
  }

  header->type = (uint8_t)type;
  header->reserved = ShroomPacketTypeToChannel(type);
  header->size = size;
}

static inline uint16_t ShroomSporeStatePacketMaxSpores(void) {
  const size_t header_size = offsetof(ShroomSporeStatePacket, spores);
  const size_t available = SHROOM_MAX_UNRELIABLE_PACKET_SIZE > header_size
                               ? SHROOM_MAX_UNRELIABLE_PACKET_SIZE - header_size
                               : 0u;

  return (uint16_t)(available / sizeof(ShroomSnapshotSporeState));
}

static inline uint16_t ShroomWorldStatePacketMaxRecords(void) {
  const size_t header_size = offsetof(ShroomWorldStatePacket, records);
  const size_t available = SHROOM_MAX_UNRELIABLE_PACKET_SIZE > header_size
                               ? SHROOM_MAX_UNRELIABLE_PACKET_SIZE - header_size
                               : 0u;

  return (uint16_t)(available / sizeof(ShroomWorldStateRecord));
}

static inline size_t ShroomVoiceFramePacketSize(uint16_t payload_size) {
  return SHROOM_VOICE_FRAME_HEADER_SIZE + (size_t)payload_size;
}

static inline uint8_t* ShroomVoiceFramePayload(ShroomVoiceFramePacket* packet) {
  return packet != NULL ? packet->payload : NULL;
}

static inline const uint8_t* ShroomVoiceFramePayloadConst(const ShroomVoiceFramePacket* packet) {
  return packet != NULL ? packet->payload : NULL;
}

static inline bool ShroomVoiceFramePacketIsValid(const void* data, size_t wire_size) {
  const ShroomVoiceFramePacket* packet = (const ShroomVoiceFramePacket*)data;

  if ((packet == NULL) || (wire_size < SHROOM_VOICE_FRAME_HEADER_SIZE) ||
      (wire_size > SHROOM_VOICE_FRAME_MAX_SIZE) || (wire_size > UINT16_MAX)) {
    return false;
  }
  if ((packet->header.type != SHROOM_PACKET_VOICE_FRAME) ||
      (packet->header.reserved != SHROOM_ENET_CHANNEL_VOICE) ||
      ((size_t)packet->header.size != wire_size) || (packet->stream_id == 0u) ||
      (packet->payload_size == 0u) || (packet->payload_size > SHROOM_VOICE_MAX_PAYLOAD_SIZE) ||
      (ShroomVoiceFramePacketSize(packet->payload_size) != wire_size) ||
      ((packet->flags & (uint8_t)~SHROOM_VOICE_FLAG_MASK) != 0u) || (packet->reserved[0] != 0u) ||
      (packet->reserved[1] != 0u) || (packet->reserved[2] != 0u)) {
    return false;
  }
  return true;
}

static inline bool ShroomPacketHeaderUsesExpectedChannel(const ShroomPacketHeader* header,
                                                         uint8_t actual_channel) {
  if (header == 0) {
    return false;
  }

  return (header->reserved == actual_channel) &&
         (header->reserved == ShroomPacketTypeToChannel((ShroomPacketType)header->type));
}

#endif
