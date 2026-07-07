#ifndef SHROOM_CURSOR_H
#define SHROOM_CURSOR_H

#include "raylib.h"

typedef enum ShroomCursorMode {
  SHROOM_CURSOR_DEFAULT = 0,
  SHROOM_CURSOR_GAMEPLAY = 1,
  SHROOM_CURSOR_CONSUME = 2,
  SHROOM_CURSOR_SPLIT = 3,
  SHROOM_CURSOR_DISABLED = 4,
} ShroomCursorMode;

void ShroomCursorHideSystem(void);
void ShroomCursorShowSystem(void);
void ShroomCursorDraw(ShroomCursorMode mode);

#endif
