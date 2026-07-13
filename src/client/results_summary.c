#include "results_summary.h"

#include <stdio.h>

uint32_t ShroomResultsElapsedSeconds(float start_time, float end_time) {
  if (end_time <= start_time) {
    return 0u;
  }

  return (uint32_t)(end_time - start_time);
}

void ShroomResultsFormatDuration(uint32_t duration_seconds, char* buffer, size_t buffer_size) {
  if ((buffer == NULL) || (buffer_size == 0u)) {
    return;
  }

  snprintf(buffer, buffer_size, "%u:%02u", duration_seconds / 60u, duration_seconds % 60u);
}

const char* ShroomResultsVoteLabel(ShroomRematchVote vote) {
  switch (vote) {
  case SHROOM_REMATCH_VOTE_PLAY_AGAIN:
    return "Play Again";
  case SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY:
    return "Return to Lobby";
  case SHROOM_REMATCH_VOTE_SPECTATE:
    return "Spectate";
  case SHROOM_REMATCH_VOTE_NONE:
  default:
    return "Not cast";
  }
}

void ShroomResultsFormatVoteParticipation(uint16_t play_again_votes, uint16_t return_to_lobby_votes,
                                          uint16_t spectate_votes, uint16_t eligible_count,
                                          char* buffer, size_t buffer_size) {
  const unsigned int votes_cast =
      (unsigned int)play_again_votes + return_to_lobby_votes + spectate_votes;

  if ((buffer == NULL) || (buffer_size == 0u)) {
    return;
  }
  snprintf(buffer, buffer_size, "Participation: %u / %u eligible", votes_cast, eligible_count);
}

void ShroomResultsFormatLocalVote(ShroomRematchVote vote, bool can_vote, char* buffer,
                                  size_t buffer_size) {
  if ((buffer == NULL) || (buffer_size == 0u)) {
    return;
  }
  if (!can_vote) {
    snprintf(buffer, buffer_size, "Your vote: Not eligible");
    return;
  }
  snprintf(buffer, buffer_size, "Your vote: %s", ShroomResultsVoteLabel(vote));
}

void ShroomResultsFormatDecision(ShroomRematchVote decision, char* buffer, size_t buffer_size) {
  if ((buffer == NULL) || (buffer_size == 0u)) {
    return;
  }
  snprintf(buffer, buffer_size, "Decision: %s", ShroomResultsVoteLabel(decision));
}
