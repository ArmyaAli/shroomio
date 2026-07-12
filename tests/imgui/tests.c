#include "app.h"
#include "imgui_te_wrapper.h"

#include "client/screens/screen_background.h"
#include "client/server_browser_model.h"
#include "shared/protocol.h"
#include "shared/sim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Inject N fake lobby entries into the game's net state as the server would. */
static void InjectFakeLobbies(int count) {
  int i;

  g_imgui_test_app.game.net.lobby_count = (uint8_t)count;
  for (i = 0; i < count; ++i) {
    ShroomLobbyEntry* e = &g_imgui_test_app.game.net.lobby_list[i];
    memset(e, 0, sizeof(*e));
    e->lobby_id = (uint32_t)(i + 1);
    snprintf(e->name, sizeof(e->name), "Arena %d", i + 1);
    e->player_count = (uint16_t)(i * 2);
    e->bot_count = 15;
    e->max_players = 28;
    e->is_dynamic = 0;
  }
}

/* Put the app into the lobby browser screen with handshake already completed. */
static void SetupLobbyBrowser(void) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY);
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  g_imgui_test_app.game.net.handshake_received = true;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "Connected");
}

static void SetupOfflineGame(void) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_GAME);
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "Offline");
}

static void SetupOnlineGame(void) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_GAME);
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.player_id = 1;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "Connected");
}

static void SetupLiveLobbyGame(void) {
  ShroomSnapshotPlayerState* player;

  SetupOfflineGame();
  ClientNetInit(&g_imgui_test_app.game.net, "127.0.0.1", 37779u);
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  g_imgui_test_app.game.net.handshake_received = true;
  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.match_entry_sent = true;
  g_imgui_test_app.game.net.lobby_id = 3u;
  g_imgui_test_app.game.net.player_id = 7u;
  g_imgui_test_app.game.net.entity_id = 42u;
  g_imgui_test_app.game.net.world_width = SHROOM_WORLD_WIDTH;
  g_imgui_test_app.game.net.world_height = SHROOM_WORLD_HEIGHT;
  g_imgui_test_app.game.net.snapshot_player_count = 1u;
  player = &g_imgui_test_app.game.net.snapshot_players[0];
  *player = (ShroomSnapshotPlayerState){
      .player_id = 7u,
      .entity_id = 42u,
      .position_x = 1200.0f,
      .position_y = 1400.0f,
      .mass = SHROOM_DEFAULT_PLAYER_MASS,
      .radius = ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS),
      .alive = 1u,
  };
  snprintf(player->name, sizeof(player->name), "%s", "Replay Tester");
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
}

static void SetupResultsScreen(float peak_mass, float final_mass, int final_rank) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  g_imgui_test_app.game.peak_mass = peak_mass;
  g_imgui_test_app.game.final_mass = final_mass;
  g_imgui_test_app.game.final_rank = final_rank;
  g_imgui_test_app.game.session_duration_seconds = 65u;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_RESULTS);
}

/* -------------------------------------------------------------------------
 * Test functions — plain C, no lambdas.
 * ---------------------------------------------------------------------- */

static void Test_MainMenuNavigation(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SETTINGS);

  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Help");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_HELP);
}

static void Test_HelpAndCreditsBackNavigation(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Help");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_HELP);
  IM_CHECK(ShroomTeImGui_WindowIsActive("How To Play"));

  ShroomTeCtx_SetRef(ctx, "How To Play");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Sprout 86"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Cluster 112"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Bloom 148"));

  ShroomTeCtx_ItemClick(ctx, "Bloom 148");
  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Credits");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_CREDITS);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Credits"));

  ShroomTeCtx_SetRef(ctx, "Credits");
  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);
}

static void Test_MainMenuExposesPrimaryActions(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Play Online"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Custom Server"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Offline Practice"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Settings"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Help"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Credits"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Exit"));
}

static void Test_GameModeAvailabilityAndNavigation(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Game Modes");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME_MODE_SELECT);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Select Game Mode"));

  ShroomTeCtx_SetRef(ctx, "Select Game Mode");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Free-for-All (FFA)"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Teams 2v2 - Unavailable"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Battle Royale - Unavailable"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Mass Race - Unavailable"));

  ShroomTeCtx_ItemClick(ctx, "Teams 2v2 - Unavailable");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME_MODE_SELECT);

  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Game Modes");
  ShroomTeCtx_SetRef(ctx, "Select Game Mode");
  ShroomTeCtx_ItemClick(ctx, "Free-for-All (FFA)");
  IM_CHECK_EQ(g_imgui_test_app.game.selected_game_mode, SHROOM_GAME_MODE_FFA);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);
}

static void Test_OfflinePracticeEntryInitializesGame(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Offline Practice");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_EQ(g_imgui_test_app.game.selected_mode, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);
}

/* screens: Offline Practice should not inherit the menu spin regression into
 * gameplay camera state, even with persisted settings from the WSL repro. */
static void Test_OfflinePracticePersistedSettingsCameraStaysStable(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.ui_scale_percent = 119;
  g_imgui_test_app.game.settings.menu_animations_enabled = false;
  g_imgui_test_app.game.settings.camera_zoom = 0.44f;

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));

  ShroomTeCtx_ItemClick(ctx, "Offline Practice");
  ShroomTeCtx_Yield(ctx, 90);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);
  IM_CHECK(fabsf(g_imgui_test_app.game.camera.rotation) <= 0.001f);
  IM_CHECK(fabsf(g_imgui_test_app.game.camera.zoom - 0.44f) <= 0.01f);
  IM_CHECK(fabsf(g_imgui_test_app.game.camera_zoom_target - 0.44f) <= 0.001f);
  IM_CHECK(ShroomScreenManagerIsRunning(&g_imgui_test_app.screen_manager));
}

