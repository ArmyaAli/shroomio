#include "app.h"
#include "imgui_te_wrapper.h"

#include "client/audio.h"
#include "client/results_summary.h"
#include "client/screens/screen_background.h"
#include "client/server_browser_model.h"
#include "shared/protocol.h"
#include "shared/sim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct ImGuiAudioBackend {
  bool ready;
  int init_count;
  int close_count;
  int load_count;
  int unload_count;
  int update_count;
  ClientSettings applied_settings;
} ImGuiAudioBackend;

static ImGuiAudioBackend g_imgui_audio_backend;

static bool ImGuiAudioInit(void* context) {
  ImGuiAudioBackend* backend = context;
  backend->init_count += 1;
  backend->ready = true;
  return true;
}

static bool ImGuiAudioReady(void* context) { return ((ImGuiAudioBackend*)context)->ready; }

static void ImGuiAudioClose(void* context) {
  ImGuiAudioBackend* backend = context;
  backend->close_count += 1;
  backend->ready = false;
}

static bool ImGuiAudioLoad(void* context) {
  ((ImGuiAudioBackend*)context)->load_count += 1;
  return true;
}

static void ImGuiAudioUnload(void* context) { ((ImGuiAudioBackend*)context)->unload_count += 1; }

static void ImGuiAudioApply(void* context, const ClientSettings* settings) {
  ((ImGuiAudioBackend*)context)->applied_settings = *settings;
}

static void ImGuiAudioUpdate(void* context, const ClientSettings* settings) {
  ImGuiAudioBackend* backend = context;
  backend->update_count += 1;
  backend->applied_settings = *settings;
}

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

static void InjectFeedbackSnapshot(ShroomMatchPhase phase, uint32_t local_entity_id,
                                   ShroomVec2 local_position, float local_mass,
                                   ShroomVec2 opponent_position, float opponent_mass) {
  ClientNetState* net = &g_imgui_test_app.game.net;

  net->last_snapshot_tick += 1u;
  net->match_phase = (uint8_t)phase;
  net->match_time_remaining = phase == SHROOM_MATCH_PHASE_RUNNING ? 120.0f : 0.0f;
  net->snapshot_player_count = 2u;
  net->snapshot_players[0] = (ShroomSnapshotPlayerState){
      .player_id = 1u,
      .entity_id = local_entity_id,
      .position_x = local_position.x,
      .position_y = local_position.y,
      .mass = local_mass,
      .radius = 20.0f,
      .alive = 1u,
  };
  snprintf(net->snapshot_players[0].name, sizeof(net->snapshot_players[0].name), "Local");
  net->snapshot_players[1] = (ShroomSnapshotPlayerState){
      .player_id = 2u,
      .entity_id = 301u,
      .position_x = opponent_position.x,
      .position_y = opponent_position.y,
      .mass = opponent_mass,
      .radius = 24.0f,
      .alive = 1u,
      .is_bot = 1u,
  };
  snprintf(net->snapshot_players[1].name, sizeof(net->snapshot_players[1].name), "Opponent");
}

static void SetupLiveLobbyGame(void) {
  ShroomSnapshotPlayerState* player;

  SetupOfflineGame();
  ClientNetInit(&g_imgui_test_app.game.net, "127.0.0.1", 37779u,
                g_imgui_test_app.game.settings.player_name);
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

static void SetupPopulatedLobbyRoster(int ui_scale_percent) {
  ClientNetState* net;

  SetupLobbyBrowser();
  net = &g_imgui_test_app.game.net;
  g_imgui_test_app.game.settings.ui_scale_percent = ui_scale_percent;
  net->welcome_received = true;
  net->player_id = 7u;
  net->lobby_id = 1u;
  net->lobby_roster_received = true;
  net->lobby_roster_count = SHROOM_MAX_PLAYABLE_PARTICIPANTS;
  net->snapshot_player_count = SHROOM_MAX_PLAYABLE_PARTICIPANTS;

  for (uint16_t i = 0u; i < SHROOM_MAX_PLAYABLE_PARTICIPANTS; ++i) {
    const uint16_t player_id = (uint16_t)(i + 1u);
    net->lobby_roster[i] = (ShroomLobbyRosterEntry){.player_id = player_id, .is_ready = 1u};
    net->snapshot_players[i] = (ShroomSnapshotPlayerState){
        .player_id = player_id,
        .entity_id = (uint32_t)(100u + i),
        .alive = 1u,
    };
  }

  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY_ROSTER);
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
  ShroomTeCtx_ItemClick(ctx, "Gameplay");
  ShroomTeCtx_Yield(ctx, 1);
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

static void Test_HelpContentMatchesCurrentGameplay(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.key_chat_open = KEY_Y;
  g_imgui_test_app.game.settings.key_hud_toggle = KEY_F6;
  g_imgui_test_app.game.settings.key_pause_menu = KEY_P;

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Help");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTestHelpRenderedTextContains("[Mouse] Move toward the cursor"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("[Space] Hold 0.6s to split (mass 384+)"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("[E / RMB] Eject mass (mass 168+)"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("[Y / Enter] Open online chat"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("[F6] Cycle HUD density"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("[P] Pause / match menu"));

  ShroomTeCtx_SetRef(ctx, "How To Play");
  ShroomTeCtx_ItemClick(ctx, "Gameplay");
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK(ShroomTestHelpRenderedTextContains("18% heavier (8% in center)"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("gain 58% of consumed mass"));
  IM_CHECK(!ShroomTestHelpRenderedTextContains("%%"));

  ShroomTeCtx_ItemClick(ctx, "Zones");
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK(ShroomTestHelpRenderedTextContains("Center: consume at 8% advantage"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("Mid: consume at 18% advantage"));
  IM_CHECK(!ShroomTestHelpRenderedTextContains("%%"));

  ShroomTeCtx_ItemClick(ctx, "Modes");
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK(ShroomTestHelpRenderedTextContains("Free-for-All (FFA) - Available"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("Teams 2v2 - Unavailable"));
  IM_CHECK(ShroomTestHelpRenderedTextContains("King of the Hill - Available"));
}

static void Test_HelpCardHeadingsAreNotInteractive(ImGuiTestContext* ctx) {
  const char* tab_labels[] = {"Controls", "Gameplay", "Zones", "Modes"};
  const char* heading_labels[] = {"Controls", "Growth Rules", "Zones", "Game Modes"};
  char child_path[64];

  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Help");

  for (size_t index = 0; index < sizeof(tab_labels) / sizeof(tab_labels[0]); ++index) {
    ShroomTeCtx_SetRef(ctx, "How To Play");
    if (index > 0u) {
      ShroomTeCtx_ItemClick(ctx, tab_labels[index]);
    }
    ShroomTeCtx_Yield(ctx, 1);
    IM_CHECK_STR_EQ(ShroomTestHelpRenderedHeading(), heading_labels[index]);

    snprintf(child_path, sizeof(child_path), "//How To Play/%s", heading_labels[index]);
    IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, child_path));
    IM_CHECK(!ShroomTeCtx_ItemExists(ctx, heading_labels[index]));
  }
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

static void Test_PlayerIdentityOnboardingPersistsAndStartsSession(ImGuiTestContext* ctx) {
  ClientSettings loaded;

  ShroomImGuiTestAppReset(true);
  g_imgui_test_app.game.settings.player_name[0] = '\0';
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_HELP);
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_MAIN_MENU);
  ShroomTeCtx_SetRef(ctx, "Player Identity");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeImGui_WindowIsActive("Player Identity"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Player Name"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Continue"));
  ShroomTeCtx_ItemInputValueStr(ctx, "Player Name", "  Moss@@   Runner!!  ");
  ShroomTeCtx_ItemClick(ctx, "Continue");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_STR_EQ(g_imgui_test_app.game.settings.player_name, "Moss Runner");
  IM_CHECK(ClientSettingsLoad(&loaded));
  IM_CHECK_STR_EQ(loaded.player_name, "Moss Runner");
  IM_CHECK(ShroomTeImGui_WindowIsActive("Main Menu"));

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Play Online");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_STR_EQ(g_imgui_test_app.game.net.player_name, "Moss Runner");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
}

