#include "game.h"
#include "screen.h"
#include "shared/lifecycle.h"
#include "raylib.h"

static ShroomLifecycle g_lifecycle;
static ShroomScreenManager g_screen_manager;

int main(void) {
  const int screen_width = 1280;
  const int screen_height = 720;

  ShroomLifecycleInit(&g_lifecycle);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_INIT);

  InitWindow(screen_width, screen_height, "shroomio");
  SetTargetFPS(60);

  ShroomScreenManagerInit(&g_screen_manager);
  ShroomScreenRegisterMainMenu(&g_screen_manager);
  ShroomScreenManagerTransition(&g_screen_manager, SHROOM_SCREEN_MAIN_MENU);

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_START);

  while (!WindowShouldClose() && !ShroomLifecycleIsShutdownRequested(&g_lifecycle) &&
         ShroomScreenManagerIsRunning(&g_screen_manager)) {
    ShroomScreenManagerHandleInput(&g_screen_manager);
    ShroomScreenManagerUpdate(&g_screen_manager, GetFrameTime());
    ShroomScreenManagerDraw(&g_screen_manager);
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_STOP);
  ShroomScreenManagerShutdown(&g_screen_manager);
  CloseWindow();
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN);
  return 0;
}