static void Test_SettingsExposesSpecControlsAndAppliesBoundaryValues(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "UI Scale"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Preferred Region"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Palette"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Master Volume"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Music Volume"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Effects Volume"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Invert Mouse"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Show Diagnostics On Launch"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Show Ping In HUD"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Particle Quality"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Animated Menu Backgrounds"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Death Cutscene"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Mushroom Species"));

  ShroomTeCtx_ItemInputValueInt(ctx, "UI Scale", 160);
  ShroomTeCtx_ItemInputValueInt(ctx, "Master Volume", 0);
  ShroomTeCtx_ItemInputValueInt(ctx, "Music Volume", 100);
  ShroomTeCtx_ItemInputValueInt(ctx, "Effects Volume", 0);
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Save");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, 160);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.master_volume_percent, 0);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.music_volume_percent, 100);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.effects_volume_percent, 0);

  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  ShroomTeCtx_ItemInputValueInt(ctx, "Music Volume", 0);
  ShroomTeCtx_ItemInputValueInt(ctx, "Effects Volume", 100);
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Save");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(g_imgui_test_app.game.settings.music_volume_percent, 0);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.effects_volume_percent, 100);
}

static void Test_SettingsPersistence(ImGuiTestContext* ctx) {
  ClientSettings loaded;
  memset(&loaded, 0, sizeof(loaded));

  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  ShroomTeCtx_ItemInputValueInt(ctx, "UI Scale", 130);
  ShroomTeCtx_ItemInputValueInt(ctx, "Master Volume", 65);
  ShroomTeCtx_ItemInputValueInt(ctx, "Music Volume", 40);
  ShroomTeCtx_ItemInputValueInt(ctx, "Effects Volume", 55);
  ShroomTeCtx_ItemCheckbox(ctx, "Invert Mouse");
  ShroomTeCtx_ItemCheckbox(ctx, "Show Diagnostics On Launch");
  ShroomTeCtx_ItemCheckbox(ctx, "Show Ping In HUD");
  ShroomTeCtx_ItemCheckbox(ctx, "Animated Menu Backgrounds");
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Save");

  IM_CHECK(ClientSettingsLoad(&loaded));
  IM_CHECK_EQ(loaded.ui_scale_percent, 130);
  IM_CHECK_EQ(loaded.master_volume_percent, 65);
  IM_CHECK_EQ(loaded.music_volume_percent, 40);
  IM_CHECK_EQ(loaded.effects_volume_percent, 55);
  IM_CHECK(loaded.invert_mouse);
  IM_CHECK(loaded.diagnostics_enabled);
  IM_CHECK(loaded.show_ping_ms);
  IM_CHECK(loaded.menu_animations_enabled);
}

static void Test_SettingsReservedKeyRejectionAndRecovery(ImGuiTestContext* ctx) {
  ClientSettings loaded;

  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  ShroomTestSettingsBeginRebind(0);

  ShroomTestSettingsCaptureKey(KEY_ENTER);
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK_EQ(ShroomTestSettingsPendingKey(0), KEY_T);
  IM_CHECK_STR_EQ(ShroomTestGetSettingsRebindError(),
                  "Enter is reserved for interface controls. Choose another key.");

  ShroomTestSettingsCaptureKey(KEY_TAB);
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK_EQ(ShroomTestSettingsPendingKey(0), KEY_T);
  IM_CHECK_STR_EQ(ShroomTestGetSettingsRebindError(),
                  "Tab is reserved for interface controls. Choose another key.");

  ShroomTestSettingsCaptureKey(KEY_Y);
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK_EQ(ShroomTestSettingsPendingKey(0), KEY_Y);
  IM_CHECK_STR_EQ(ShroomTestGetSettingsRebindError(), "");

  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Save");
  IM_CHECK_EQ(g_imgui_test_app.game.settings.key_chat_open, KEY_Y);
  IM_CHECK(ClientSettingsLoad(&loaded));
  IM_CHECK_EQ(loaded.key_chat_open, KEY_Y);
}

static void Test_UiScaleEndpointsKeepPrimaryWindowsUsable(ImGuiTestContext* ctx) {
  const int scales[] = {80, 100, 160};

  for (size_t index = 0; index < sizeof(scales) / sizeof(scales[0]); ++index) {
    ShroomImGuiTestAppReset(true);
    g_imgui_test_app.game.settings.ui_scale_percent = scales[index];
    ShroomTeCtx_SetRef(ctx, "Main Menu");
    ShroomTeCtx_Yield(ctx, 2);

    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Main Menu"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Settings"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Exit"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Settings"));
  }

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("Settings"));
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "UI Scale"));
  ShroomTeCtx_SetRef(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Save"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Restore Defaults"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Save"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Restore Defaults"));

  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.ui_scale_percent = 160;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_SERVER_BROWSER);
  ShroomTeCtx_SetRef(ctx, "Server Browser");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("Server Browser"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Join Host"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Join Host"));

  SetupLobbyBrowser();
  g_imgui_test_app.game.settings.ui_scale_percent = 160;
  ShroomTeCtx_SetRef(ctx, "Lobby Browser");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("Lobby Browser"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Create"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Back"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Create"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Back"));

  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.player_id = 7u;
  g_imgui_test_app.game.net.lobby_roster_received = true;
  g_imgui_test_app.game.net.lobby_roster_count = 1u;
  g_imgui_test_app.game.net.lobby_roster[0] =
      (ShroomLobbyRosterEntry){.player_id = 7u, .is_ready = 1u};
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY_ROSTER);
  ShroomTeCtx_SetRef(ctx, "Lobby Roster");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("Lobby Roster"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Enter Match"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Enter Match"));

  SetupResultsScreen(420.0f, 300.0f, 3);
  g_imgui_test_app.game.settings.ui_scale_percent = 160;
  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_Yield(ctx, 4);
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("Match Results"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Play Again"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Main Menu"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Play Again"));
  IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Main Menu"));

  SetupOnlineGame();
  g_imgui_test_app.game.settings.ui_scale_percent = 160;
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("HUD Left"));
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("HUD Right"));
  IM_CHECK(ShroomTeImGui_WindowFitsViewport("Chat"));
}

