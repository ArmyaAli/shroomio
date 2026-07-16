#include "app.h"
#include "imgui_te_wrapper.h"

#include "client/client_settings.h"
#include "client/client_storage.h"
#include "client/audio.h"
#include "client/chat_cache.h"
#include "client/client_paths.h"
#include "client/imgui_wrapper.h"
#include "client/layout.h"
#include "client/voice.h"
#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ShroomImGuiTestApp g_imgui_test_app;

typedef struct ShroomImGuiAccountBackend {
  int login_calls;
  int register_calls;
  int logout_calls;
} ShroomImGuiAccountBackend;

static ShroomImGuiAccountBackend g_account_backend;

static void AccountRespond(ShroomClientHttpResponse* response, long status, const char* body) {
  const size_t length = strlen(body);
  response->status = status;
  if (length >= response->body_capacity) {
    snprintf(response->transport_error, sizeof(response->transport_error), "%s",
             "Stub response overflow");
    return;
  }
  memcpy(response->body, body, length + 1u);
  response->body_length = length;
}

static bool AccountPerform(void* context, const ShroomClientHttpRequest* request,
                           ShroomClientHttpResponse* response) {
  ShroomImGuiAccountBackend* backend = context;

  if (strstr(request->url, "/register") != NULL) {
    ++backend->register_calls;
    AccountRespond(response, 201,
                   "{\"account\":{},\"session\":{\"access_token\":\"access\","
                   "\"expires_in\":900,\"refresh_token\":\"refresh\","
                   "\"refresh_expires_in\":3600}}");
  } else if (strstr(request->url, "/login") != NULL) {
    ++backend->login_calls;
    AccountRespond(response, 200,
                   "{\"access_token\":\"access\",\"expires_in\":900,"
                   "\"refresh_token\":\"refresh\",\"refresh_expires_in\":3600}");
  } else if (strstr(request->url, "/me") != NULL) {
    AccountRespond(response, 200,
                   "{\"player_id\":\"player-test\",\"username\":\"Account Player\","
                   "\"email\":\"account@example.test\","
                   "\"created_at\":\"2026-07-14T00:00:00Z\"}");
  } else if (strstr(request->url, "/logout") != NULL) {
    ++backend->logout_calls;
    AccountRespond(response, 204, "");
  } else {
    AccountRespond(response, 404, "{\"error\":{\"code\":\"not_found\",\"message\":\"Missing\"}}");
  }
  return true;
}

int ShroomImGuiTestAccountLoginCalls(void) { return g_account_backend.login_calls; }
int ShroomImGuiTestAccountRegisterCalls(void) { return g_account_backend.register_calls; }
int ShroomImGuiTestAccountLogoutCalls(void) { return g_account_backend.logout_calls; }

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
  char path[SHROOM_CLIENT_PATH_MAX];
  char temporary_path[SHROOM_CLIENT_PATH_MAX + sizeof(".tmp")];

  unlink("client_settings.cfg");
  unlink("client_settings.cfg.tmp");
  unlink("client_settings.cfg.bak");
  unlink("client_settings.cfg.bak.tmp");
  unlink("server_browser_recent.txt");
  unlink(SHROOM_CHAT_CACHE_LEGACY_PATH);
  if (ShroomClientPathsGetCacheFile(path, sizeof(path), "server_browser_recent.txt")) {
    unlink(path);
  }
  if (ShroomClientPathsGetCacheFile(path, sizeof(path), SHROOM_CHAT_CACHE_FILENAME)) {
    unlink(path);
    if (snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path) > 0) {
      unlink(temporary_path);
    }
  }
  unlink("imgui.ini");
  unlink("account_session.cfg");
  unlink("account_session.cfg.tmp");
}

void ShroomImGuiTestAppReset(bool reset_files) {
  if (g_imgui_test_app.screen_manager_initialized) {
    ShroomScreenManagerShutdown(&g_imgui_test_app.screen_manager);
    g_imgui_test_app.screen_manager_initialized = false;
  }
  if (g_imgui_test_app.account_flow_initialized) {
    ShroomAccountFlowShutdown(&g_imgui_test_app.account_flow);
    g_imgui_test_app.account_flow_initialized = false;
  }

  if (reset_files) {
    ResetPersistentFiles();
  }

  memset(&g_imgui_test_app.game, 0, sizeof(g_imgui_test_app.game));
  memset(&g_account_backend, 0, sizeof(g_account_backend));
  ClientSettingsSetDefaults(&g_imgui_test_app.game.settings);
  snprintf(g_imgui_test_app.game.settings.player_name,
           sizeof(g_imgui_test_app.game.settings.player_name), "%s", "Test Player");
  snprintf(g_imgui_test_app.game.selected_server_host,
           sizeof(g_imgui_test_app.game.selected_server_host), "%s", "127.0.0.1");
  g_imgui_test_app.game.selected_server_port = SHROOM_SERVER_PORT;
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
  ShroomQuickMatchInit(&g_imgui_test_app.game.quick_match);
  {
    const ShroomClientRestConfig config = {.base_url = "https://account.example.test",
                                           .session_path = "account_session.cfg",
                                           .transport = AccountPerform,
                                           .transport_context = &g_account_backend};
    if (!ShroomClientRestInit(&g_imgui_test_app.account_rest, &config)) {
      fprintf(stderr, "failed to initialize account REST test backend\n");
      abort();
    }
  }
  ShroomAccountFlowInit(&g_imgui_test_app.account_flow, &g_imgui_test_app.account_rest);
  g_imgui_test_app.account_flow_initialized = true;
  g_imgui_test_app.game.account_flow = &g_imgui_test_app.account_flow;

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
  {
    char cache_root[SHROOM_CLIENT_PATH_MAX];
    const int length =
        snprintf(cache_root, sizeof(cache_root), "%s/platform-cache", g_imgui_test_app.temp_dir);
    if ((length < 0) || ((size_t)length >= sizeof(cache_root))) {
      return 1;
    }
    ShroomClientPathsSetTestCacheRoot(cache_root);
    ShroomClientStorageSetTestConfigRoot(g_imgui_test_app.temp_dir);
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
  ShroomAccountFlowShutdown(&g_imgui_test_app.account_flow);
  ShroomVoiceShutdown();
  ShroomClientAudioShutdown();
  ShroomImGui_Shutdown();
  CloseWindow();
  ShroomTeEngine_Destroy(engine);
  return exit_code;
}