static void Test_PlayerNameInputsStayVisibleAtUiScaleEndpoints(ImGuiTestContext* ctx) {
  const int scales[] = {80, 100, 160};
  const char* short_name = "Moss";
  const char* max_name = "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";

  IM_CHECK_EQ(strlen(max_name), SHROOM_MAX_NAME_LENGTH - 1u);

  for (size_t index = 0; index < sizeof(scales) / sizeof(scales[0]); ++index) {
    ShroomImGuiTestAppReset(true);
    g_imgui_test_app.game.settings.ui_scale_percent = scales[index];
    g_imgui_test_app.game.settings.player_name[0] = '\0';
    ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_HELP);
    ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_MAIN_MENU);
    ShroomTeCtx_SetRef(ctx, "Player Identity");
    ShroomTeCtx_Yield(ctx, 2);

    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Player Identity"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Player Name"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Player Name"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Continue"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Continue"));

    ShroomTeCtx_ItemInputValueStr(ctx, "Player Name", short_name);
    ShroomTeCtx_Yield(ctx, 1);
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Player Name"));
    ShroomTeCtx_ItemInputValueStr(ctx, "Player Name", max_name);
    ShroomTeCtx_Yield(ctx, 1);
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Player Name"));
    ShroomTeCtx_ItemClick(ctx, "Continue");
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK_STR_EQ(g_imgui_test_app.game.settings.player_name, max_name);

    ShroomTeCtx_SetRef(ctx, "Main Menu");
    ShroomTeCtx_ItemClick(ctx, "Settings");
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Settings"));
    IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Player Name"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Player Name"));
    ShroomTeCtx_ItemInputValueStr(ctx, "Player Name", short_name);
    ShroomTeCtx_Yield(ctx, 1);
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Player Name"));
    ShroomTeCtx_ItemInputValueStr(ctx, "Player Name", max_name);
    ShroomTeCtx_Yield(ctx, 1);
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Player Name"));
  }
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
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "King of the Hill"));
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

static void Test_KingOfHillCompleteMatchHudAndResults(ImGuiTestContext* ctx) {
  const ShroomVec2 local_position = {SHROOM_WORLD_WIDTH * 0.5f, SHROOM_WORLD_HEIGHT * 0.5f};
  const ShroomVec2 opponent_position = {100.0f, 100.0f};

  SetupOnlineGame();
  g_imgui_test_app.game.net.match_entry_sent = true;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, local_position, 300.0f,
                         opponent_position, 200.0f);
  g_imgui_test_app.game.net.game_mode = SHROOM_GAME_MODE_KING_OF_HILL;
  g_imgui_test_app.game.net.objective_target_score = SHROOM_KOTH_TARGET_SCORE;
  g_imgui_test_app.game.net.objective_controller_id = 1u;
  g_imgui_test_app.game.net.snapshot_players[0].objective_score = 99.0f;
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomTeImGui_WindowIsActive("King of the Hill Objective"));
  IM_CHECK_EQ(g_imgui_test_app.game.world.objective_controller_id, 1u);
  IM_CHECK(fabsf(ShroomWorldGetObjectiveScore(&g_imgui_test_app.game.world, 1u) - 99.0f) < 0.001f);

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 101u, local_position, 300.0f,
                         opponent_position, 200.0f);
  g_imgui_test_app.game.net.game_mode = SHROOM_GAME_MODE_KING_OF_HILL;
  g_imgui_test_app.game.net.objective_target_score = SHROOM_KOTH_TARGET_SCORE;
  g_imgui_test_app.game.net.snapshot_players[0].objective_score = SHROOM_KOTH_TARGET_SCORE;
  g_imgui_test_app.game.net.podium_player_ids[0] = 1u;
  g_imgui_test_app.game.net.podium_masses[0] = SHROOM_KOTH_TARGET_SCORE;
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Match Results"));
  IM_CHECK(fabsf(ShroomWorldGetObjectiveScore(&g_imgui_test_app.game.world, 1u) -
                 SHROOM_KOTH_TARGET_SCORE) < 0.001f);
  IM_CHECK_EQ(g_imgui_test_app.game.final_rank, 1);
}

static void Test_AuthoritativeResultsRankBotsByKingOfHillScore(ImGuiTestContext* ctx) {
  const ShroomVec2 local_position = {600.0f, 600.0f};
  const ShroomVec2 opponent_position = {900.0f, 900.0f};
  LeaderboardEntry leaderboard[SHROOM_MAX_PLAYER_ENTITIES];
  size_t leaderboard_count = 0u;

  SetupOnlineGame();
  g_imgui_test_app.game.net.match_entry_sent = true;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 101u, local_position, 300.0f,
                         opponent_position, 100.0f);
  g_imgui_test_app.game.net.game_mode = SHROOM_GAME_MODE_KING_OF_HILL;
  g_imgui_test_app.game.net.objective_target_score = SHROOM_KOTH_TARGET_SCORE;
  g_imgui_test_app.game.net.snapshot_players[0].objective_score = 50.0f;
  g_imgui_test_app.game.net.snapshot_players[1].objective_score = 60.0f;
  g_imgui_test_app.game.net.snapshot_players[2] = (ShroomSnapshotPlayerState){
      .player_id = 3u,
      .entity_id = 302u,
      .position_x = 1100.0f,
      .position_y = 1100.0f,
      .mass = 900.0f,
      .radius = 30.0f,
      .alive = 1u,
      .is_bot = 1u,
      .objective_score = 40.0f,
  };
  snprintf(g_imgui_test_app.game.net.snapshot_players[2].name,
           sizeof(g_imgui_test_app.game.net.snapshot_players[2].name), "Heavy Bot");
  g_imgui_test_app.game.net.snapshot_player_count = 3u;
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Match Results"));
  BuildLeaderboard(&g_imgui_test_app.game, leaderboard, &leaderboard_count);
  IM_CHECK_EQ(leaderboard_count, 3u);
  IM_CHECK_EQ(leaderboard[0].player_id, 2u);
  IM_CHECK_EQ(leaderboard[1].player_id, 1u);
  IM_CHECK_EQ(leaderboard[2].player_id, 3u);
  IM_CHECK_EQ(g_imgui_test_app.game.final_rank, 2);
}