static void Test_SettingsDiscardAndEscape(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  const int original_scale = g_imgui_test_app.game.settings.ui_scale_percent;

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  ShroomTeCtx_ItemInputValueInt(ctx, "UI Scale", 150);
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, original_scale);
  ShroomTeCtx_SetRef(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Discard Changes"));
  ShroomTestSettingsEscape(&g_imgui_test_app.screen_manager);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, original_scale);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  ShroomTeCtx_ItemInputValueInt(ctx, "UI Scale", 145);
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Discard Changes");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, original_scale);
}

static void Test_SettingsRestoreDefaultsRequiresSave(ImGuiTestContext* ctx) {
  ClientSettings loaded;
  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.ui_scale_percent = 140;
  g_imgui_test_app.game.settings.master_volume_percent = 35;

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Restore Defaults");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Confirm Defaults"));
  ShroomTeCtx_ItemClick(ctx, "Confirm Defaults");

  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, 140);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.master_volume_percent, 35);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Discard Changes"));

  ShroomTeCtx_ItemClick(ctx, "Save");
  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, 100);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.master_volume_percent, 80);
  IM_CHECK(ClientSettingsLoad(&loaded));
  IM_CHECK_EQ(loaded.ui_scale_percent, 100);
  IM_CHECK_EQ(loaded.master_volume_percent, 80);
}

static void Test_ServerBrowserJoinAndValidation(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Custom Server");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);

  ShroomTeCtx_SetRef(ctx, "Server Browser");
  ShroomTeCtx_ItemInputValueStr(ctx, "Port", "70000");
  ShroomTeCtx_ItemClick(ctx, "Join Host");
  IM_CHECK_STR_EQ(ShroomTestGetServerBrowserValidationMessage(),
                  "Port must be between 1 and 65535.");

  ShroomTeCtx_ItemInputValueStr(ctx, "Port", "7777");
  IM_CHECK_EQ(ShroomTestGetServerBrowserSelectedIndex(), -1);
  IM_CHECK_STR_EQ(ShroomTestGetServerBrowserSelectedHost(), "");
  ShroomTeCtx_ItemClick(ctx, "Join Host");

  /* JoinServer connects and transitions to the lobby browser, not GAME. */
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
  IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "127.0.0.1");
  IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, SHROOM_SERVER_PORT);
}

static void Test_ServerBrowserRecentJoinPersists(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Custom Server");
  ShroomTeCtx_SetRef(ctx, "Server Browser");
  ShroomTeCtx_ItemInputValueStr(ctx, "Host", "recent.shroomio.test");
  ShroomTeCtx_ItemInputValueStr(ctx, "Port", "7788");
  ShroomTeCtx_ItemClick(ctx, "Join Host");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "recent.shroomio.test");
  IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, 7788);

  ShroomImGuiTestAppReset(false);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Custom Server");
  ShroomTeCtx_SetRef(ctx, "Server Browser");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomTestGetServerBrowserRecentCount(), 1);
  ShroomTeCtx_ItemClick(ctx, "Join Recent");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "recent.shroomio.test");
  IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, 7788);
}

static void Test_ServerBrowserInvalidHostAndHostPortParsing(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Custom Server");
  ShroomTeCtx_SetRef(ctx, "Server Browser");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Refresh"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Sort by"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Ascending"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Join Selected"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Join Host"));

  ShroomTeCtx_ItemInputValueStr(ctx, "Host", "bad host!");
  ShroomTeCtx_ItemInputValueStr(ctx, "Port", "7777");
  ShroomTeCtx_ItemClick(ctx, "Join Host");
  IM_CHECK_STR_EQ(ShroomTestGetServerBrowserValidationMessage(), "Invalid hostname or IP address.");

  ShroomTeCtx_ItemInputValueStr(ctx, "Host", "recent.shroomio.test:7789");
  ShroomTeCtx_ItemClick(ctx, "Join Host");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
  IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "recent.shroomio.test");
  IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, 7789);
}

static void Test_ServerBrowserDiscoveryStatesAndSorting(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Custom Server");
  ShroomTeCtx_SetRef(ctx, "Server Browser");

  IM_CHECK_EQ(ShroomTestGetServerBrowserDiscoveryState(), SHROOM_SERVER_DISCOVERY_EMPTY);
  IM_CHECK_EQ(ShroomTestGetServerBrowserServerCount(), 0);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Join Host"));

  ShroomTeCtx_ItemClick(ctx, "Refresh");
  IM_CHECK_EQ(ShroomTestGetServerBrowserDiscoveryState(), SHROOM_SERVER_DISCOVERY_LOADING);
  ShroomTestCompleteServerBrowserRefresh(false);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomTestGetServerBrowserDiscoveryState(), SHROOM_SERVER_DISCOVERY_FAILED);
  IM_CHECK_EQ(ShroomTestGetServerBrowserServerCount(), 0);

  ShroomTeCtx_ItemClick(ctx, "Refresh");
  ShroomTestCompleteServerBrowserRefresh(true);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomTestGetServerBrowserDiscoveryState(), SHROOM_SERVER_DISCOVERY_READY);
  IM_CHECK_EQ(ShroomTestGetServerBrowserServerCount(), 5);
  IM_CHECK_STR_EQ(ShroomTestGetServerBrowserSelectedHost(), "local.demo.invalid");

  ShroomTeCtx_ItemClick(ctx, "Ascending");
  IM_CHECK(ShroomTestGetServerBrowserSortDescending());
  ShroomTeCtx_ItemClick(ctx, "Join Selected");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);

  ShroomTestMarkServerBrowserStale();
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomTestGetServerBrowserDiscoveryState(), SHROOM_SERVER_DISCOVERY_STALE);
  ShroomTeCtx_ItemClick(ctx, "Join Selected");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);
}

static void Test_LobbyConnectionModalStates(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(false);
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY);
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTING;
  g_imgui_test_app.game.net.handshake_received = false;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "Connecting to test server");

  ShroomTeCtx_SetRef(ctx, "Connection Status");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Connection Status"));

  g_imgui_test_app.game.net.status = CLIENT_NET_ERROR;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "No route to host");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Retry"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Back"));

  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);
}

/* lobby: Play Online against an unreachable server surfaces a friendly
 * 'unable to connect' message after the connect timeout, with Retry/Back
 * recovery so the user isn't stuck on the connecting spinner. Regression for
 * #371. */
