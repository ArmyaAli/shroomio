#ifndef SHROOM_LOBBY_CAPACITY_H
#define SHROOM_LOBBY_CAPACITY_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ShroomLobbyAdmissionPlan {
  bool accepted;
  uint16_t bots_to_remove;
} ShroomLobbyAdmissionPlan;

ShroomLobbyAdmissionPlan ShroomLobbyPlanAdmission(uint16_t real_player_count, uint16_t bot_count);

#endif
