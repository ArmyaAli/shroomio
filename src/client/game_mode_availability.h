#ifndef SHROOM_GAME_MODE_AVAILABILITY_H
#define SHROOM_GAME_MODE_AVAILABILITY_H

#include "shared/config.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct ShroomGameModeCapability {
  ShroomGameMode mode;
  const char* label;
  const char* summary;
  bool available;
} ShroomGameModeCapability;

const ShroomGameModeCapability* ShroomGameModeCapabilities(size_t* count);
bool ShroomGameModeIsAvailable(ShroomGameMode mode);

#endif
