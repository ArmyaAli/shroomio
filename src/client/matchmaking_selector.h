#ifndef SHROOM_MATCHMAKING_SELECTOR_H
#define SHROOM_MATCHMAKING_SELECTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHROOM_MATCHMAKING_DEFAULT_LATENCY_WEIGHT 0.7f
#define SHROOM_MATCHMAKING_DEFAULT_LOAD_WEIGHT 0.3f
#define SHROOM_MATCHMAKING_PREFERRED_LATENCY_MS 200u

typedef struct ShroomMatchmakingWeights {
  float latency;
  float load;
} ShroomMatchmakingWeights;

typedef struct ShroomMatchmakingCandidate {
  const char* host;
  uint16_t port;
  uint16_t latency_ms;
  uint16_t player_count;
  uint16_t capacity;
  bool reachable;
} ShroomMatchmakingCandidate;

typedef struct ShroomMatchmakingSelection {
  size_t candidate_index;
  float score;
  bool high_latency_fallback;
} ShroomMatchmakingSelection;

ShroomMatchmakingWeights ShroomMatchmakingDefaultWeights(void);
bool ShroomMatchmakingWeightsAreValid(ShroomMatchmakingWeights weights);
bool ShroomMatchmakingSelect(const ShroomMatchmakingCandidate* candidates, size_t candidate_count,
                             ShroomMatchmakingWeights weights,
                             ShroomMatchmakingSelection* selection);

#endif
