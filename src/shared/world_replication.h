#ifndef SHROOM_WORLD_REPLICATION_H
#define SHROOM_WORLD_REPLICATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"
#include "world.h"

#define SHROOM_MAX_WORLD_STATE_RECORDS (SHROOM_MAX_SPORES + SHROOM_MAX_POWERUPS)
#define SHROOM_WORLD_REPLICATION_MAX_CHUNKS 72u

typedef struct ShroomWorldReplicationPeerState {
  bool initialized;
  uint64_t last_keyframe_tick;
  bool spore_known[SHROOM_MAX_SPORES];
  bool powerup_known[SHROOM_MAX_POWERUPS];
  ShroomSnapshotSporeState spores[SHROOM_MAX_SPORES];
  ShroomSnapshotPowerupState powerups[SHROOM_MAX_POWERUPS];
} ShroomWorldReplicationPeerState;

typedef struct ShroomWorldReplicationBatch {
  uint64_t tick;
  size_t record_count;
  bool keyframe;
} ShroomWorldReplicationBatch;

typedef struct ShroomWorldReplicationClientState {
  bool tick_received;
  uint64_t latest_tick;
  bool keyframe_pending;
  uint64_t keyframe_tick;
  uint16_t keyframe_chunk_count;
  uint64_t keyframe_chunks[2];
  uint16_t keyframe_spore_count;
  uint16_t keyframe_powerup_count;
  ShroomEntityId keyframe_spores[SHROOM_MAX_SPORES];
  ShroomEntityId keyframe_powerups[SHROOM_MAX_POWERUPS];
} ShroomWorldReplicationClientState;

bool ShroomWorldReplicationInInterest(ShroomVec2 center, ShroomVec2 position);
ShroomWorldReplicationBatch ShroomWorldReplicationBuild(ShroomWorldReplicationPeerState* peer_state,
                                                        const ShroomWorldState* world,
                                                        ShroomVec2 interest_center,
                                                        bool force_keyframe,
                                                        ShroomWorldStateRecord* records,
                                                        size_t record_capacity);
bool ShroomWorldReplicationApplyPacket(ShroomWorldReplicationClientState* client_state,
                                       ShroomSnapshotSporeState* spores, uint16_t* spore_count,
                                       ShroomSnapshotPowerupState* powerups,
                                       uint16_t* powerup_count,
                                       const ShroomWorldStatePacket* packet, size_t packet_size);
size_t ShroomWorldReplicationPacketBytes(size_t record_count);
size_t ShroomWorldReplicationPacketCount(size_t record_count);

#endif