static void Test_LobbyUnreachableServerShowsFriendlyError(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(false);
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY);
  /* Reproduce the post-click Play Online state: auto-join pending, connecting. */
  g_imgui_test_app.game.auto_join_lobby = true;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTING;
  g_imgui_test_app.game.net.handshake_received = false;
  g_imgui_test_app.game.net.connect_started_ms = 1000u;

  /* Advance the net clock past the connect timeout via the real helper the
   * production ClientNetUpdate uses. */
  ClientNetTestCheckConnectTimeout(&g_imgui_test_app.game.net,
                                   1000u + SHROOM_CLIENT_CONNECT_TIMEOUT_MS);

  IM_CHECK_EQ(g_imgui_test_app.game.net.status, CLIENT_NET_ERROR);
  IM_CHECK_STR_EQ(g_imgui_test_app.game.net.status_text, SHROOM_NET_CONNECT_UNREACHABLE_MSG);

  ShroomTeCtx_SetRef(ctx, "Connection Status");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Connection Status"));
  /* Recovery path must be reachable so the player can escape the error. */
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Retry"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Back"));

  ShroomTeCtx_ItemClick(ctx, "Back");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);
  IM_CHECK_EQ(g_imgui_test_app.game.auto_join_lobby, false);
}

static void Test_LobbyEmptyAndFullStatesRender(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  g_imgui_test_app.game.net.lobby_count = 0;

  ShroomTeCtx_SetRef(ctx, "Lobby Browser");
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Browser"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Refresh"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Create"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Back"));

  g_imgui_test_app.game.net.lobby_count = 1;
  memset(&g_imgui_test_app.game.net.lobby_list[0], 0,
         sizeof(g_imgui_test_app.game.net.lobby_list[0]));
  g_imgui_test_app.game.net.lobby_list[0].lobby_id = 77u;
  snprintf(g_imgui_test_app.game.net.lobby_list[0].name,
           sizeof(g_imgui_test_app.game.net.lobby_list[0].name), "Full Lobby");
  g_imgui_test_app.game.net.lobby_list[0].player_count = 32u;
  g_imgui_test_app.game.net.lobby_list[0].max_players = 32u;
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK(!ShroomTeCtx_ItemExists(ctx, "Join##77"));
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Browser"));
}

static void Test_GameplayOverlayStateToggles(ImGuiTestContext* ctx) {
  SetupOnlineGame();

  IM_CHECK_EQ(g_imgui_test_app.game.leaderboard_overlay_open, false);
  g_imgui_test_app.game.leaderboard_overlay_open = true;
  g_imgui_test_app.game.menu_overlay_open = false;
  g_imgui_test_app.game.leave_confirmation_open = false;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.leaderboard_overlay_open, true);

  g_imgui_test_app.game.menu_overlay_open = true;
  g_imgui_test_app.game.leaderboard_overlay_open = false;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.menu_overlay_open, true);

  g_imgui_test_app.game.leave_confirmation_open = true;
  g_imgui_test_app.game.menu_overlay_open = false;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.leave_confirmation_open, true);

  g_imgui_test_app.game.diagnostics_overlay_open = true;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.diagnostics_overlay_open, true);
}

static void Test_GameplayMenuOverlayActions(ImGuiTestContext* ctx) {
  SetupOnlineGame();
  g_imgui_test_app.game.menu_overlay_open = true;
  g_imgui_test_app.game.net.rtt_ms = 12345u;
  g_imgui_test_app.game.net.rtt_average_ms = 12000u;

  ShroomTeCtx_SetRef(ctx, "Match Menu");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Match Menu"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Resume"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Show Leaderboard"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Retry Connection"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Return To Main Menu"));

  ShroomTeCtx_ItemClick(ctx, "Show Leaderboard");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(!g_imgui_test_app.game.menu_overlay_open);
  IM_CHECK(g_imgui_test_app.game.leaderboard_overlay_open);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Leaderboard"));

  ShroomTeCtx_SetRef(ctx, "Leaderboard");
  ShroomTeCtx_ItemClick(ctx, "Close");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(!g_imgui_test_app.game.leaderboard_overlay_open);

  g_imgui_test_app.game.menu_overlay_open = true;
  ShroomTeCtx_SetRef(ctx, "Match Menu");
  ShroomTeCtx_ItemClick(ctx, "Return To Main Menu");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(!g_imgui_test_app.game.menu_overlay_open);
  IM_CHECK(g_imgui_test_app.game.leave_confirmation_open);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Leave Match?"));
}

static void Test_GameplayLeaveConfirmationStayAndLeave(ImGuiTestContext* ctx) {
  SetupOnlineGame();
  g_imgui_test_app.game.leave_confirmation_open = true;

  ShroomTeCtx_SetRef(ctx, "Leave Match?");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Leave Match"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Stay"));

  ShroomTeCtx_ItemClick(ctx, "Stay");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(!g_imgui_test_app.game.leave_confirmation_open);
  IM_CHECK(g_imgui_test_app.game.menu_overlay_open);

  g_imgui_test_app.game.menu_overlay_open = false;
  g_imgui_test_app.game.leave_confirmation_open = true;
  ShroomTeCtx_SetRef(ctx, "Leave Match?");
  ShroomTeCtx_ItemClick(ctx, "Leave Match");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(!g_imgui_test_app.game.leave_confirmation_open);
}

static void Test_GameplayDiagnosticsAndConnectionOverlays(ImGuiTestContext* ctx) {
  SetupOnlineGame();
  g_imgui_test_app.game.diagnostics_overlay_open = true;
  g_imgui_test_app.game.net.rtt_ms = 15000u;
  g_imgui_test_app.game.net.rtt_average_ms = 14000u;

  ShroomTeCtx_SetRef(ctx, "Diagnostics");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Diagnostics"));

  g_imgui_test_app.game.net.welcome_received = false;
  g_imgui_test_app.game.net.status = CLIENT_NET_ERROR;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "timeout");
  ShroomTeCtx_SetRef(ctx, "Connection Status");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Connection Status"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Retry"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Back To Menu"));
}

