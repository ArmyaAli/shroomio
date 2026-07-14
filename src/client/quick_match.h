#ifndef SHROOM_QUICK_MATCH_H
#define SHROOM_QUICK_MATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "matchmaking_selector.h"
#include "shared/protocol.h"

#define SHROOM_QUICK_MATCH_PREVIEW_MS 750ull

typedef enum ShroomQuickMatchPhase {
  SHROOM_QUICK_MATCH_IDLE = 0,
  SHROOM_QUICK_MATCH_FINDING,
  SHROOM_QUICK_MATCH_PREVIEW,
  SHROOM_QUICK_MATCH_CONNECTING,
  SHROOM_QUICK_MATCH_SUCCEEDED,
  SHROOM_QUICK_MATCH_CANCELLED,
  SHROOM_QUICK_MATCH_FAILED,
} ShroomQuickMatchPhase;

typedef enum ShroomQuickMatchFailure {
  SHROOM_QUICK_MATCH_FAILURE_NONE = 0,
  SHROOM_QUICK_MATCH_FAILURE_DIRECTORY,
  SHROOM_QUICK_MATCH_FAILURE_NO_SERVERS,
  SHROOM_QUICK_MATCH_FAILURE_SERVERS_FULL,
  SHROOM_QUICK_MATCH_FAILURE_EXHAUSTED,
} ShroomQuickMatchFailure;

typedef struct ShroomQuickMatchCandidate {
  char name[SHROOM_DIRECTORY_SERVER_NAME_LENGTH];
  char host[SHROOM_DIRECTORY_HOST_LENGTH];
  uint16_t port;
  uint16_t latency_ms;
  uint16_t player_count;
  uint16_t capacity;
  bool reachable;
} ShroomQuickMatchCandidate;

typedef struct ShroomQuickMatchState {
  ShroomQuickMatchPhase phase;
  ShroomQuickMatchFailure failure;
  ShroomMatchmakingWeights weights;
  uint64_t preview_started_ms;
  size_t candidate_count;
  size_t selected_index;
  size_t attempt_count;
  bool high_latency_fallback;
  bool excluded[SHROOM_DIRECTORY_MAX_ENTRIES];
  ShroomQuickMatchCandidate candidates[SHROOM_DIRECTORY_MAX_ENTRIES];
} ShroomQuickMatchState;

void ShroomQuickMatchInit(ShroomQuickMatchState* state);
void ShroomQuickMatchBegin(ShroomQuickMatchState* state);
bool ShroomQuickMatchSetCandidates(ShroomQuickMatchState* state,
                                   const ShroomQuickMatchCandidate* candidates,
                                   size_t candidate_count, uint64_t now_ms);
void ShroomQuickMatchDirectoryFailed(ShroomQuickMatchState* state);
void ShroomQuickMatchServersFull(ShroomQuickMatchState* state);
void ShroomQuickMatchUpdate(ShroomQuickMatchState* state, uint64_t now_ms);
void ShroomQuickMatchConnectionFailed(ShroomQuickMatchState* state, uint64_t now_ms);
void ShroomQuickMatchConnectionSucceeded(ShroomQuickMatchState* state);
void ShroomQuickMatchCancel(ShroomQuickMatchState* state);
const ShroomQuickMatchCandidate* ShroomQuickMatchSelected(const ShroomQuickMatchState* state);
const char* ShroomQuickMatchStatusText(const ShroomQuickMatchState* state);
bool ShroomQuickMatchIsActive(const ShroomQuickMatchState* state);

#endif