static void Test_KingOfHillModeSelection(ImGuiTestContext* ctx) {
  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Game Modes");
  ShroomTeCtx_SetRef(ctx, "Select Game Mode");
  ShroomTeCtx_ItemClick(ctx, "King of the Hill");

  IM_CHECK_EQ(g_imgui_test_app.game.selected_game_mode, SHROOM_GAME_MODE_KING_OF_HILL);
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

static void Test_OfflinePracticeBotsExerciseTacticalSplit(ImGuiTestContext* ctx) {
  ShroomPlayerState* aggressive_bot = NULL;
  size_t bot_piece_count = 0u;

  ShroomImGuiTestAppReset(true);
  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Offline Practice");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);

  for (size_t index = 0; index < g_imgui_test_app.game.world.player_count; ++index) {
    ShroomPlayerState* player = &g_imgui_test_app.game.world.players[index];

    if (player->is_bot &&
        (ShroomBotProfileForPlayer(player->player_id) == SHROOM_BOT_PROFILE_AGGRESSIVE)) {
      aggressive_bot = player;
      break;
    }
  }
  IM_CHECK(aggressive_bot != NULL);
  if (aggressive_bot == NULL) {
    return;
  }

  for (size_t index = 0; index < g_imgui_test_app.game.world.player_count; ++index) {
    ShroomPlayerState* player = &g_imgui_test_app.game.world.players[index];

    if ((player != aggressive_bot) && (player != g_imgui_test_app.game.local_player)) {
      player->alive = false;
    }
  }
  aggressive_bot->position = (ShroomVec2){2000.0f, 2000.0f};
  aggressive_bot->mass = 800.0f;
  aggressive_bot->radius = ShroomMassToRadius(aggressive_bot->mass);
  aggressive_bot->bot_tactical_cooldown_timer = 0.0f;
  g_imgui_test_app.game.local_player->position = (ShroomVec2){2200.0f, 2000.0f};
  g_imgui_test_app.game.local_player->mass = 200.0f;
  g_imgui_test_app.game.local_player->radius =
      ShroomMassToRadius(g_imgui_test_app.game.local_player->mass);
  g_imgui_test_app.game.local_player->spawn_protection_timer = 0.0f;
  g_imgui_test_app.game.world.spore_count = 0u;
  g_imgui_test_app.game.world.powerup_count = 0u;

  ShroomTeCtx_Yield(ctx, 2);
  for (size_t index = 0; index < g_imgui_test_app.game.world.player_count; ++index) {
    const ShroomPlayerState* player = &g_imgui_test_app.game.world.players[index];
    if (player->alive && (player->player_id == aggressive_bot->player_id)) {
      bot_piece_count += 1u;
    }
  }

  IM_CHECK(bot_piece_count >= 2u);
  IM_CHECK(ShroomScreenManagerIsRunning(&g_imgui_test_app.screen_manager));
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

static void Test_AudioSurvivesMatchTransitionsAndRestartsFromSettings(ImGuiTestContext* ctx) {
  ShroomClientAudioTestBackend backend;

  ShroomImGuiTestAppReset(false);
  memset(&g_imgui_audio_backend, 0, sizeof(g_imgui_audio_backend));
  backend = (ShroomClientAudioTestBackend){
      .context = &g_imgui_audio_backend,
      .init_device = ImGuiAudioInit,
      .device_ready = ImGuiAudioReady,
      .close_device = ImGuiAudioClose,
      .load_assets = ImGuiAudioLoad,
      .unload_assets = ImGuiAudioUnload,
      .apply_settings = ImGuiAudioApply,
      .update_music = ImGuiAudioUpdate,
  };
  ShroomClientAudioTestSetBackend(&backend);
  g_imgui_test_app.game.settings.master_volume_percent = 43;
  g_imgui_test_app.game.settings.music_volume_percent = 57;
  g_imgui_test_app.game.settings.effects_volume_percent = 79;
  IM_CHECK(ShroomClientAudioInit(&g_imgui_test_app.game.settings));

  for (int transition = 0; transition < 12; ++transition) {
    GameInit(&g_imgui_test_app.game, 1280, 720, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
    GameShutdown(&g_imgui_test_app.game);
    IM_CHECK(ShroomClientAudioIsReady());
  }
  IM_CHECK_EQ(g_imgui_audio_backend.init_count, 1);
  IM_CHECK_EQ(g_imgui_audio_backend.load_count, 1);
  IM_CHECK_EQ(g_imgui_audio_backend.unload_count, 0);

  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_SETTINGS);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SETTINGS);
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Settings/SettingsContent"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Restart Audio"));
  ShroomTeCtx_ItemClick(ctx, "Restart Audio");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(ShroomClientAudioIsReady());
  IM_CHECK_EQ(g_imgui_audio_backend.init_count, 2);
  IM_CHECK_EQ(g_imgui_audio_backend.load_count, 2);
  IM_CHECK_EQ(g_imgui_audio_backend.unload_count, 1);
  IM_CHECK_EQ(g_imgui_audio_backend.close_count, 1);
  IM_CHECK_EQ(g_imgui_audio_backend.applied_settings.master_volume_percent, 43);
  IM_CHECK_EQ(g_imgui_audio_backend.applied_settings.music_volume_percent, 57);
  IM_CHECK_EQ(g_imgui_audio_backend.applied_settings.effects_volume_percent, 79);
}

static void Test_AudioSurvivesMultiRoundGameplayCycle(ImGuiTestContext* ctx) {
  ShroomClientAudioTestBackend backend;
  ShroomVec2 local_pos = {500.0f, 600.0f};
  ShroomVec2 opponent_pos = {900.0f, 1000.0f};

  ShroomImGuiTestAppReset(false);
  ShroomClientAudioTestSetBackend(NULL);
  memset(&g_imgui_audio_backend, 0, sizeof(g_imgui_audio_backend));
  backend = (ShroomClientAudioTestBackend){
      .context = &g_imgui_audio_backend,
      .init_device = ImGuiAudioInit,
      .device_ready = ImGuiAudioReady,
      .close_device = ImGuiAudioClose,
      .load_assets = ImGuiAudioLoad,
      .unload_assets = ImGuiAudioUnload,
      .apply_settings = ImGuiAudioApply,
      .update_music = ImGuiAudioUpdate,
  };
  ShroomClientAudioTestSetBackend(&backend);
  IM_CHECK(ShroomClientAudioInit(&g_imgui_test_app.game.settings));

  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_GAME);
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.player_id = 1;
  snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
           "Connected");
  IM_CHECK_EQ(g_imgui_audio_backend.init_count, 1);
  IM_CHECK_EQ(g_imgui_audio_backend.load_count, 1);
  IM_CHECK_EQ(g_imgui_audio_backend.unload_count, 0);
  IM_CHECK_EQ(g_imgui_audio_backend.close_count, 0);

  for (int round = 0; round < 3; ++round) {
    uint32_t base_entity = (uint32_t)((round * 100) + 1);

    InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, base_entity, local_pos, 300.0f, opponent_pos,
                           200.0f);
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_GAME);

    InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, base_entity, local_pos, 300.0f, opponent_pos,
                           200.0f);
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_RESULTS);

    InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESET, base_entity, local_pos, 300.0f, opponent_pos,
                           200.0f);
    ShroomTeCtx_Yield(ctx, 2);

    IM_CHECK(ShroomClientAudioIsReady());
    IM_CHECK_EQ(g_imgui_audio_backend.init_count, 1);
    IM_CHECK_EQ(g_imgui_audio_backend.load_count, 1);
    IM_CHECK_EQ(g_imgui_audio_backend.unload_count, 0);
    IM_CHECK_EQ(g_imgui_audio_backend.close_count, 0);
  }
}

