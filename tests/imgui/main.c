#include "app.h"
#include "imgui_te_wrapper.h"

#include "client/client_settings.h"
#include "client/audio.h"
#include "client/imgui_wrapper.h"
#include "client/layout.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ShroomImGuiTestApp g_imgui_test_app;

extern void ShroomRegisterImGuiTests(ImGuiTestEngine* engine);

static void RegisterScreens(ShroomScreenManager* manager) {
  ShroomScreenRegisterMainMenu(manager);
  ShroomScreenRegisterSettings(manager);
  ShroomScreenRegisterHelp(manager);
  ShroomScreenRegisterCredits(manager);
  ShroomScreenRegisterGameModeSelect(manager);
  ShroomScreenRegisterServerBrowser(manager);
  ShroomScreenRegisterLobbyBrowser(manager);
  ShroomScreenRegisterLobbyRoster(manager);
  ShroomScreenRegisterGame(manager);
  ShroomScreenRegisterResults(manager);
}

static void ResetPersistentFiles(void) {
  unlink("client_settings.cfg");
  unlink("server_browser_recent.txt");
  unlink(SHROOM_CHAT_CACHE_DEFAULT_PATH);
  unlink(SHROOM_CHAT_CACHE_DEFAULT_PATH ".tmp");
  unlink("imgui.ini");
}

void ShroomImGuiTestAppReset(bool reset_files) {
  if (g_imgui_test_app.screen_manager_initialized) {
    ShroomScreenManagerShutdown(&g_imgui_test_app.screen_manager);
    g_imgui_test_app.screen_manager_initialized = false;
  }

  if (reset_files) {
    ResetPersistentFiles();
  }

  memset(&g_imgui_test_app.game, 0, sizeof(g_imgui_test_app.game));
  ClientSettingsSetDefaults(&g_imgui_test_app.game.settings);
  snprintf(g_imgui_test_app.game.settings.player_name,
           sizeof(g_imgui_test_app.game.settings.player_name), "%s", "Test Player");
  snprintf(g_imgui_test_app.game.selected_server_host,
           sizeof(g_imgui_test_app.game.selected_server_host), "%s", "127.0.0.1");
  g_imgui_test_app.game.selected_server_port = SHROOM_SERVER_PORT;
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;

  ShroomScreenManagerInit(&g_imgui_test_app.screen_manager);
  g_imgui_test_app.screen_manager.user_data = &g_imgui_test_app.game;
  RegisterScreens(&g_imgui_test_app.screen_manager);
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_MAIN_MENU);
  g_imgui_test_app.screen_manager_initialized = true;
}

static bool SetupWorkingDirectory(void) {
  char template_path[] = "/tmp/shroomio-imgui-tests-XXXXXX";
  char* temp_dir = mkdtemp(template_path);

  if (temp_dir == NULL) {
    perror("mkdtemp");
    return false;
  }

  snprintf(g_imgui_test_app.temp_dir, sizeof(g_imgui_test_app.temp_dir), "%s", temp_dir);
  if (chdir(g_imgui_test_app.temp_dir) != 0) {
    perror("chdir");
    return false;
  }

  return true;
}

int main(void) {
  ImGuiTestEngine* engine;
  int tested = 0;
  int success = 0;
  int queued = 0;
  int exit_code = 0;

  if (!SetupWorkingDirectory()) {
    return 1;
  }

  InitWindow(1280, 720, "shroomio imgui tests");
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);

  ShroomImGui_Init();
  ShroomImGuiTestAppReset(true);
  ShroomClientAudioInit(&g_imgui_test_app.game.settings);

  engine = ShroomTeEngine_Create();
  ShroomRegisterImGuiTests(engine);
  ShroomTeEngine_QueueAll(engine);

  while (!WindowShouldClose()) {
    const float frame_delta = g_imgui_test_app.frame_delta_override > 0.0f
                                  ? g_imgui_test_app.frame_delta_override
                                  : GetFrameTime();
    ShroomScreenManagerHandleInput(&g_imgui_test_app.screen_manager);
    ShroomScreenManagerUpdate(&g_imgui_test_app.screen_manager, frame_delta);
    ShroomClientAudioUpdateMusic(&g_imgui_test_app.game.settings);

    ShroomImGui_ApplyTheme(g_imgui_test_app.game.settings.palette_preset ==
                           CLIENT_PALETTE_HIGH_CONTRAST);
    ShroomLayoutSetScale((float)g_imgui_test_app.game.settings.ui_scale_percent / 100.0f);
    ShroomImGui_SetUiScale(ShroomLayoutGetScale());
    ShroomImGui_NewFrame();

    BeginDrawing();
    ShroomScreenManagerDraw(&g_imgui_test_app.screen_manager);
    ShroomImGui_Render();
    ShroomTeEngine_PreSwap(engine);
    EndDrawing();
    ShroomTeEngine_PostSwap(engine);

    if (ShroomTeEngine_IsDone(engine)) {
      break;
    }
  }

  ShroomTeEngine_Stop(engine);
  ShroomTeEngine_GetResults(engine, &tested, &success, &queued);
  fprintf(stderr, "ImGui tests: tested=%d success=%d queued=%d\n", tested, success, queued);
  if ((tested == 0) || (tested != success) || (queued != 0)) {
    exit_code = 1;
  }

  ShroomScreenManagerShutdown(&g_imgui_test_app.screen_manager);
  ShroomClientAudioShutdown();
  ShroomImGui_Shutdown();
  CloseWindow();
  ShroomTeEngine_Destroy(engine);
  return exit_code;
}
