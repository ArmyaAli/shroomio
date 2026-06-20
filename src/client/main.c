#include "game.h"
#include "imgui_wrapper.h"
#include "screen.h"
#include "shared/lifecycle.h"

#include <stdio.h>

#include "raylib.h"

static ShroomLifecycle g_lifecycle;
static ShroomScreenManager g_screen_manager;
static Game g_game;

int main(void) {
  const int screen_width = 1280;
  const int screen_height = 720;

  ShroomLifecycleInit(&g_lifecycle);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_INIT);

  InitWindow(screen_width, screen_height, "shroomio");
  InitAudioDevice();
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);

  ShroomImGui_Init();
  ClientSettingsLoad(&g_game.settings);
  SetMasterVolume((float)g_game.settings.master_volume_percent / 100.0f);
  snprintf(g_game.selected_server_host, sizeof(g_game.selected_server_host), "%s", "127.0.0.1");
  g_game.selected_server_port = 7777;
  g_game.selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;

  ShroomScreenManagerInit(&g_screen_manager);
  g_screen_manager.user_data = &g_game;
  ShroomScreenRegisterMainMenu(&g_screen_manager);
  ShroomScreenRegisterSettings(&g_screen_manager);
  ShroomScreenRegisterHelp(&g_screen_manager);
  ShroomScreenRegisterCredits(&g_screen_manager);
  ShroomScreenRegisterServerBrowser(&g_screen_manager);
  ShroomScreenRegisterLobbyBrowser(&g_screen_manager);
  ShroomScreenRegisterGame(&g_screen_manager);
  ShroomScreenRegisterResults(&g_screen_manager);
  ShroomScreenManagerTransition(&g_screen_manager, SHROOM_SCREEN_MAIN_MENU);

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_START);

  while (!WindowShouldClose() && !ShroomLifecycleIsShutdownRequested(&g_lifecycle) &&
         ShroomScreenManagerIsRunning(&g_screen_manager)) {
    ShroomScreenManagerHandleInput(&g_screen_manager);
    ShroomScreenManagerUpdate(&g_screen_manager, GetFrameTime());

    ShroomImGui_ApplyTheme(g_game.settings.palette_preset == CLIENT_PALETTE_HIGH_CONTRAST);
    ShroomImGui_SetUiScale((float)g_game.settings.ui_scale_percent / 100.0f);
    ShroomImGui_NewFrame();

    BeginDrawing();
    ShroomScreenManagerDraw(&g_screen_manager);
    ShroomImGui_Render();
    EndDrawing();
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_STOP);
  ShroomScreenManagerShutdown(&g_screen_manager);
  ShroomImGui_Shutdown();
  if (IsAudioDeviceReady()) {
    CloseAudioDevice();
  }
  CloseWindow();
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN);
  return 0;
}
