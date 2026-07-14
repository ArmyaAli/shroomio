#include "lobby_capacity.h"

#include "shared/config.h"

ShroomLobbyAdmissionPlan ShroomLobbyPlanAdmission(uint16_t real_player_count, uint16_t bot_count) {
  ShroomLobbyAdmissionPlan plan = {0};
  uint32_t occupied;

  if (real_player_count >= SHROOM_MAX_PLAYABLE_PARTICIPANTS) {
    return plan;
  }
  occupied = (uint32_t)real_player_count + bot_count;
  plan.bots_to_remove =
      occupied < SHROOM_MAX_PARTICIPANTS ? 0u : (uint16_t)(occupied - SHROOM_MAX_PARTICIPANTS + 1u);
  plan.accepted = plan.bots_to_remove <= bot_count;
  return plan;
}
