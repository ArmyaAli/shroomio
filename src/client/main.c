#include "audio.h"
#include "account_flow.h"
#include "client_rest_curl.h"
#include "game.h"
#include "imgui_wrapper.h"
#include "layout.h"
#include "screen.h"
#include "shared/lifecycle.h"
#include "voice.h"
#include "voice_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "raylib.h"

static ShroomLifecycle g_lifecycle;
static ShroomScreenManager g_screen_manager;
static Game g_game;
static ShroomClientRest g_client_rest;
static ShroomClientRestCurl g_client_rest_curl;
static ShroomAccountFlow g_account_flow;
static bool g_client_rest_ready;
static bool g_client_rest_curl_ready;

#ifndef _WIN32
static void HandleCrashSignal(int signal_number) {
  void* frames[64];
  const int frame_count = backtrace(frames, 64);

  fprintf(stderr, "\nshroomio caught signal %d on screen %s\n", signal_number,
          ShroomScreenManagerGetCurrentScreenName(&g_screen_manager));
  backtrace_symbols_fd(frames, frame_count, STDERR_FILENO);
  _Exit(128 + signal_number);
}
#endif

int main(void) {
  const int screen_width = 1280;
  const int screen_height = 720;

#ifndef _WIN32
  signal(SIGSEGV, HandleCrashSignal);
  signal(SIGABRT, HandleCrashSignal);
#endif

  ShroomLifecycleInit(&g_lifecycle);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_INIT);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(screen_width, screen_height, "shroomio");
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);

  ShroomImGui_Init();
  ClientSettingsLoad(&g_game.settings);
  {
    const char* base_url = getenv("SHROOM_REST_BASE_URL");
    const char* ca_certificate = getenv("SHROOM_REST_CA_CERT");
    const char* pinned_public_key = getenv("SHROOM_REST_PINNED_PUBLIC_KEY");
    const char* development_mode = getenv("SHROOM_REST_DEV_MODE");
    ShroomClientRestConfig rest_config;

    if ((base_url == NULL) || (base_url[0] == '\0')) {
      base_url = "https://127.0.0.1:7443";
    }
    g_client_rest_curl_ready = ShroomClientRestCurlGlobalInit();
    if (g_client_rest_curl_ready &&
        ShroomClientRestCurlInit(&g_client_rest_curl, ca_certificate, pinned_public_key)) {
      rest_config = (ShroomClientRestConfig){
          .base_url = base_url,
          .development_mode = (development_mode != NULL) && (strcmp(development_mode, "1") == 0),
          .transport = ShroomClientRestCurlPerform,
          .transport_context = &g_client_rest_curl,
      };
      g_client_rest_ready = ShroomClientRestInit(&g_client_rest, &rest_config);
    }
    if (g_client_rest_ready) {
      ShroomAccountFlowInit(&g_account_flow, &g_client_rest);
      g_game.account_flow = &g_account_flow;
      if (ShroomClientRestHasStoredSession(&g_client_rest)) {
        (void)ShroomAccountFlowStartRestore(&g_account_flow);
      }
    }
  }
  ShroomClientAudioInit(&g_game.settings);
  (void)ShroomVoiceConfigure(ShroomVoiceProductionBackend());
  snprintf(g_game.selected_server_host, sizeof(g_game.selected_server_host), "%s", "127.0.0.1");
  g_game.selected_server_port = 7777;
  g_game.selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
  ShroomQuickMatchInit(&g_game.quick_match);

  ShroomScreenManagerInit(&g_screen_manager);
  g_screen_manager.user_data = &g_game;
  ShroomScreenRegisterMainMenu(&g_screen_manager);
  ShroomScreenRegisterSettings(&g_screen_manager);
  ShroomScreenRegisterHelp(&g_screen_manager);
  ShroomScreenRegisterCredits(&g_screen_manager);
  ShroomScreenRegisterGameModeSelect(&g_screen_manager);
  ShroomScreenRegisterServerBrowser(&g_screen_manager);
  ShroomScreenRegisterLobbyBrowser(&g_screen_manager);
  ShroomScreenRegisterLobbyRoster(&g_screen_manager);
  ShroomScreenRegisterGame(&g_screen_manager);
  ShroomScreenRegisterResults(&g_screen_manager);
  ShroomScreenManagerTransition(&g_screen_manager, SHROOM_SCREEN_MAIN_MENU);

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_START);

  while (!WindowShouldClose() && !ShroomLifecycleIsShutdownRequested(&g_lifecycle) &&
         ShroomScreenManagerIsRunning(&g_screen_manager)) {
    if (IsWindowResized()) {
      GameHandleResize(&g_game, GetScreenWidth(), GetScreenHeight());
    }

    ShroomScreenManagerHandleInput(&g_screen_manager);
    ShroomScreenManagerUpdate(&g_screen_manager, GetFrameTime());
    ShroomClientAudioUpdateMusic(&g_game.settings);

    ShroomImGui_ApplyTheme(g_game.settings.palette_preset == CLIENT_PALETTE_HIGH_CONTRAST);
    ShroomLayoutSetScale((float)g_game.settings.ui_scale_percent / 100.0f);
    ShroomImGui_SetUiScale(ShroomLayoutGetScale());
    ShroomImGui_NewFrame();

    BeginDrawing();
    ShroomScreenManagerDraw(&g_screen_manager);
    ShroomImGui_Render();
    EndDrawing();
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_STOP);
  ShroomScreenManagerShutdown(&g_screen_manager);
  if (g_client_rest_ready) {
    ShroomAccountFlowShutdown(&g_account_flow);
    ShroomClientRestClear(&g_client_rest);
  }
  if (g_client_rest_curl_ready) {
    ShroomClientRestCurlGlobalCleanup();
  }
  ShroomVoiceShutdown();
  ShroomClientAudioShutdown();
  ShroomImGui_Shutdown();
  CloseWindow();
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN);
  return 0;
}