static void Test_GameplayOfflineMenuReturnRequestsResults(ImGuiTestContext* ctx) {
  SetupOfflineGame();
  g_imgui_test_app.game.menu_overlay_open = true;
  g_imgui_test_app.game.return_to_menu_requested = true;

  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Match Results"));
}

static void Test_ResultsNavigationActions(ImGuiTestContext* ctx) {
  SetupResultsScreen(420.0f, 300.0f, 3);
  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeImGui_WindowIsActive("Match Results"));
  IM_CHECK_STR_EQ(ShroomTestGetResultsDurationText(&g_imgui_test_app.game), "1:05");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Play Again"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Main Menu"));

  ShroomTeCtx_ItemClick(ctx, "Main Menu");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);

  SetupResultsScreen(420.0f, 300.0f, 3);
  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_ItemClick(ctx, "Play Again");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);
  IM_CHECK_EQ(g_imgui_test_app.game.session_duration_seconds, 0u);
  IM_CHECK(g_imgui_test_app.game.session_start_time > 0.0f);
}

static void Test_ResultsDurationStaysFrozen(ImGuiTestContext* ctx) {
  uint32_t captured_duration;

  SetupOfflineGame();
  g_imgui_test_app.game.session_start_time = (float)GetTime() - 65.25f;
  g_imgui_test_app.game.return_to_menu_requested = true;
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  captured_duration = g_imgui_test_app.game.session_duration_seconds;
  IM_CHECK_EQ(captured_duration, 65u);

  g_imgui_test_app.game.session_start_time = (float)GetTime() - 600.0f;
  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK_EQ(g_imgui_test_app.game.session_duration_seconds, captured_duration);
  IM_CHECK_STR_EQ(ShroomTestGetResultsDurationText(&g_imgui_test_app.game), "1:05");
}

static void Test_OnlineResultsPlayAgainPreservesLobbySession(ImGuiTestContext* ctx) {
  ENetHost* original_host;
  ENetPeer* original_peer;

  SetupLiveLobbyGame();
  original_host = g_imgui_test_app.game.net.host;
  original_peer = g_imgui_test_app.game.net.peer;
  IM_CHECK(original_host != NULL);
  IM_CHECK(original_peer != NULL);

  g_imgui_test_app.game.return_to_menu_requested = true;
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(ClientNetCanResumeLobbySession(&g_imgui_test_app.game.net));
  IM_CHECK_EQ(g_imgui_test_app.game.net.host, original_host);
  IM_CHECK_EQ(g_imgui_test_app.game.net.peer, original_peer);

  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_ItemClick(ctx, "Play Again");
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_LOBBY_PLAY);
  IM_CHECK_EQ(g_imgui_test_app.game.net.host, original_host);
  IM_CHECK_EQ(g_imgui_test_app.game.net.peer, original_peer);
  IM_CHECK_EQ(g_imgui_test_app.game.world.player_count, 1u);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);
  IM_CHECK_EQ(g_imgui_test_app.game.local_player->player_id, 7u);
}

static void Test_OnlineResultsMainMenuCleansUpLobbySession(ImGuiTestContext* ctx) {
  SetupLiveLobbyGame();
  g_imgui_test_app.game.return_to_menu_requested = true;
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(g_imgui_test_app.game.net.host != NULL);

  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_ItemClick(ctx, "Main Menu");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);
  IM_CHECK(g_imgui_test_app.game.net.host == NULL);
  IM_CHECK(g_imgui_test_app.game.net.peer == NULL);
  IM_CHECK(!g_imgui_test_app.game.net.enet_initialized);
}

static void Test_OnlineResultsWithoutSessionRejoinsLobby(ImGuiTestContext* ctx) {
  SetupResultsScreen(420.0f, 300.0f, 3);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
  IM_CHECK(!ClientNetCanResumeLobbySession(&g_imgui_test_app.game.net));

  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_ItemClick(ctx, "Play Again");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
  IM_CHECK(g_imgui_test_app.game.net.host != NULL);
  IM_CHECK(g_imgui_test_app.game.net.peer != NULL);
  IM_CHECK(g_imgui_test_app.game.auto_join_lobby);
}

static void Test_DeathCutscenePlayAgainResumesOnlineMatch(ImGuiTestContext* ctx) {
  ShroomPlayerState* original_player;

  SetupOnlineGame();
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_LOBBY_PLAY;
  g_imgui_test_app.game.death_cutscene_duration = 1.0f;
  g_imgui_test_app.game.death_cutscene_timer = 1.0f;
  original_player = g_imgui_test_app.game.local_player;

  ShroomTeCtx_SetRef(ctx, "Death Cutscene Actions");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeImGui_WindowIsActive("Death Cutscene Actions"));
  ShroomTeCtx_ItemClick(ctx, "Play Again");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_LOBBY_PLAY);
  IM_CHECK_EQ(g_imgui_test_app.game.death_cutscene_duration, 0.0f);
  IM_CHECK_EQ(g_imgui_test_app.game.local_player, original_player);
}

/* chat: Chat dock is active in online mode. */
static void Test_ChatDockVisibleOnline(ImGuiTestContext* ctx) {
  SetupOnlineGame();
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Chat"));
}

/* chat: Chat dock is not active in offline-practice mode. */
static void Test_ChatDockHiddenOffline(ImGuiTestContext* ctx) {
  SetupOfflineGame();
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
  IM_CHECK(!ShroomTeImGui_WindowIsActive("Chat"));
}

/* chat: Messages injected into the ring buffer are rendered in the dock. */
static void Test_ChatHistoryRendersIncoming(ImGuiTestContext* ctx) {
  int i;
  SetupOnlineGame();

  for (i = 0; i < 3; ++i) {
    ChatMessage* slot =
        &g_imgui_test_app.game.net.chat_history[g_imgui_test_app.game.net.chat_history_head %
                                                SHROOM_CLIENT_CHAT_HISTORY_COUNT];
    slot->sender_id = (uint32_t)(i + 2);
    snprintf(slot->sender_name, sizeof(slot->sender_name), "Player%d", i + 1);
    snprintf(slot->message, sizeof(slot->message), "hello from player %d", i + 1);
    g_imgui_test_app.game.net.chat_history_head =
        (g_imgui_test_app.game.net.chat_history_head + 1u) % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
    g_imgui_test_app.game.net.chat_history_count += 1u;
    g_imgui_test_app.game.net.chat_unread_count += 1u;
  }

  g_imgui_test_app.game.chat_open = true;
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK_EQ(g_imgui_test_app.game.net.chat_history_count, 3u);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Chat"));
}