static void Test_AudioUpdatesOnNonGameplayScreens(ImGuiTestContext* ctx) {
  ShroomClientAudioTestBackend backend;
  int main_menu_updates;

  ShroomImGuiTestAppReset(false);
  memset(&g_imgui_audio_backend, 0, sizeof(g_imgui_audio_backend));
  backend = (ShroomClientAudioTestBackend){
      .context = &g_imgui_audio_backend,
      .init_device = ImGuiAudioInit,
      .device_ready = ImGuiAudioReady,
      .close_device = ImGuiAudioClose,
      .load_assets = ImGuiAudioLoad,
      .unload_assets = ImGuiAudioUnload,
      .apply_settings = ImGuiAudioApply,
      .update_music = ImGuiAudioUpdate,
  };
  ShroomClientAudioTestSetBackend(&backend);
  IM_CHECK(ShroomClientAudioInit(&g_imgui_test_app.game.settings));

  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_MAIN_MENU);
  ShroomTeCtx_Yield(ctx, 5);
  main_menu_updates = g_imgui_audio_backend.update_count;
  IM_CHECK(main_menu_updates >= 5);

  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_HELP);
  ShroomTeCtx_Yield(ctx, 5);
  IM_CHECK(g_imgui_audio_backend.update_count >= main_menu_updates + 5);
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

static bool RewriteSettingsAsUnversioned(void) {
  FILE* file = fopen("client_settings.cfg", "rb");
  char contents[2048];
  char* first_newline;
  size_t count;

  if (file == NULL) {
    return false;
  }
  count = fread(contents, 1u, sizeof(contents) - 1u, file);
  fclose(file);
  contents[count] = '\0';
  first_newline = strchr(contents, '\n');
  if (first_newline == NULL) {
    return false;
  }
  file = fopen("client_settings.cfg", "wb");
  if (file == NULL) {
    return false;
  }
  fputs(first_newline + 1, file);
  return fclose(file) == 0;
}

static void Test_MigratedSettingsSurviveCrossScreenWorkflow(ImGuiTestContext* ctx) {
  ClientSettings legacy;
  FILE* file;
  char schema_line[64] = {0};

  ShroomImGuiTestAppReset(true);
  ClientSettingsSetDefaults(&legacy);
  snprintf(legacy.player_name, sizeof(legacy.player_name), "%s", "Migrated Player");
  legacy.ui_scale_percent = 110;
  legacy.master_volume_percent = 63;
  legacy.camera_zoom = 1.37f;
  legacy.key_chat_open = KEY_Y;
  IM_CHECK(ClientSettingsSave(&legacy));
  IM_CHECK(RewriteSettingsAsUnversioned());
  IM_CHECK(ClientSettingsLoad(&g_imgui_test_app.game.settings));

  file = fopen("client_settings.cfg", "rb");
  IM_CHECK(file != NULL);
  if (file != NULL) {
    IM_CHECK(fgets(schema_line, sizeof(schema_line), file) != NULL);
    fclose(file);
  }
  IM_CHECK_STR_EQ(schema_line, "schema_version=1\n");
  IM_CHECK_EQ(g_imgui_test_app.game.settings.ui_scale_percent, 110);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.master_volume_percent, 63);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.key_chat_open, KEY_Y);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SETTINGS);
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemClick(ctx, "Back");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_MAIN_MENU);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Offline Practice");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK_STR_EQ(g_imgui_test_app.game.settings.player_name, "Migrated Player");
  IM_CHECK(fabsf(g_imgui_test_app.game.settings.camera_zoom - 1.37f) < 0.001f);
  IM_CHECK_EQ(g_imgui_test_app.game.settings.key_chat_open, KEY_Y);
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

  for (size_t index = 0; index < sizeof(scales) / sizeof(scales[0]); ++index) {
    SetupOnlineGame();
    g_imgui_test_app.game.settings.ui_scale_percent = scales[index];
    g_imgui_test_app.game.leaderboard_overlay_open = true;
    ShroomTeCtx_SetRef(ctx, "Leaderboard");
    ShroomTeCtx_Yield(ctx, 3);

    IM_CHECK(ShroomTeImGui_WindowFitsViewport("HUD Left"));
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("HUD Right"));
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Chat"));
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Leaderboard"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Close"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Close"));

    g_imgui_test_app.game.leaderboard_overlay_open = false;
    g_imgui_test_app.game.menu_overlay_open = true;
    ShroomTeCtx_SetRef(ctx, "Match Menu");
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Match Menu"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Resume"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Return To Main Menu"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Resume"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Return To Main Menu"));

    g_imgui_test_app.game.menu_overlay_open = false;
    g_imgui_test_app.game.leave_confirmation_open = true;
    ShroomTeCtx_SetRef(ctx, "Leave Match?");
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Leave Match?"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Leave Match"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Stay"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Leave Match"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Stay"));

    g_imgui_test_app.game.leave_confirmation_open = false;
    g_imgui_test_app.game.diagnostics_overlay_open = true;
    ShroomTeCtx_SetRef(ctx, "Diagnostics");
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Diagnostics"));

    g_imgui_test_app.game.diagnostics_overlay_open = false;
    g_imgui_test_app.game.net.welcome_received = false;
    g_imgui_test_app.game.net.status = CLIENT_NET_ERROR;
    snprintf(g_imgui_test_app.game.net.status_text, sizeof(g_imgui_test_app.game.net.status_text),
             "timeout");
    ShroomTeCtx_SetRef(ctx, "Connection Status");
    ShroomTeCtx_Yield(ctx, 2);
    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Connection Status"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Retry"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Back To Menu"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Retry"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Back To Menu"));
  }
}

