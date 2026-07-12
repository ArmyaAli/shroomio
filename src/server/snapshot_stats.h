#ifndef SHROOM_SERVER_SNAPSHOT_STATS_H
#define SHROOM_SERVER_SNAPSHOT_STATS_H

#include "shared/protocol.h"
#include "shared/world.h"

void ShroomServerPopulateSnapshotRoundStats(const ShroomWorldState* world, ShroomPlayerId player_id,
                                            ShroomSnapshotPlayerState* snapshot_player);

#endif