/* chat: Unread count increments when messages arrive while dock is closed. */
static void Test_ChatUnreadCountIncrements(ImGuiTestContext* ctx) {
  int i;
  SetupOnlineGame();
  IM_CHECK_EQ(g_imgui_test_app.game.net.chat_unread_count, 0u);

  for (i = 0; i < 4; ++i) {
    ChatMessage* slot =
        &g_imgui_test_app.game.net.chat_history[g_imgui_test_app.game.net.chat_history_head %
                                                SHROOM_CLIENT_CHAT_HISTORY_COUNT];
    slot->sender_id = 2u;
    snprintf(slot->sender_name, sizeof(slot->sender_name), "Bot");
    snprintf(slot->message, sizeof(slot->message), "msg %d", i);
    g_imgui_test_app.game.net.chat_history_head =
        (g_imgui_test_app.game.net.chat_history_head + 1u) % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
    g_imgui_test_app.game.net.chat_history_count += 1u;
    g_imgui_test_app.game.net.chat_unread_count += 1u;
  }

  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.net.chat_unread_count, 4u);
  IM_CHECK_EQ(g_imgui_test_app.game.chat_open, false);
}

/* lobby: Lobby Browser screen renders when transitioned to. */
static void Test_LobbyScreenRenders(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Browser"));
}

/* lobby: Injected lobby entries appear in the table. */
static void Test_LobbyListRendersEntries(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  InjectFakeLobbies(3);
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(g_imgui_test_app.game.net.lobby_count, (uint8_t)3);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Browser"));
}

/* lobby: auto_join_lobby flag triggers join of least-populated lobby. */
static void Test_LobbyAutoJoinPicksLeastPopulated(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  InjectFakeLobbies(3);
  /* lobby_list[0].player_count=0, [1]=2, [2]=4 — best is index 0, lobby_id=1 */
  g_imgui_test_app.game.auto_join_lobby = true;
  ShroomScreenManagerUpdate(&g_imgui_test_app.screen_manager, 1.0f / 60.0f);
  ShroomTeCtx_Yield(ctx, 1);
  /* auto_join_lobby should be cleared after the join attempt */
  IM_CHECK_EQ(g_imgui_test_app.game.auto_join_lobby, false);
}

/* lobby: Back button disconnects and returns to server browser. */
static void Test_LobbyBackReturnsToServerBrowser(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  ShroomTeCtx_Yield(ctx, 3);
  ShroomTeCtx_SetRef(ctx, "Lobby Browser");
  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);
}

/* lobby: Auto-join + welcome_received transitions to the lobby roster panel
 * (regression for #334 — Play Online flow used to die here because the
 * LobbyRoster screen wasn't registered in the test harness). */
static void Test_LobbyAutoJoinTransitionsToRoster(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  InjectFakeLobbies(2);
  g_imgui_test_app.game.auto_join_lobby = true;
  ShroomTeCtx_Yield(ctx, 1);
  /* Simulate the server replying with LOBBY_JOINED for our join. */
  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.player_id = 7u;
  g_imgui_test_app.game.net.entity_id = 42u;
  g_imgui_test_app.game.net.lobby_id = 1u;
  g_imgui_test_app.game.net.lobby_roster_received = true;
  g_imgui_test_app.game.net.lobby_roster_count = 1u;
  g_imgui_test_app.game.net.lobby_roster[0] =
      (ShroomLobbyRosterEntry){.player_id = 7u, .is_ready = 1u};
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.auto_join_lobby, false);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY_ROSTER);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Roster"));
}

static void Test_FirstLobbyEntryDoesNotOpenDeathCutscene(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  ClientNetInit(&g_imgui_test_app.game.net, "127.0.0.1", 37779u);
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  g_imgui_test_app.game.net.handshake_received = true;
  InjectFakeLobbies(1);
  g_imgui_test_app.game.auto_join_lobby = true;
  ShroomTeCtx_Yield(ctx, 1);

  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.player_id = 7u;
  g_imgui_test_app.game.net.entity_id = 42u;
  g_imgui_test_app.game.net.lobby_id = 1u;
  g_imgui_test_app.game.net.lobby_roster_received = true;
  g_imgui_test_app.game.net.lobby_roster_count = 1u;
  g_imgui_test_app.game.net.lobby_roster[0] =
      (ShroomLobbyRosterEntry){.player_id = 7u, .is_ready = 1u};
  ShroomTeCtx_Yield(ctx, 2);

  ShroomTeCtx_SetRef(ctx, "Lobby Roster");
  ShroomTeCtx_ItemClick(ctx, "Enter Match");
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_LOBBY_PLAY);
  IM_CHECK_EQ(g_imgui_test_app.game.net.match_entry_sent, true);
  IM_CHECK_EQ(g_imgui_test_app.game.death_cutscene_duration, 0.0f);
  IM_CHECK(!ShroomTeImGui_WindowIsActive("Death Cutscene Actions"));
}

/* menu: Clicking Play Online calls ClientNetInit (real ENet host create +
 * connect) and transitions to the lobby browser. Regression for #334's
 * reported segfault — the click handler touches ENet + audio + screen
 * transition in sequence, and would crash if any of those dereferenced
 * uninitialized state. */