static void Test_LobbyRosterScrollKeepsActionsUsable(ImGuiTestContext* ctx) {
  const int scales[] = {80, 100, 160};

  for (size_t index = 0; index < sizeof(scales) / sizeof(scales[0]); ++index) {
    SetupPopulatedLobbyRoster(scales[index]);
    ShroomTeCtx_SetRef(ctx, "Lobby Roster");
    ShroomTeCtx_Yield(ctx, 3);

    IM_CHECK(ShroomTeImGui_WindowFitsViewport("Lobby Roster"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Ready to enter"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Leave Lobby"));
    IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Enter Match"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Ready to enter"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Leave Lobby"));
    IM_CHECK(ShroomTeCtx_ItemIsFullyVisible(ctx, "Enter Match"));
  }
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

static void Test_LeaderboardAggregatesSplitColony(ImGuiTestContext* ctx) {
  LeaderboardEntry before[SHROOM_MAX_PLAYER_ENTITIES];
  LeaderboardEntry after[SHROOM_MAX_PLAYER_ENTITIES];
  size_t before_count = 0u;
  size_t after_count = 0u;
  float local_mass = 0.0f;

  SetupOnlineGame();
  BuildLeaderboard(&g_imgui_test_app.game, before, &before_count);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);
  IM_CHECK(g_imgui_test_app.game.world.player_count < SHROOM_MAX_PLAYER_ENTITIES);

  const size_t fragment_index = g_imgui_test_app.game.world.player_count++;
  g_imgui_test_app.game.world.players[fragment_index] = *g_imgui_test_app.game.local_player;
  g_imgui_test_app.game.world.players[fragment_index].entity_id = 99999u;
  g_imgui_test_app.game.world.players[fragment_index].piece_index = 1u;
  g_imgui_test_app.game.world.players[fragment_index].mass = 25.0f;
  BuildLeaderboard(&g_imgui_test_app.game, after, &after_count);

  IM_CHECK_EQ(after_count, before_count);
  for (size_t index = 0; index < after_count; ++index) {
    if (after[index].player_id == g_imgui_test_app.game.local_player->player_id) {
      local_mass = after[index].mass;
      break;
    }
  }
  IM_CHECK_EQ(local_mass, g_imgui_test_app.game.local_player->mass + 25.0f);

  g_imgui_test_app.game.leaderboard_overlay_open = true;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Leaderboard"));
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
  const uint64_t telemetry_now = enet_time_get();

  SetupOnlineGame();
  g_imgui_test_app.game.diagnostics_overlay_open = true;
  g_imgui_test_app.game.net.rtt_ms = 15000u;
  g_imgui_test_app.game.net.rtt_average_ms = 14000u;
  for (size_t index = 0u; index < 30u; ++index) {
    ShroomNetTelemetryRecordSent(&g_imgui_test_app.game.net.telemetry, 0u,
                                 SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT, 512u,
                                 telemetry_now);
  }
  for (size_t index = 0u; index < 15u; ++index) {
    ShroomNetTelemetryRecordAccepted(&g_imgui_test_app.game.net.telemetry, 0u,
                                     SHROOM_ENET_CHANNEL_SNAPSHOT, SHROOM_PACKET_SNAPSHOT, 1024u,
                                     telemetry_now);
  }
  ShroomNetTelemetrySetPeerTransport(&g_imgui_test_app.game.net.telemetry, 0u, 70u, 250u, true);

  ShroomTeCtx_SetRef(ctx, "Diagnostics");
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Diagnostics"));
  IM_CHECK_STR_EQ(ShroomTestGetDiagnosticsRatesText(), "Input: 30 Hz  Snapshot: 15 Hz");
  IM_CHECK_STR_EQ(ShroomTestGetDiagnosticsBandwidthText(), "In: 15.0 KiB/s  Out: 15.0 KiB/s");
  IM_CHECK_STR_EQ(ShroomTestGetDiagnosticsTransportText(), "Loss: 2.50%  Queue: 70 (congested)");

  g_imgui_test_app.game.diagnostics_overlay_open = false;
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

static void Test_MatchResetRebaselinesFeedback(ImGuiTestContext* ctx) {
  const ShroomVec2 opening_local_position = {120.0f, 140.0f};
  const ShroomVec2 opening_opponent_position = {1800.0f, 1800.0f};
  const ShroomVec2 reset_local_position = {1100.0f, 1000.0f};
  const ShroomVec2 reset_opponent_position = {1700.0f, 1700.0f};

  SetupOnlineGame();
  g_imgui_test_app.game.particle_baseline_ready = false;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, opening_local_position, 800.0f,
                         opening_opponent_position, 900.0f);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.world.match_phase, SHROOM_MATCH_PHASE_RUNNING);

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 101u, opening_local_position, 800.0f,
                         opening_opponent_position, 900.0f);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.world.match_phase, SHROOM_MATCH_PHASE_RESULTS);

  g_imgui_test_app.game.death_cutscene_duration = 2.8f;
  g_imgui_test_app.game.death_cutscene_timer = 2.8f;
  g_imgui_test_app.game.death_camera_hold_timer = 1.0f;
  g_imgui_test_app.game.screen_flash_timer = 1.0f;
  g_imgui_test_app.game.notification_count = 1u;
  g_imgui_test_app.game.notifications[0].active = true;
  g_imgui_test_app.game.kill_feed_count = 1u;
  g_imgui_test_app.game.kill_feed[0].active = true;
  g_imgui_test_app.game.particle_cursor = 1u;
  g_imgui_test_app.game.particles[0].active = true;

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, reset_local_position,
                         SHROOM_DEFAULT_PLAYER_MASS, reset_opponent_position,
                         SHROOM_DEFAULT_PLAYER_MASS);
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(g_imgui_test_app.game.world.match_phase, SHROOM_MATCH_PHASE_RUNNING);
  IM_CHECK_EQ(g_imgui_test_app.game.death_cutscene_duration, 0.0f);
  IM_CHECK_EQ(g_imgui_test_app.game.death_camera_hold_timer, 0.0f);
  IM_CHECK_EQ(g_imgui_test_app.game.screen_flash_timer, 0.0f);
  IM_CHECK_EQ(g_imgui_test_app.game.notification_count, 0u);
  IM_CHECK_EQ(g_imgui_test_app.game.kill_feed_count, 0u);
  IM_CHECK_EQ(g_imgui_test_app.game.particle_cursor, 0u);
  IM_CHECK_EQ(g_imgui_test_app.game.gameplay_event_count, 0u);
  IM_CHECK(!ShroomTeImGui_WindowIsActive("Death Cutscene Actions"));

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 202u, reset_local_position,
                         SHROOM_DEFAULT_PLAYER_MASS, reset_opponent_position,
                         SHROOM_DEFAULT_PLAYER_MASS + 64.0f);
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK(g_imgui_test_app.game.death_cutscene_duration > 0.0f);
  IM_CHECK(g_imgui_test_app.game.notification_count > 0u);
  IM_CHECK(g_imgui_test_app.game.kill_feed_count > 0u);
  IM_CHECK(g_imgui_test_app.game.screen_flash_timer > 0.0f);

  g_imgui_test_app.game.death_cutscene_timer = g_imgui_test_app.game.death_cutscene_duration;
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Death Cutscene Actions"));
}

static void Test_OnlinePredictionMovesImmediatelyAndReconciles(ImGuiTestContext* ctx) {
  const ShroomVec2 start = {SHROOM_WORLD_WIDTH * 0.5f, SHROOM_WORLD_HEIGHT * 0.5f};
  const ShroomVec2 opponent = {start.x + 1000.0f, start.y + 1000.0f};
  float before_input_x;
  float before_input_y;

  SetupOnlineGame();
  g_imgui_test_app.game.net.match_entry_sent = true;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, start, SHROOM_DEFAULT_PLAYER_MASS,
                         opponent, SHROOM_DEFAULT_PLAYER_MASS);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(g_imgui_test_app.game.local_player != NULL);
  IM_CHECK(g_imgui_test_app.game.snapshot_applied);

  before_input_x = g_imgui_test_app.game.local_player->position.x;
  before_input_y = g_imgui_test_app.game.local_player->position.y;
  g_imgui_test_app.game.death_cutscene_duration = 0.0f;
  g_imgui_test_app.game.menu_overlay_open = false;
  g_imgui_test_app.game.leaderboard_overlay_open = false;
  g_imgui_test_app.game.leave_confirmation_open = false;
  g_imgui_test_app.game.chat_open = false;
  g_imgui_test_app.frame_delta_override = 1.0f / 60.0f;
  GameTestSetMovementInput((ShroomVec2){1.0f, 0.0f});
  ShroomTeCtx_Yield(ctx, 8);
  IM_CHECK(ShroomDistanceSqr((ShroomVec2){before_input_x, before_input_y},
                             g_imgui_test_app.game.local_player->position) > 0.0001f);
  IM_CHECK(ShroomDistanceSqr((ShroomVec2){before_input_x, before_input_y},
                             g_imgui_test_app.game.render_positions[0]) > 0.0001f);
  GameTestSetMovementInput((ShroomVec2){0});
  g_imgui_test_app.frame_delta_override = 0.0f;

  SetMousePosition(640, 360);
  ShroomVec2 small_correction = g_imgui_test_app.game.local_player->position;
  small_correction.x -= 20.0f;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, small_correction,
                         SHROOM_DEFAULT_PLAYER_MASS, opponent, SHROOM_DEFAULT_PLAYER_MASS);
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK(fabsf(g_imgui_test_app.game.render_positions[0].x -
                 g_imgui_test_app.game.local_player->position.x) > 0.01f);
  IM_CHECK(fabsf(g_imgui_test_app.game.render_positions[0].x -
                 g_imgui_test_app.game.local_player->position.x) < SHROOM_PREDICTION_SNAP_DISTANCE);

  ShroomVec2 large_correction = g_imgui_test_app.game.local_player->position;
  large_correction.x += SHROOM_PREDICTION_SNAP_DISTANCE * 2.0f;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, large_correction,
                         SHROOM_DEFAULT_PLAYER_MASS, opponent, SHROOM_DEFAULT_PLAYER_MASS);
  ShroomTeCtx_Yield(ctx, 1);
  IM_CHECK(fabsf(g_imgui_test_app.game.render_positions[0].x -
                 g_imgui_test_app.game.local_player->position.x) < 0.01f);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
}

