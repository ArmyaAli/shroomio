#include "intermission.h"

#include <string.h>

static void AdjustTotal(ShroomIntermissionState* state, uint8_t vote, int delta) {
  uint16_t* total = NULL;

  if (vote == SHROOM_REMATCH_VOTE_PLAY_AGAIN) {
    total = &state->play_again_votes;
  } else if (vote == SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY) {
    total = &state->return_to_lobby_votes;
  } else if (vote == SHROOM_REMATCH_VOTE_SPECTATE) {
    total = &state->spectate_votes;
  }
  if (total != NULL) {
    *total = (uint16_t)((int)*total + delta);
  }
}

void ShroomIntermissionBegin(ShroomIntermissionState* state, const uint32_t* player_ids,
                             uint16_t player_count, uint32_t round_id) {
  if (state == NULL) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->round_id = round_id;
  if (player_count > SHROOM_MAX_PARTICIPANTS) {
    player_count = SHROOM_MAX_PARTICIPANTS;
  }
  for (uint16_t index = 0; index < player_count; ++index) {
    if ((player_ids == NULL) || (player_ids[index] == 0u)) {
      continue;
    }
    state->voters[state->voter_count++] = (ShroomIntermissionVoter){
        .player_id = player_ids[index], .vote = SHROOM_REMATCH_VOTE_NONE, .eligible = true};
    ++state->eligible_count;
  }
}

bool ShroomIntermissionCastVote(ShroomIntermissionState* state, uint32_t player_id,
                                ShroomRematchVote vote) {
  if ((state == NULL) || state->resolved || (vote < SHROOM_REMATCH_VOTE_PLAY_AGAIN) ||
      (vote > SHROOM_REMATCH_VOTE_SPECTATE)) {
    return false;
  }
  for (uint16_t index = 0; index < state->voter_count; ++index) {
    ShroomIntermissionVoter* voter = &state->voters[index];
    if (voter->eligible && (voter->player_id == player_id)) {
      if (voter->vote == vote) {
        return false;
      }
      AdjustTotal(state, voter->vote, -1);
      voter->vote = (uint8_t)vote;
      AdjustTotal(state, voter->vote, 1);
      return true;
    }
  }
  return false;
}

bool ShroomIntermissionRemoveVoter(ShroomIntermissionState* state, uint32_t player_id) {
  if ((state == NULL) || state->resolved) {
    return false;
  }
  for (uint16_t index = 0; index < state->voter_count; ++index) {
    ShroomIntermissionVoter* voter = &state->voters[index];
    if (voter->eligible && (voter->player_id == player_id)) {
      AdjustTotal(state, voter->vote, -1);
      voter->eligible = false;
      --state->eligible_count;
      return true;
    }
  }
  return false;
}

bool ShroomIntermissionAllEligibleVoted(const ShroomIntermissionState* state) {
  if ((state == NULL) || (state->eligible_count == 0u)) {
    return false;
  }
  return (uint16_t)(state->play_again_votes + state->return_to_lobby_votes +
                    state->spectate_votes) == state->eligible_count;
}

ShroomRematchVote ShroomIntermissionResolve(ShroomIntermissionState* state) {
  uint16_t lobby_total;

  if (state == NULL) {
    return SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY;
  }
  if (state->resolved) {
    return (ShroomRematchVote)state->decision;
  }
  /* Non-voters default to lobby. Ties prefer lobby, then play again, then spectate. */
  lobby_total =
      (uint16_t)(state->return_to_lobby_votes + state->eligible_count - state->play_again_votes -
                 state->return_to_lobby_votes - state->spectate_votes);
  state->decision = SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY;
  if ((state->play_again_votes > lobby_total) &&
      (state->play_again_votes >= state->spectate_votes)) {
    state->decision = SHROOM_REMATCH_VOTE_PLAY_AGAIN;
  } else if ((state->spectate_votes > lobby_total) &&
             (state->spectate_votes > state->play_again_votes)) {
    state->decision = SHROOM_REMATCH_VOTE_SPECTATE;
  }
  state->resolved = true;
  return (ShroomRematchVote)state->decision;
}

bool ShroomIntermissionPlayerContinuesMatch(const ShroomIntermissionState* state,
                                            uint32_t player_id) {
  uint16_t index;

  if ((state == NULL) || !state->resolved || (player_id == 0u) ||
      (state->decision != SHROOM_REMATCH_VOTE_PLAY_AGAIN)) {
    return false;
  }
  for (index = 0u; index < state->voter_count; ++index) {
    if (state->voters[index].eligible && (state->voters[index].player_id == player_id)) {
      return true;
    }
  }
  return false;
}