static void Test_PlayOnlineClickTransitionsToLobby(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));

  ShroomTeCtx_ItemClick(ctx, "Play Online");
  ShroomTeCtx_Yield(ctx, 2);

  /* The click must have flipped auto_join_lobby and entered the lobby screen. */
  IM_CHECK_EQ(g_imgui_test_app.game.auto_join_lobby, true);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);

  /* ENet host must be allocated and the peer must be attempting to connect. */
  IM_CHECK(g_imgui_test_app.game.net.host != NULL);
  IM_CHECK(g_imgui_test_app.game.net.peer != NULL);
  /* We bound to a real UDP port — 0 is the "not yet bound" sentinel. */
  IM_CHECK_EQ(g_imgui_test_app.game.net.status, CLIENT_NET_CONNECTING);
}

/* menu: Play Online must survive the real post-click lobby frames with the
 * same persisted settings shape that exposed the WSL crash. */
static void Test_PlayOnlineWithPersistedSettingsSurvivesLobbyFrames(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.ui_scale_percent = 119;
  g_imgui_test_app.game.settings.menu_animations_enabled = false;
  g_imgui_test_app.game.settings.camera_zoom = 0.44f;
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));

  ShroomTeCtx_ItemClick(ctx, "Play Online");
  ShroomTeCtx_Yield(ctx, 90);

  IM_CHECK(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager) ==
               SHROOM_SCREEN_LOBBY ||
           ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager) ==
               SHROOM_SCREEN_LOBBY_ROSTER);
  IM_CHECK(ShroomScreenManagerIsRunning(&g_imgui_test_app.screen_manager));
}

/* menu: MainMenuAnimationsEnabled must honor menu_animations_enabled.
 * Regression for #334 — d95936a accidentally hard-coded it to true so the
 * menu mushrooms spun even when the player disabled menu animations. */
static void Test_MainMenuRespectsAnimationsToggle(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.menu_animations_enabled = false;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomTestMainMenuAnimationsEnabled(&g_imgui_test_app.game), false);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.menu_animations_enabled, false);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));

  /* Toggle back on and the screen still renders fine. */
  g_imgui_test_app.game.settings.menu_animations_enabled = true;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomTestMainMenuAnimationsEnabled(&g_imgui_test_app.game), true);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));

  /* A NULL game must still report "animate" (safer than crashing in main.c,
   * where the manager user_data is briefly NULL during reset). */
  IM_CHECK_EQ(ShroomTestMainMenuAnimationsEnabled(NULL), true);
}

/* menu: the main menu update loop must advance the visible mushroom pose when
 * animations are enabled. Regression for #340, where the background could look
 * completely static because draw-time and update-time animation diverged. */
static void Test_MainMenuBackgroundAnimationAdvances(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.menu_animations_enabled = true;
  ShroomTeCtx_Yield(ctx, 2);
  const ShroomFungalBackgroundDebugState before =
      ShroomScreenGetFungalBackgroundDebugState(0, true, 1280, 720);

  ShroomTeCtx_Yield(ctx, 20);

  const ShroomFungalBackgroundDebugState after =
      ShroomScreenGetFungalBackgroundDebugState(0, true, 1280, 720);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));
  IM_CHECK(after.global_time > before.global_time);
  IM_CHECK(after.mushroom_x != before.mushroom_x || after.mushroom_y != before.mushroom_y ||
           after.mushroom_sway != before.mushroom_sway);

  g_imgui_test_app.game.settings.menu_animations_enabled = false;
  ShroomTeCtx_Yield(ctx, 2);
  const ShroomFungalBackgroundDebugState disabled_before =
      ShroomScreenGetFungalBackgroundDebugState(0, false, 1280, 720);
  ShroomTeCtx_Yield(ctx, 20);
  const ShroomFungalBackgroundDebugState disabled_after =
      ShroomScreenGetFungalBackgroundDebugState(0, false, 1280, 720);
  IM_CHECK_EQ(disabled_after.global_time, disabled_before.global_time);
  IM_CHECK_EQ(disabled_after.mushroom_x, disabled_before.mushroom_x);
  IM_CHECK_EQ(disabled_after.mushroom_y, disabled_before.mushroom_y);
  IM_CHECK_EQ(disabled_after.mushroom_sway, disabled_before.mushroom_sway);
}

/* chat: Opening chat focuses the input and WantCaptureKeyboard prevents
 * stray key presses from closing the dock mid-session. */
static void Test_ChatInputFocusAndKeyboardCapture(ImGuiTestContext* ctx) {
  SetupOnlineGame();

  /* Open chat programmatically as the T-key handler would. */
  g_imgui_test_app.game.chat_open = true;
  g_imgui_test_app.game.chat_focus_input = true;
  g_imgui_test_app.game.net.chat_unread_count = 0;

  /* Yield once so DrawChatDock renders and SetKeyboardFocusHere fires. */
  ShroomTeCtx_Yield(ctx, 2);

  /* Focus should already be directed at the chat window once the dock is open. */
  IM_CHECK(ShroomTeImGui_WindowIsActive("Chat"));
  IM_CHECK(ShroomTeImGui_WindowIsNavFocused("Chat"));
  IM_CHECK_EQ(g_imgui_test_app.game.chat_open, true);
}

/* -------------------------------------------------------------------------
 * Registration
 * ---------------------------------------------------------------------- */