static void Test_AuthoritativeResultsCompleteTwoRoundCycle(ImGuiTestContext* ctx) {
  const ShroomVec2 local_position = {500.0f, 600.0f};
  const ShroomVec2 opponent_position = {900.0f, 1000.0f};

  SetupOnlineGame();
  g_imgui_test_app.game.net.match_entry_sent = true;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 101u, local_position, 300.0f,
                         opponent_position, 200.0f);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 101u, local_position, 300.0f,
                         opponent_position, 200.0f);
  g_imgui_test_app.game.net.snapshot_players[0].round_spores = 17u;
  g_imgui_test_app.game.net.snapshot_players[0].round_kills = 3u;
  g_imgui_test_app.game.net.snapshot_players[2] = g_imgui_test_app.game.net.snapshot_players[0];
  g_imgui_test_app.game.net.snapshot_players[2].entity_id = 102u;
  g_imgui_test_app.game.net.snapshot_players[2].mass = 25.0f;
  g_imgui_test_app.game.net.snapshot_player_count = 3u;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Match Results"));
  IM_CHECK(!ShroomTeCtx_ItemExists(ctx, "Play Again"));
  IM_CHECK_EQ(g_imgui_test_app.game.final_rank, 1);
  IM_CHECK(fabsf(g_imgui_test_app.game.final_mass - 325.0f) < 0.001f);
  IM_CHECK_EQ(g_imgui_test_app.game.final_spores_collected, 17u);
  IM_CHECK_EQ(g_imgui_test_app.game.final_kills, 3u);
  IM_CHECK_STR_EQ(ShroomTestGetResultsSporesText(&g_imgui_test_app.game), "Spores Collected: 17");
  IM_CHECK_STR_EQ(ShroomTestGetResultsKillsText(&g_imgui_test_app.game), "Players Consumed: 3");
  IM_CHECK(fabsf(g_imgui_test_app.game.peak_mass - 325.0f) < 0.001f);

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESET, 101u, local_position, 300.0f, opponent_position,
                         200.0f);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK_EQ(g_imgui_test_app.game.net.snapshot_players[0].round_spores, 0u);
  IM_CHECK_EQ(g_imgui_test_app.game.net.snapshot_players[0].round_kills, 0u);
  IM_CHECK_EQ(g_imgui_test_app.game.final_spores_collected, 17u);
  IM_CHECK_EQ(g_imgui_test_app.game.final_kills, 3u);
  IM_CHECK_STR_EQ(ShroomTestGetResultsSporesText(&g_imgui_test_app.game), "Spores Collected: 17");
  IM_CHECK_STR_EQ(ShroomTestGetResultsKillsText(&g_imgui_test_app.game), "Players Consumed: 3");

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 201u, local_position,
                         SHROOM_DEFAULT_PLAYER_MASS, opponent_position, SHROOM_DEFAULT_PLAYER_MASS);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK(g_imgui_test_app.game.net.welcome_received);
  IM_CHECK(g_imgui_test_app.game.net.match_entry_sent);
  IM_CHECK(!g_imgui_test_app.game.show_results);
  IM_CHECK(fabsf(g_imgui_test_app.game.peak_mass - SHROOM_DEFAULT_PLAYER_MASS) < 0.001f);

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 201u, local_position, 250.0f,
                         opponent_position, 350.0f);
  g_imgui_test_app.game.net.snapshot_players[0].round_spores = 4u;
  g_imgui_test_app.game.net.snapshot_players[0].round_kills = 1u;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  IM_CHECK_EQ(g_imgui_test_app.game.final_rank, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.final_spores_collected, 4u);
  IM_CHECK_EQ(g_imgui_test_app.game.final_kills, 1u);
  IM_CHECK_STR_EQ(ShroomTestGetResultsSporesText(&g_imgui_test_app.game), "Spores Collected: 4");
  IM_CHECK_STR_EQ(ShroomTestGetResultsKillsText(&g_imgui_test_app.game), "Players Consumed: 1");

  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESET, 201u, local_position, 250.0f, opponent_position,
                         350.0f);
  ShroomTeCtx_Yield(ctx, 1);
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RUNNING, 301u, local_position,
                         SHROOM_DEFAULT_PLAYER_MASS, opponent_position, SHROOM_DEFAULT_PLAYER_MASS);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);

  /* A spectator joining during intermission follows the first phase snapshot too. */
  SetupOnlineGame();
  g_imgui_test_app.game.net.spectating = true;
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 0u, local_position, 300.0f, opponent_position,
                         200.0f);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
}

static void Test_AuthoritativeIntermissionMultiClientState(ImGuiTestContext* ctx) {
  const ShroomVec2 local_position = {500.0f, 600.0f};
  const ShroomVec2 opponent_position = {900.0f, 1000.0f};
  ShroomIntermissionStatusPacket* status;
  char state_text[128];

  SetupOnlineGame();
  status = &g_imgui_test_app.game.net.intermission;
  g_imgui_test_app.game.net.intermission_received = true;
  *status = (ShroomIntermissionStatusPacket){.round_id = 12u,
                                             .seconds_remaining = 18.0f,
                                             .eligible_count = 3u,
                                             .play_again_votes = 2u,
                                             .return_to_lobby_votes = 1u,
                                             .your_vote = SHROOM_REMATCH_VOTE_NONE,
                                             .can_vote = 1u};
  InjectFeedbackSnapshot(SHROOM_MATCH_PHASE_RESULTS, 101u, local_position, 300.0f,
                         opponent_position, 200.0f);
  ShroomTeCtx_SetRef(ctx, "Match Results");
  ShroomTeCtx_Yield(ctx, 2);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  ShroomResultsFormatLocalVote((ShroomRematchVote)status->your_vote, status->can_vote != 0u,
                               state_text, sizeof(state_text));
  IM_CHECK_STR_EQ(state_text, "Your vote: Not cast");
  ShroomResultsFormatVoteParticipation(status->play_again_votes, status->return_to_lobby_votes,
                                       status->spectate_votes, status->eligible_count, state_text,
                                       sizeof(state_text));
  IM_CHECK_STR_EQ(state_text, "Participation: 3 / 3 eligible");
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Play Again"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Lobby"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Spectate"));

  /* Server acknowledgements persist on the selected action and replace an earlier vote. */
  status->your_vote = SHROOM_REMATCH_VOTE_PLAY_AGAIN;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Play Again (Selected)"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Lobby"));

  status->your_vote = SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Play Again"));
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Lobby (Selected)"));

  status->your_vote = SHROOM_REMATCH_VOTE_SPECTATE;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Vote Spectate (Selected)"));

  /* A spectator/late joiner receives the same totals but no voting controls. */
  status->can_vote = 0u;
  status->your_vote = SHROOM_REMATCH_VOTE_NONE;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.net.intermission.can_vote, 0u);
  ShroomResultsFormatLocalVote((ShroomRematchVote)status->your_vote, status->can_vote != 0u,
                               state_text, sizeof(state_text));
  IM_CHECK_STR_EQ(state_text, "Your vote: Not eligible");

  /* A resolved play-again vote remains visible until the next running snapshot arrives. */
  status->can_vote = 1u;
  status->resolved = 1u;
  status->decision = SHROOM_REMATCH_VOTE_PLAY_AGAIN;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_RESULTS);
  ShroomResultsFormatDecision((ShroomRematchVote)status->decision, state_text, sizeof(state_text));
  IM_CHECK_STR_EQ(state_text, "Decision: Play Again");
  IM_CHECK(!ShroomTeCtx_ItemExists(ctx, "Vote Play Again"));

  /* The reliable final decision returns every observing client to the lobby. */
  status->decision = SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY;
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY_ROSTER);
  IM_CHECK(g_imgui_test_app.game.net.welcome_received);
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

