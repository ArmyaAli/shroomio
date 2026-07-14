#include "quick_match.h"

#include <string.h>

static bool SelectNext(ShroomQuickMatchState* state, uint64_t now_ms) {
  ShroomMatchmakingCandidate candidates[SHROOM_DIRECTORY_MAX_ENTRIES];
  size_t source_indices[SHROOM_DIRECTORY_MAX_ENTRIES];
  ShroomMatchmakingSelection selection;
  size_t available_count = 0u;

  for (size_t index = 0u; index < state->candidate_count; ++index) {
    const ShroomQuickMatchCandidate* candidate = &state->candidates[index];
    if (state->excluded[index]) {
      continue;
    }
    candidates[available_count] = (ShroomMatchmakingCandidate){
        .host = candidate->host,
        .port = candidate->port,
        .latency_ms = candidate->latency_ms,
        .player_count = candidate->player_count,
        .capacity = candidate->capacity,
        .reachable = candidate->reachable,
    };
    source_indices[available_count++] = index;
  }
  if (!ShroomMatchmakingSelect(candidates, available_count, state->weights, &selection)) {
    state->selected_index = SIZE_MAX;
    state->high_latency_fallback = false;
    state->phase = SHROOM_QUICK_MATCH_FAILED;
    state->failure = state->attempt_count > 0u ? SHROOM_QUICK_MATCH_FAILURE_EXHAUSTED
                                               : SHROOM_QUICK_MATCH_FAILURE_NO_SERVERS;
    return false;
  }
  state->selected_index = source_indices[selection.candidate_index];
  state->high_latency_fallback = selection.high_latency_fallback;
  state->preview_started_ms = now_ms;
  state->phase = SHROOM_QUICK_MATCH_PREVIEW;
  state->failure = SHROOM_QUICK_MATCH_FAILURE_NONE;
  return true;
}

void ShroomQuickMatchInit(ShroomQuickMatchState* state) {
  if (state == NULL) {
    return;
  }
  *state = (ShroomQuickMatchState){.selected_index = SIZE_MAX,
                                   .weights = ShroomMatchmakingDefaultWeights()};
}

void ShroomQuickMatchBegin(ShroomQuickMatchState* state) {
  if (state == NULL) {
    return;
  }
  ShroomQuickMatchInit(state);
  state->phase = SHROOM_QUICK_MATCH_FINDING;
}

bool ShroomQuickMatchSetCandidates(ShroomQuickMatchState* state,
                                   const ShroomQuickMatchCandidate* candidates,
                                   size_t candidate_count, uint64_t now_ms) {
  bool saw_full = false;

  if ((state == NULL) || (state->phase != SHROOM_QUICK_MATCH_FINDING) ||
      ((candidate_count > 0u) && (candidates == NULL))) {
    return false;
  }
  state->candidate_count = candidate_count < SHROOM_DIRECTORY_MAX_ENTRIES
                               ? candidate_count
                               : SHROOM_DIRECTORY_MAX_ENTRIES;
  for (size_t index = 0u; index < state->candidate_count; ++index) {
    state->candidates[index] = candidates[index];
    state->candidates[index].name[sizeof(state->candidates[index].name) - 1u] = '\0';
    state->candidates[index].host[sizeof(state->candidates[index].host) - 1u] = '\0';
    saw_full = saw_full || (candidates[index].reachable && (candidates[index].capacity > 0u) &&
                            (candidates[index].player_count >= candidates[index].capacity));
  }
  if (SelectNext(state, now_ms)) {
    return true;
  }
  if ((state->attempt_count == 0u) && saw_full) {
    state->failure = SHROOM_QUICK_MATCH_FAILURE_SERVERS_FULL;
  }
  return false;
}

void ShroomQuickMatchDirectoryFailed(ShroomQuickMatchState* state) {
  if ((state != NULL) && (state->phase == SHROOM_QUICK_MATCH_FINDING)) {
    state->phase = SHROOM_QUICK_MATCH_FAILED;
    state->failure = SHROOM_QUICK_MATCH_FAILURE_DIRECTORY;
  }
}

