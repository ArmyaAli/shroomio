#ifndef SHROOM_SNAPSHOT_REPLICATION_H
#define SHROOM_SNAPSHOT_REPLICATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"
#include "world.h"

#define SHROOM_SNAPSHOT_INTEREST_HYSTERESIS 240.0f
#define SHROOM_SNAPSHOT_HISTORY_SIZE 4u

typedef struct ShroomSnapshotInterestState {
  ShroomEntityId known_entity_ids[SHROOM_MAX_PLAYER_ENTITIES];
} ShroomSnapshotInterestState;

typedef struct ShroomSnapshotFrameMetadata {
  uint64_t tick;
  uint32_t last_processed_input_sequence;
  uint32_t player_id;
  uint32_t entity_id;
  uint8_t match_phase;
  uint8_t game_mode;
  float match_time_remaining;
  float objective_target_score;
  uint32_t objective_controller_id;
  uint8_t objective_contested;
  uint32_t podium_player_ids[SHROOM_MATCH_PODIUM_COUNT];
  float podium_masses[SHROOM_MATCH_PODIUM_COUNT];
} ShroomSnapshotFrameMetadata;

typedef struct ShroomSnapshotFrame {
  uint64_t tick;
  uint16_t player_count;
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS];
} ShroomSnapshotFrame;

typedef struct ShroomSnapshotHistory {
  uint8_t next;
  ShroomSnapshotFrame frames[SHROOM_SNAPSHOT_HISTORY_SIZE];
} ShroomSnapshotHistory;

typedef struct ShroomSnapshotEncodedFrame {
  uint16_t packet_count;
  size_t wire_bytes;
  ShroomSnapshotPacket packets[SHROOM_SNAPSHOT_MAX_CHUNKS];
} ShroomSnapshotEncodedFrame;

typedef struct ShroomSnapshotAssembly {
  bool active;
  uint64_t tick;
  uint64_t baseline_tick;
  uint32_t received_chunks;
  uint16_t chunk_count;
  uint16_t total_player_count;
  uint8_t flags;
  ShroomSnapshotFrameMetadata metadata;
  ShroomSnapshotPacket chunks[SHROOM_SNAPSHOT_MAX_CHUNKS];
} ShroomSnapshotAssembly;

typedef enum ShroomSnapshotAssemblyResult {
  SHROOM_SNAPSHOT_ASSEMBLY_REJECTED = 0,
  SHROOM_SNAPSHOT_ASSEMBLY_PENDING = 1,
  SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE = 2,
} ShroomSnapshotAssemblyResult;

size_t ShroomSnapshotSelectPlayers(ShroomSnapshotInterestState* state,
                                   const ShroomWorldState* world, ShroomPlayerId viewer_player_id,
                                   ShroomEntityId focus_entity_id, bool spectator,
                                   uint16_t* selected_indices, size_t selected_capacity);
size_t ShroomSnapshotChunkCount(size_t player_count);
bool ShroomSnapshotTickIsNewer(uint64_t candidate, uint64_t reference);
const ShroomSnapshotFrame* ShroomSnapshotHistoryFind(const ShroomSnapshotHistory* history,
                                                     uint64_t tick);
void ShroomSnapshotHistoryStore(ShroomSnapshotHistory* history, uint64_t tick,
                                const ShroomSnapshotPlayerState* players, uint16_t player_count);
bool ShroomSnapshotEncodeFrame(const ShroomSnapshotFrameMetadata* metadata,
                               const ShroomSnapshotPlayerState* players, uint16_t player_count,
                               const ShroomSnapshotFrame* baseline, bool keyframe,
                               ShroomSnapshotEncodedFrame* encoded);
ShroomSnapshotAssemblyResult ShroomSnapshotAssemblyPush(
    ShroomSnapshotAssembly* assembly, const ShroomSnapshotPacket* packet, size_t packet_size,
    ShroomSnapshotHistory* history, ShroomSnapshotFrameMetadata* completed_metadata,
    ShroomSnapshotPlayerState* completed_players, uint16_t* completed_player_count);

#endif
