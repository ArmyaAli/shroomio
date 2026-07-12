#ifndef SHROOM_SERVER_SESSION_CLEANUP_H
#define SHROOM_SERVER_SESSION_CLEANUP_H

#include "shared/sim.h"

size_t ShroomServerCleanupPlayer(ShroomWorldState* world, ShroomPlayerId player_id,
                                 ShroomPlayerState** primary, uint32_t* focused_entity_id);

#endif
