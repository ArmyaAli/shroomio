#include "results_transition.h"

bool ShroomIntermissionRoundIsNewer(uint32_t candidate, uint32_t reference) {
  return (int32_t)(candidate - reference) > 0;
}

ShroomResultsRoute ShroomResultsResolveRoute(ShroomMatchPhase phase, bool status_received,
                                             const ShroomIntermissionStatusPacket* status,
                                             bool consumed_round_valid,
                                             uint32_t consumed_round_id) {
  const bool current_status = status_received && (status != NULL) &&
                              (!consumed_round_valid ||
                               ShroomIntermissionRoundIsNewer(status->round_id, consumed_round_id));

  if (current_status) {
    if (!status->resolved) {
      return SHROOM_RESULTS_ROUTE_WAIT;
    }
    if (status->decision == SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY) {
      return SHROOM_RESULTS_ROUTE_LOBBY;
    }
    if (status->decision == SHROOM_REMATCH_VOTE_SPECTATE) {
      return SHROOM_RESULTS_ROUTE_SPECTATE;
    }
    if (status->decision != SHROOM_REMATCH_VOTE_PLAY_AGAIN) {
      return SHROOM_RESULTS_ROUTE_WAIT;
    }
  }

  if (phase == SHROOM_MATCH_PHASE_RUNNING) {
    return SHROOM_RESULTS_ROUTE_GAME;
  }
  return SHROOM_RESULTS_ROUTE_WAIT;
}
