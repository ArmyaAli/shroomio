#ifndef SHROOM_CLIENT_CURSOR_H
#define SHROOM_CLIENT_CURSOR_H

typedef enum ShroomCursorMode {
  SHROOM_CURSOR_DEFAULT = 0,
  SHROOM_CURSOR_GAMEPLAY,
  SHROOM_CURSOR_CONSUME,
  SHROOM_CURSOR_DISABLED,
} ShroomCursorMode;

void ShroomCursorInit(void);
void ShroomCursorShutdown(void);
void ShroomCursorBeginFrame(void);
void ShroomCursorSetMode(ShroomCursorMode mode);
void ShroomCursorDraw(void);

#endif
