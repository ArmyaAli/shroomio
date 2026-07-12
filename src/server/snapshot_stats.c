#include "snapshot_stats.h"

#include "shared/sim.h"

void ShroomServerPopulateSnapshotRoundStats(const ShroomWorldState* world, ShroomPlayerId player_id,
                                            ShroomSnapshotPlayerState* snapshot_player) {
  const ShroomRoundStats* stats;

  if (snapshot_player == NULL) {
    return;
  }
  snapshot_player->round_spores = 0u;
  snapshot_player->round_kills = 0u;
  stats = ShroomWorldGetRoundStats(world, player_id);
  if (stats == NULL) {
    return;
  }
  snapshot_player->round_spores = (uint16_t)stats->spores_collected;
  snapshot_player->round_kills = (uint8_t)stats->kills;
}