/* chat: Reconnecting restores history without duplicating messages or unread state. */
static void Test_ChatHistoryRestoresOnReconnect(ImGuiTestContext* ctx) {
  ClientNetState* net;
  ShroomChatCacheKey key = {.port = SHROOM_SERVER_PORT, .lobby_id = 42u};
  ChatMessage cached = {.sender_id = 7u, .timestamp_sec = (uint32_t)time(NULL)};
  ShroomLobbyJoinedPacket joined = {0};
  ShroomChatPacket chat = {0};
  ENetPacket joined_packet = {.data = (enet_uint8*)&joined, .dataLength = sizeof(joined)};
  ENetPacket chat_packet = {.data = (enet_uint8*)&chat, .dataLength = sizeof(chat)};

  SetupOnlineGame();
  net = &g_imgui_test_app.game.net;
  snprintf(net->server_host, sizeof(net->server_host), "%s", "cache.example");
  net->server_port = SHROOM_SERVER_PORT;
  snprintf(net->chat_cache_path, sizeof(net->chat_cache_path), "%s",
           SHROOM_CHAT_CACHE_DEFAULT_PATH);
  snprintf(key.host, sizeof(key.host), "%s", net->server_host);
  snprintf(cached.sender_name, sizeof(cached.sender_name), "%s", "Player Seven");
  snprintf(cached.message, sizeof(cached.message), "%s", "still here after reconnect");
  IM_CHECK(ShroomChatCacheStoreMessage(net->chat_cache_path, &key, &cached, cached.timestamp_sec));

  joined.lobby_id = key.lobby_id;
  joined.player_id = 1u;
  joined.entity_id = 1u;
  snprintf(joined.lobby_name, sizeof(joined.lobby_name), "%s", "Cache Lobby");
  ClientNetTestHandleLobbyJoined(net, &joined_packet);
  IM_CHECK_EQ(net->chat_history_count, 1u);
  IM_CHECK_EQ(net->chat_unread_count, 0u);

  chat.sender_id = cached.sender_id;
  snprintf(chat.sender_name, sizeof(chat.sender_name), "%s", cached.sender_name);
  snprintf(chat.message, sizeof(chat.message), "%s", cached.message);
  ClientNetTestHandleChat(net, &chat_packet);
  IM_CHECK_EQ(net->chat_history_count, 1u);
  IM_CHECK_EQ(net->chat_unread_count, 0u);

  snprintf(chat.message, sizeof(chat.message), "%s", "a new live message");
  ClientNetTestHandleChat(net, &chat_packet);
  IM_CHECK_EQ(net->chat_history_count, 2u);
  IM_CHECK_EQ(net->chat_unread_count, 1u);

  ClientNetTestHandleLobbyJoined(net, &joined_packet);
  g_imgui_test_app.game.chat_open = true;
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(net->chat_history_count, 2u);
  IM_CHECK_EQ(net->chat_unread_count, 0u);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Chat"));
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
  IM_CHECK_EQ(ShroomLobbyRosterCapacity(&g_imgui_test_app.game), SHROOM_MAX_PLAYABLE_PARTICIPANTS);
}

