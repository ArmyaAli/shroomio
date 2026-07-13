#ifndef SHROOM_SERVER_MATCH_PERSISTENCE_H
#define SHROOM_SERVER_MATCH_PERSISTENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sqlite3.h>

#include "shared/world.h"

typedef struct ShroomPersistedParticipant {
  int64_t database_player_id;
  uint32_t runtime_player_id;
  bool disconnected;
  int final_rank;
  float final_mass;
  ShroomRoundStats round_stats;
} ShroomPersistedParticipant;

typedef struct ShroomCompletedMatch {
  const char* session_uuid;
  uint32_t lobby_id;
  uint32_t round_id;
  ShroomGameMode game_mode;
  uint64_t final_tick;
  uint32_t duration_seconds;
  uint16_t bot_count;
  uint32_t winner_runtime_player_id;
  const ShroomPersistedParticipant* participants;
  size_t participant_count;
} ShroomCompletedMatch;

typedef enum ShroomMatchPersistenceResult {
  SHROOM_MATCH_PERSISTENCE_ERROR = 0,
  SHROOM_MATCH_PERSISTENCE_SAVED,
  SHROOM_MATCH_PERSISTENCE_ALREADY_SAVED,
} ShroomMatchPersistenceResult;

ShroomMatchPersistenceResult ShroomMatchPersistenceSave(sqlite3* db,
                                                        const ShroomCompletedMatch* match);

#endif