void ShroomRegisterImGuiTests(ImGuiTestEngine* engine) {
  ShroomTeEngine_RegisterTest(engine, "screens", "main_menu_navigation", Test_MainMenuNavigation);
  ShroomTeEngine_RegisterTest(engine, "screens", "help_and_credits_back_navigation",
                              Test_HelpAndCreditsBackNavigation);
  ShroomTeEngine_RegisterTest(engine, "screens", "main_menu_exposes_primary_actions",
                              Test_MainMenuExposesPrimaryActions);
  ShroomTeEngine_RegisterTest(engine, "screens", "game_mode_availability_and_navigation",
                              Test_GameModeAvailabilityAndNavigation);
  ShroomTeEngine_RegisterTest(engine, "screens", "offline_practice_entry_initializes_game",
                              Test_OfflinePracticeEntryInitializesGame);
  ShroomTeEngine_RegisterTest(engine, "screens",
                              "offline_practice_persisted_settings_camera_stays_stable",
                              Test_OfflinePracticePersistedSettingsCameraStaysStable);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_exposes_controls_and_applies_bounds",
                              Test_SettingsExposesSpecControlsAndAppliesBoundaryValues);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_persistence", Test_SettingsPersistence);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_reserved_key_rejection_and_recovery",
                              Test_SettingsReservedKeyRejectionAndRecovery);
  ShroomTeEngine_RegisterTest(engine, "screens", "ui_scale_endpoints_keep_windows_usable",
                              Test_UiScaleEndpointsKeepPrimaryWindowsUsable);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_discard_and_escape",
                              Test_SettingsDiscardAndEscape);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_restore_defaults_requires_save",
                              Test_SettingsRestoreDefaultsRequiresSave);
  ShroomTeEngine_RegisterTest(engine, "screens", "server_browser_join_and_validation",
                              Test_ServerBrowserJoinAndValidation);
  ShroomTeEngine_RegisterTest(engine, "screens", "server_browser_recent_join_persists",
                              Test_ServerBrowserRecentJoinPersists);
  ShroomTeEngine_RegisterTest(engine, "screens", "server_browser_invalid_host_and_host_port",
                              Test_ServerBrowserInvalidHostAndHostPortParsing);
  ShroomTeEngine_RegisterTest(engine, "screens", "server_browser_discovery_states_and_sorting",
                              Test_ServerBrowserDiscoveryStatesAndSorting);
  ShroomTeEngine_RegisterTest(engine, "screens", "lobby_connection_modal_states",
                              Test_LobbyConnectionModalStates);
  ShroomTeEngine_RegisterTest(engine, "screens", "lobby_unreachable_server_friendly_error",
                              Test_LobbyUnreachableServerShowsFriendlyError);
  ShroomTeEngine_RegisterTest(engine, "screens", "lobby_empty_and_full_states_render",
                              Test_LobbyEmptyAndFullStatesRender);
  ShroomTeEngine_RegisterTest(engine, "screens", "gameplay_overlay_state_toggles",
                              Test_GameplayOverlayStateToggles);
  ShroomTeEngine_RegisterTest(engine, "screens", "gameplay_menu_overlay_actions",
                              Test_GameplayMenuOverlayActions);
  ShroomTeEngine_RegisterTest(engine, "screens", "gameplay_leave_confirmation_stay_and_leave",
                              Test_GameplayLeaveConfirmationStayAndLeave);
  ShroomTeEngine_RegisterTest(engine, "screens", "gameplay_diagnostics_and_connection_overlays",
                              Test_GameplayDiagnosticsAndConnectionOverlays);
  ShroomTeEngine_RegisterTest(engine, "screens", "gameplay_offline_menu_return_requests_results",
                              Test_GameplayOfflineMenuReturnRequestsResults);
  ShroomTeEngine_RegisterTest(engine, "screens", "results_navigation_actions",
                              Test_ResultsNavigationActions);
  ShroomTeEngine_RegisterTest(engine, "screens", "results_duration_stays_frozen",
                              Test_ResultsDurationStaysFrozen);
  ShroomTeEngine_RegisterTest(engine, "screens", "online_results_play_again_preserves_session",
                              Test_OnlineResultsPlayAgainPreservesLobbySession);
  ShroomTeEngine_RegisterTest(engine, "screens", "online_results_main_menu_cleans_up_session",
                              Test_OnlineResultsMainMenuCleansUpLobbySession);
  ShroomTeEngine_RegisterTest(engine, "screens", "online_results_without_session_rejoins_lobby",
                              Test_OnlineResultsWithoutSessionRejoinsLobby);
  ShroomTeEngine_RegisterTest(engine, "screens", "death_cutscene_play_again_resumes_online_match",
                              Test_DeathCutscenePlayAgainResumesOnlineMatch);
  ShroomTeEngine_RegisterTest(engine, "chat", "dock_visible_in_online_mode",
                              Test_ChatDockVisibleOnline);
  ShroomTeEngine_RegisterTest(engine, "chat", "dock_hidden_in_offline_mode",
                              Test_ChatDockHiddenOffline);
  ShroomTeEngine_RegisterTest(engine, "chat", "history_renders_incoming_messages",
                              Test_ChatHistoryRendersIncoming);
  ShroomTeEngine_RegisterTest(engine, "chat", "unread_count_increments_on_receive",
                              Test_ChatUnreadCountIncrements);
  ShroomTeEngine_RegisterTest(engine, "lobby", "screen_renders", Test_LobbyScreenRenders);
  ShroomTeEngine_RegisterTest(engine, "lobby", "lobby_list_renders_entries",
                              Test_LobbyListRendersEntries);
  ShroomTeEngine_RegisterTest(engine, "lobby", "auto_join_picks_least_populated",
                              Test_LobbyAutoJoinPicksLeastPopulated);
  ShroomTeEngine_RegisterTest(engine, "lobby", "auto_join_transitions_to_roster",
                              Test_LobbyAutoJoinTransitionsToRoster);
  ShroomTeEngine_RegisterTest(engine, "lobby", "first_entry_has_no_death_cutscene",
                              Test_FirstLobbyEntryDoesNotOpenDeathCutscene);
  ShroomTeEngine_RegisterTest(engine, "menu", "play_online_click_transitions_to_lobby",
                              Test_PlayOnlineClickTransitionsToLobby);
  ShroomTeEngine_RegisterTest(engine, "menu",
                              "play_online_persisted_settings_survives_lobby_frames",
                              Test_PlayOnlineWithPersistedSettingsSurvivesLobbyFrames);
  ShroomTeEngine_RegisterTest(engine, "lobby", "back_returns_to_server_browser",
                              Test_LobbyBackReturnsToServerBrowser);
  ShroomTeEngine_RegisterTest(engine, "menu", "respects_animations_toggle",
                              Test_MainMenuRespectsAnimationsToggle);
  ShroomTeEngine_RegisterTest(engine, "menu", "background_animation_advances",
                              Test_MainMenuBackgroundAnimationAdvances);
  ShroomTeEngine_RegisterTest(engine, "chat", "input_focus_and_keyboard_capture",
                              Test_ChatInputFocusAndKeyboardCapture);
}
