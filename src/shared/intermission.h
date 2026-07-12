#ifndef SHROOM_INTERMISSION_H
#define SHROOM_INTERMISSION_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef enum ShroomRematchVote {
  SHROOM_REMATCH_VOTE_NONE = 0,
  SHROOM_REMATCH_VOTE_PLAY_AGAIN = 1,
  SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY = 2,
  SHROOM_REMATCH_VOTE_SPECTATE = 3,
} ShroomRematchVote;

typedef struct ShroomIntermissionVoter {
  uint32_t player_id;
  uint8_t vote;
  bool eligible;
} ShroomIntermissionVoter;

typedef struct ShroomIntermissionState {
  ShroomIntermissionVoter voters[SHROOM_MAX_PARTICIPANTS];
  uint16_t voter_count;
  uint16_t eligible_count;
  uint16_t play_again_votes;
  uint16_t return_to_lobby_votes;
  uint16_t spectate_votes;
  uint32_t round_id;
  bool resolved;
  uint8_t decision;
} ShroomIntermissionState;

void ShroomIntermissionBegin(ShroomIntermissionState* state, const uint32_t* player_ids,
                             uint16_t player_count, uint32_t round_id);
bool ShroomIntermissionCastVote(ShroomIntermissionState* state, uint32_t player_id,
                                ShroomRematchVote vote);
bool ShroomIntermissionRemoveVoter(ShroomIntermissionState* state, uint32_t player_id);
bool ShroomIntermissionAllEligibleVoted(const ShroomIntermissionState* state);
ShroomRematchVote ShroomIntermissionResolve(ShroomIntermissionState* state);

#endif
