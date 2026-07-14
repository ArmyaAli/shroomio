#include "spectator_target.h"

bool ShroomSpectatorTargetIsEligible(const ShroomPlayerState* player,
                                     ShroomPlayerId excluded_player_id) {
  return (player != NULL) && player->alive && (player->player_id != 0u) &&
         (player->entity_id != 0u) && (player->piece_index == 0u) &&
         ((excluded_player_id == 0u) || (player->player_id != excluded_player_id));
}

ShroomEntityId ShroomSpectatorSelectNextTarget(const ShroomPlayerState* players,
                                               size_t player_count,
                                               ShroomEntityId current_entity_id,
                                               ShroomPlayerId excluded_player_id,
                                               ShroomEntityId excluded_entity_id, int direction) {
  size_t start_index;
  size_t current_index = player_count;

  if ((players == NULL) || (player_count == 0u)) {
    return 0u;
  }

  for (size_t index = 0u; index < player_count; ++index) {
    if (players[index].entity_id == current_entity_id) {
      current_index = index;
      break;
    }
  }

  start_index =
      current_index < player_count ? current_index : (direction < 0 ? 0u : player_count - 1u);
  for (size_t offset = 1u; offset <= player_count; ++offset) {
    const size_t index = direction < 0 ? (start_index + player_count - offset) % player_count
                                       : (start_index + offset) % player_count;
    const ShroomPlayerState* player = &players[index];

    if (player->entity_id == excluded_entity_id) {
      continue;
    }
    if (ShroomSpectatorTargetIsEligible(player, excluded_player_id)) {
      return player->entity_id;
    }
  }

  return 0u;
}

bool ShroomSpectatorTargetLifeEnded(const ShroomPlayerState* previous,
                                    const ShroomPlayerState* current) {
  if ((previous == NULL) || !previous->alive || (previous->entity_id == 0u)) {
    return false;
  }
  if ((current == NULL) || !current->alive || (current->player_id != previous->player_id) ||
      (current->entity_id != previous->entity_id)) {
    return true;
  }

  return current->life_generation != previous->life_generation;
}
