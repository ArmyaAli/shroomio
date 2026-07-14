#ifndef SHROOM_CLIENT_RESULTS_TRANSITION_H
#define SHROOM_CLIENT_RESULTS_TRANSITION_H

#include <stdbool.h>
#include <stdint.h>

#include "shared/protocol.h"
#include "shared/world.h"

typedef enum ShroomResultsRoute {
  SHROOM_RESULTS_ROUTE_WAIT = 0,
  SHROOM_RESULTS_ROUTE_GAME,
  SHROOM_RESULTS_ROUTE_LOBBY,
  SHROOM_RESULTS_ROUTE_SPECTATE,
} ShroomResultsRoute;

bool ShroomIntermissionRoundIsNewer(uint32_t candidate, uint32_t reference);
ShroomResultsRoute ShroomResultsResolveRoute(ShroomMatchPhase phase, bool status_received,
                                             const ShroomIntermissionStatusPacket* status,
                                             bool consumed_round_valid, uint32_t consumed_round_id);

#endif