static void Test_FirstLobbyEntryDoesNotOpenDeathCutscene(ImGuiTestContext* ctx) {
  SetupLobbyBrowser();
  ClientNetInit(&g_imgui_test_app.game.net, "127.0.0.1", 37779u,
                g_imgui_test_app.game.settings.player_name);
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

static void Test_LateIntermissionJoinerWaitsForExplicitMatchEntry(ImGuiTestContext* ctx) {
  ClientNetState* net;
  ShroomLobbyRosterPacket roster = {0};
  ShroomIntermissionStatusPacket intermission = {0};
  ENetPacket roster_packet = {
      .data = (enet_uint8*)&roster,
      .dataLength = offsetof(ShroomLobbyRosterPacket, players) +
                    (2u * sizeof(ShroomLobbyRosterEntry)),
  };
  ENetPacket intermission_packet = {
      .data = (enet_uint8*)&intermission,
      .dataLength = sizeof(intermission),
  };

  SetupLobbyBrowser();
  net = &g_imgui_test_app.game.net;
  IM_CHECK(ClientNetInit(net, "127.0.0.1", 37779u,
                         g_imgui_test_app.game.settings.player_name));
  net->status = CLIENT_NET_CONNECTED;
  net->welcome_received = true;
  net->player_id = 7u;
  net->entity_id = 42u;
  net->lobby_id = 1u;
  net->world_width = SHROOM_WORLD_WIDTH;
  net->world_height = SHROOM_WORLD_HEIGHT;

  intermission.round_id = 5u;
  intermission.resolved = 1u;
  intermission.decision = SHROOM_REMATCH_VOTE_PLAY_AGAIN;
  intermission.can_vote = 0u;
  ClientNetTestHandleIntermissionStatus(net, &intermission_packet);

  roster.lobby_id = net->lobby_id;
  roster.player_count = 2u;
  roster.match_started = 1u;
  roster.players[0] =
      (ShroomLobbyRosterEntry){.player_id = 3u, .is_ready = 0u, .entered_match = 1u};
  roster.players[1] =
      (ShroomLobbyRosterEntry){.player_id = net->player_id, .is_ready = 0u, .entered_match = 0u};
  ClientNetTestHandleLobbyRoster(net, &roster_packet);

  net->match_phase = SHROOM_MATCH_PHASE_RUNNING;
  net->last_snapshot_tick = 1u;
  net->snapshot_player_count = 2u;
  net->snapshot_players[0] = (ShroomSnapshotPlayerState){
      .player_id = net->player_id,
      .entity_id = net->entity_id,
      .position_x = 600.0f,
      .position_y = 700.0f,
      .mass = SHROOM_DEFAULT_PLAYER_MASS,
      .radius = ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS),
      .alive = 1u,
  };
  net->snapshot_players[1] = (ShroomSnapshotPlayerState){
      .player_id = 3u,
      .entity_id = 30u,
      .position_x = 640.0f,
      .position_y = 700.0f,
      .mass = SHROOM_DEFAULT_PLAYER_MASS * 4.0f,
      .radius = ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS * 4.0f),
      .alive = 1u,
  };

  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY_ROSTER);
  ShroomTeCtx_Yield(ctx, 5);
  IM_CHECK(ShroomTeCtx_SetRefWindow(ctx, "//Lobby Roster"));

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY_ROSTER);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Roster"));
  IM_CHECK(net->intermission_received);
  IM_CHECK_EQ(net->intermission.can_vote, 0u);
  IM_CHECK_EQ(net->lobby_roster[1].entered_match, 0u);
  IM_CHECK_EQ(net->match_entry_sent, false);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Ready to enter"));
  IM_CHECK(!ShroomTeCtx_ItemExists(ctx, "Enter Match"));
  IM_CHECK(!ShroomTeImGui_WindowIsActive("Death Cutscene Actions"));

  roster.players[1].is_ready = 1u;
  ClientNetTestHandleLobbyRoster(net, &roster_packet);
  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY_ROSTER);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Roster"));
  IM_CHECK_EQ(net->lobby_roster[1].is_ready, 1u);
  IM_CHECK(ShroomTeCtx_ItemExists(ctx, "Enter Match"));
  ShroomTeCtx_ItemClick(ctx, "Enter Match");
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_GAME);
  IM_CHECK(net->match_entry_sent);
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
  ShroomTeEngine_RegisterTest(engine, "screens", "help_content_matches_current_gameplay",
                              Test_HelpContentMatchesCurrentGameplay);
  ShroomTeEngine_RegisterTest(engine, "screens", "help_card_headings_are_not_interactive",
                              Test_HelpCardHeadingsAreNotInteractive);
  ShroomTeEngine_RegisterTest(engine, "screens", "main_menu_exposes_primary_actions",
                              Test_MainMenuExposesPrimaryActions);
  ShroomTeEngine_RegisterTest(engine, "screens", "player_identity_onboarding_persists_session",
                              Test_PlayerIdentityOnboardingPersistsAndStartsSession);
  ShroomTeEngine_RegisterTest(engine, "screens", "player_name_inputs_stay_visible_at_ui_scales",
                              Test_PlayerNameInputsStayVisibleAtUiScaleEndpoints);
  ShroomTeEngine_RegisterTest(engine, "screens", "game_mode_availability_and_navigation",
                              Test_GameModeAvailabilityAndNavigation);
  ShroomTeEngine_RegisterTest(engine, "screens", "offline_practice_entry_initializes_game",
                              Test_OfflinePracticeEntryInitializesGame);
  ShroomTeEngine_RegisterTest(engine, "screens", "offline_practice_bots_tactical_split",
                              Test_OfflinePracticeBotsExerciseTacticalSplit);
  ShroomTeEngine_RegisterTest(engine, "screens",
                              "offline_practice_persisted_settings_camera_stays_stable",
                              Test_OfflinePracticePersistedSettingsCameraStaysStable);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_exposes_controls_and_applies_bounds",
                              Test_SettingsExposesSpecControlsAndAppliesBoundaryValues);
  ShroomTeEngine_RegisterTest(engine, "screens", "audio_lifecycle_soak_and_restart",
                              Test_AudioSurvivesMatchTransitionsAndRestartsFromSettings);
  ShroomTeEngine_RegisterTest(engine, "screens", "audio_multi_round_gameplay_cycle",
                              Test_AudioSurvivesMultiRoundGameplayCycle);
  ShroomTeEngine_RegisterTest(engine, "screens", "audio_updates_on_non_gameplay_screens",
                              Test_AudioUpdatesOnNonGameplayScreens);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_persistence", Test_SettingsPersistence);
  ShroomTeEngine_RegisterTest(engine, "screens", "migrated_settings_cross_screen_workflow",
                              Test_MigratedSettingsSurviveCrossScreenWorkflow);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_reserved_key_rejection_and_recovery",
                              Test_SettingsReservedKeyRejectionAndRecovery);
  ShroomTeEngine_RegisterTest(engine, "screens", "ui_scale_endpoints_keep_windows_usable",
                              Test_UiScaleEndpointsKeepPrimaryWindowsUsable);
  ShroomTeEngine_RegisterTest(engine, "lobby", "roster_scroll_keeps_actions_usable",
                              Test_LobbyRosterScrollKeepsActionsUsable);
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
  ShroomTeEngine_RegisterTest(engine, "screens", "leaderboard_aggregates_split_colony",
                              Test_LeaderboardAggregatesSplitColony);
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
  ShroomTeEngine_RegisterTest(engine, "screens", "match_reset_rebaselines_feedback",
                              Test_MatchResetRebaselinesFeedback);
  ShroomTeEngine_RegisterTest(engine, "screens", "online_prediction_moves_and_reconciles",
                              Test_OnlinePredictionMovesImmediatelyAndReconciles);
  ShroomTeEngine_RegisterTest(engine, "screens", "authoritative_results_two_round_cycle",
                              Test_AuthoritativeResultsCompleteTwoRoundCycle);
  ShroomTeEngine_RegisterTest(engine, "screens", "king_of_hill_complete_match_hud_and_results",
                              Test_KingOfHillCompleteMatchHudAndResults);
  ShroomTeEngine_RegisterTest(engine, "screens", "authoritative_results_rank_koth_bots",
                              Test_AuthoritativeResultsRankBotsByKingOfHillScore);
  ShroomTeEngine_RegisterTest(engine, "screens", "king_of_hill_mode_selection",
                              Test_KingOfHillModeSelection);
  ShroomTeEngine_RegisterTest(engine, "screens", "authoritative_intermission_multi_client_state",
                              Test_AuthoritativeIntermissionMultiClientState);
  ShroomTeEngine_RegisterTest(engine, "chat", "dock_visible_in_online_mode",
                              Test_ChatDockVisibleOnline);
  ShroomTeEngine_RegisterTest(engine, "chat", "dock_hidden_in_offline_mode",
                              Test_ChatDockHiddenOffline);
  ShroomTeEngine_RegisterTest(engine, "chat", "history_renders_incoming_messages",
                              Test_ChatHistoryRendersIncoming);
  ShroomTeEngine_RegisterTest(engine, "chat", "unread_count_increments_on_receive",
                              Test_ChatUnreadCountIncrements);
  ShroomTeEngine_RegisterTest(engine, "chat", "history_restores_on_reconnect",
                              Test_ChatHistoryRestoresOnReconnect);
  ShroomTeEngine_RegisterTest(engine, "lobby", "screen_renders", Test_LobbyScreenRenders);
  ShroomTeEngine_RegisterTest(engine, "lobby", "lobby_list_renders_entries",
                              Test_LobbyListRendersEntries);
  ShroomTeEngine_RegisterTest(engine, "lobby", "auto_join_picks_least_populated",
                              Test_LobbyAutoJoinPicksLeastPopulated);
  ShroomTeEngine_RegisterTest(engine, "lobby", "auto_join_transitions_to_roster",
                              Test_LobbyAutoJoinTransitionsToRoster);
  ShroomTeEngine_RegisterTest(engine, "lobby", "first_entry_has_no_death_cutscene",
                              Test_FirstLobbyEntryDoesNotOpenDeathCutscene);
  ShroomTeEngine_RegisterTest(engine, "lobby", "late_intermission_joiner_explicit_entry",
                              Test_LateIntermissionJoinerWaitsForExplicitMatchEntry);
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
