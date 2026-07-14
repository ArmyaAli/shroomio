#ifndef SHROOM_CLIENT_SPECTATOR_TARGET_H
#define SHROOM_CLIENT_SPECTATOR_TARGET_H

#include "shared/world.h"

#include <stddef.h>

bool ShroomSpectatorTargetIsEligible(const ShroomPlayerState* player,
                                     ShroomPlayerId excluded_player_id);
ShroomEntityId ShroomSpectatorSelectNextTarget(const ShroomPlayerState* players,
                                               size_t player_count,
                                               ShroomEntityId current_entity_id,
                                               ShroomPlayerId excluded_player_id,
                                               ShroomEntityId excluded_entity_id, int direction);
bool ShroomSpectatorTargetLifeEnded(const ShroomPlayerState* previous,
                                    const ShroomPlayerState* current);

#endif
