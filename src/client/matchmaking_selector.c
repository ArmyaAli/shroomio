#include "matchmaking_selector.h"

#include <math.h>
#include <string.h>

static bool CandidateIsJoinable(const ShroomMatchmakingCandidate* candidate) {
  return (candidate != NULL) && candidate->reachable && (candidate->host != NULL) &&
         (candidate->host[0] != '\0') && (candidate->port != 0u) && (candidate->capacity != 0u) &&
         (candidate->player_count < candidate->capacity);
}

static float LoadRatio(const ShroomMatchmakingCandidate* candidate) {
  return (float)candidate->player_count / (float)candidate->capacity;
}

static bool CandidateWinsTie(const ShroomMatchmakingCandidate* candidate,
                             const ShroomMatchmakingCandidate* incumbent) {
  const float candidate_load = LoadRatio(candidate);
  const float incumbent_load = LoadRatio(incumbent);
  const int host_order = strcmp(candidate->host, incumbent->host);

  if (candidate->latency_ms != incumbent->latency_ms) {
    return candidate->latency_ms < incumbent->latency_ms;
  }
  if (candidate_load != incumbent_load) {
    return candidate_load < incumbent_load;
  }
  if (host_order != 0) {
    return host_order < 0;
  }
  return candidate->port < incumbent->port;
}

ShroomMatchmakingWeights ShroomMatchmakingDefaultWeights(void) {
  return (ShroomMatchmakingWeights){.latency = SHROOM_MATCHMAKING_DEFAULT_LATENCY_WEIGHT,
                                    .load = SHROOM_MATCHMAKING_DEFAULT_LOAD_WEIGHT};
}

bool ShroomMatchmakingWeightsAreValid(ShroomMatchmakingWeights weights) {
  return isfinite(weights.latency) && isfinite(weights.load) && (weights.latency >= 0.0f) &&
         (weights.load >= 0.0f) && isfinite(weights.latency + weights.load) &&
         ((weights.latency + weights.load) > 0.0f);
}

bool ShroomMatchmakingSelect(const ShroomMatchmakingCandidate* candidates, size_t candidate_count,
                             ShroomMatchmakingWeights weights,
                             ShroomMatchmakingSelection* selection) {
  bool use_high_latency_fallback = true;
  bool found = false;
  size_t selected_index = 0u;
  uint16_t maximum_latency = 0u;
  float selected_score = 0.0f;
  float weight_total;

  if (selection != NULL) {
    *selection = (ShroomMatchmakingSelection){.candidate_index = SIZE_MAX};
  }
  if ((candidates == NULL) || (candidate_count == 0u) || (selection == NULL) ||
      !ShroomMatchmakingWeightsAreValid(weights)) {
    return false;
  }

  for (size_t index = 0u; index < candidate_count; ++index) {
    if (CandidateIsJoinable(&candidates[index]) &&
        (candidates[index].latency_ms <= SHROOM_MATCHMAKING_PREFERRED_LATENCY_MS)) {
      use_high_latency_fallback = false;
      break;
    }
  }
  for (size_t index = 0u; index < candidate_count; ++index) {
    if (!CandidateIsJoinable(&candidates[index]) ||
        (!use_high_latency_fallback &&
         (candidates[index].latency_ms > SHROOM_MATCHMAKING_PREFERRED_LATENCY_MS))) {
      continue;
    }
    if (candidates[index].latency_ms > maximum_latency) {
      maximum_latency = candidates[index].latency_ms;
    }
  }
  if (maximum_latency == 0u) {
    maximum_latency = 1u;
  }

  weight_total = weights.latency + weights.load;
  for (size_t index = 0u; index < candidate_count; ++index) {
    const ShroomMatchmakingCandidate* candidate = &candidates[index];
    float score;

    if (!CandidateIsJoinable(candidate) ||
        (!use_high_latency_fallback &&
         (candidate->latency_ms > SHROOM_MATCHMAKING_PREFERRED_LATENCY_MS))) {
      continue;
    }
    score =
        (weights.latency / weight_total) * ((float)candidate->latency_ms / (float)maximum_latency) +
        (weights.load / weight_total) * LoadRatio(candidate);
    if (!found || (score < selected_score) ||
        ((score == selected_score) && CandidateWinsTie(candidate, &candidates[selected_index]))) {
      found = true;
      selected_index = index;
      selected_score = score;
    }
  }
  if (!found) {
    return false;
  }
  selection->candidate_index = selected_index;
  selection->score = selected_score;
  selection->high_latency_fallback = use_high_latency_fallback;
  return true;
}
