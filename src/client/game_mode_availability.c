#include "game_mode_availability.h"

static const ShroomGameModeCapability GAME_MODE_CAPABILITIES[] = {
    {SHROOM_GAME_MODE_FFA, "Free-for-All (FFA)",
     "Grow, split, and outlast every other colony in the arena.", true},
    {SHROOM_GAME_MODE_TEAMS_2V2, "Teams 2v2", "Coordinate with one ally against another pair.",
     false},
    {SHROOM_GAME_MODE_TEAMS_3V3, "Teams 3v3", "Three-player squads compete for total control.",
     false},
    {SHROOM_GAME_MODE_TEAMS_4V4, "Teams 4v4", "Large squads share pressure across the arena.",
     false},
    {SHROOM_GAME_MODE_BATTLE_ROYALE, "Battle Royale",
     "Survive a shrinking arena until one colony remains.", false},
    {SHROOM_GAME_MODE_KING_OF_HILL, "King of the Hill",
     "Hold the center zone to build a winning score.", true},
    {SHROOM_GAME_MODE_MASS_RACE, "Mass Race", "Be the first colony to reach the mass target.",
     false},
};

const ShroomGameModeCapability* ShroomGameModeCapabilities(size_t* count) {
  if (count != NULL) {
    *count = sizeof(GAME_MODE_CAPABILITIES) / sizeof(GAME_MODE_CAPABILITIES[0]);
  }
  return GAME_MODE_CAPABILITIES;
}

bool ShroomGameModeIsAvailable(ShroomGameMode mode) {
  size_t count;
  const ShroomGameModeCapability* capabilities = ShroomGameModeCapabilities(&count);

  for (size_t index = 0; index < count; ++index) {
    if (capabilities[index].mode == mode) {
      return capabilities[index].available;
    }
  }
  return false;
}