void ShroomQuickMatchServersFull(ShroomQuickMatchState* state) {
  if ((state != NULL) && (state->phase == SHROOM_QUICK_MATCH_FINDING)) {
    state->phase = SHROOM_QUICK_MATCH_FAILED;
    state->failure = SHROOM_QUICK_MATCH_FAILURE_SERVERS_FULL;
  }
}

void ShroomQuickMatchUpdate(ShroomQuickMatchState* state, uint64_t now_ms) {
  if ((state != NULL) && (state->phase == SHROOM_QUICK_MATCH_PREVIEW) &&
      (now_ms >= state->preview_started_ms) &&
      ((now_ms - state->preview_started_ms) >= SHROOM_QUICK_MATCH_PREVIEW_MS)) {
    state->phase = SHROOM_QUICK_MATCH_CONNECTING;
    ++state->attempt_count;
  }
}

void ShroomQuickMatchConnectionFailed(ShroomQuickMatchState* state, uint64_t now_ms) {
  if ((state == NULL) || (state->phase != SHROOM_QUICK_MATCH_CONNECTING) ||
      (state->selected_index >= state->candidate_count)) {
    return;
  }
  state->excluded[state->selected_index] = true;
  SelectNext(state, now_ms);
}

void ShroomQuickMatchConnectionSucceeded(ShroomQuickMatchState* state) {
  if ((state != NULL) && (state->phase == SHROOM_QUICK_MATCH_CONNECTING)) {
    state->phase = SHROOM_QUICK_MATCH_SUCCEEDED;
    state->failure = SHROOM_QUICK_MATCH_FAILURE_NONE;
  }
}

void ShroomQuickMatchCancel(ShroomQuickMatchState* state) {
  if (ShroomQuickMatchIsActive(state)) {
    state->phase = SHROOM_QUICK_MATCH_CANCELLED;
    state->failure = SHROOM_QUICK_MATCH_FAILURE_NONE;
  }
}

const ShroomQuickMatchCandidate* ShroomQuickMatchSelected(const ShroomQuickMatchState* state) {
  if ((state == NULL) || (state->selected_index >= state->candidate_count)) {
    return NULL;
  }
  return &state->candidates[state->selected_index];
}

const char* ShroomQuickMatchStatusText(const ShroomQuickMatchState* state) {
  if (state == NULL) {
    return "Quick Match unavailable.";
  }
  switch (state->phase) {
  case SHROOM_QUICK_MATCH_FINDING:
    return "Finding the best live server...";
  case SHROOM_QUICK_MATCH_PREVIEW:
    return "Server selected. Joining shortly...";
  case SHROOM_QUICK_MATCH_CONNECTING:
    return "Connecting to selected server...";
  case SHROOM_QUICK_MATCH_SUCCEEDED:
    return "Connected.";
  case SHROOM_QUICK_MATCH_CANCELLED:
    return "Quick Match cancelled.";
  case SHROOM_QUICK_MATCH_FAILED:
    switch (state->failure) {
    case SHROOM_QUICK_MATCH_FAILURE_DIRECTORY:
      return "The server directory is unavailable. Retry or browse manually.";
    case SHROOM_QUICK_MATCH_FAILURE_SERVERS_FULL:
      return "All live servers are full. Retry or browse manually.";
    case SHROOM_QUICK_MATCH_FAILURE_EXHAUSTED:
      return "Every selected server failed to connect. Retry or browse manually.";
    case SHROOM_QUICK_MATCH_FAILURE_NO_SERVERS:
    default:
      return "No joinable live servers were found. Retry or browse manually.";
    }
  case SHROOM_QUICK_MATCH_IDLE:
  default:
    return "Quick Match ready.";
  }
}

bool ShroomQuickMatchIsActive(const ShroomQuickMatchState* state) {
  return (state != NULL) && ((state->phase == SHROOM_QUICK_MATCH_FINDING) ||
                             (state->phase == SHROOM_QUICK_MATCH_PREVIEW) ||
                             (state->phase == SHROOM_QUICK_MATCH_CONNECTING));
}
