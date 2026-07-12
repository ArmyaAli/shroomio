#include "session_cleanup.h"

size_t ShroomServerCleanupPlayer(ShroomWorldState* world, ShroomPlayerId player_id,
                                 ShroomPlayerState** primary, uint32_t* focused_entity_id) {
  size_t removed = ShroomWorldRemovePlayer(world, player_id);

  if (primary != NULL) {
    *primary = NULL;
  }
  if (focused_entity_id != NULL) {
    *focused_entity_id = 0u;
  }
  return removed;
}
