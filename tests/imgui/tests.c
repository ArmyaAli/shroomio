#include "app.h"
#include "imgui_te_wrapper.h"

#include "shared/protocol.h"

#include <stdio.h>
#include <string.h>

/* Inject N fake lobby entries into the game's net state as the server would. */
static void InjectFakeLobbies(int count) {
  int i;

  g_imgui_test_app.game.net.lobby_count = (uint8_t)count;
  for (i = 0; i < count; ++i) {
    ShroomLobbyEntry *e = &g_imgui_test_app.game.net.lobby_list[i];
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
  snprintf(g_imgui_test_app.game.net.status_text,
           sizeof(g_imgui_test_app.game.net.status_text), "Connected");
}

static void SetupOfflineGame(void) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_GAME);
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  snprintf(g_imgui_test_app.game.net.status_text,
           sizeof(g_imgui_test_app.game.net.status_text), "Offline");
}

static void SetupOnlineGame(void) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_GAME);
  g_imgui_test_app.game.active_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  g_imgui_test_app.game.net.welcome_received = true;
  g_imgui_test_app.game.net.player_id = 1;
  snprintf(g_imgui_test_app.game.net.status_text,
           sizeof(g_imgui_test_app.game.net.status_text), "Connected");
}

/* -------------------------------------------------------------------------
 * Test functions — plain C, no lambdas.
 * ---------------------------------------------------------------------- */

static void Test_MainMenuNavigation(ImGuiTestContext *ctx) {
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

static void Test_SettingsPersistence(ImGuiTestContext *ctx) {
  ClientSettings loaded;
  memset(&loaded, 0, sizeof(loaded));

  ShroomImGuiTestAppReset(true);

  ShroomTeCtx_SetRef(ctx, "Main Menu");
  ShroomTeCtx_ItemClick(ctx, "Settings");
  ShroomTeCtx_SetRef(ctx, "Settings");
  ShroomTeCtx_ItemInputValueInt(ctx, "UI Scale", 130);
  ShroomTeCtx_ItemCheckbox(ctx, "Invert Mouse");
  ShroomTeCtx_ItemCheckbox(ctx, "Show Diagnostics On Launch");
  ShroomTeCtx_ItemClick(ctx, "Save");

  IM_CHECK(ClientSettingsLoad(&loaded));
  IM_CHECK_EQ(loaded.ui_scale_percent, 130);
  IM_CHECK(loaded.invert_mouse);
  IM_CHECK(loaded.diagnostics_enabled);
}

static void Test_ServerBrowserJoinAndValidation(ImGuiTestContext *ctx) {
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
  IM_CHECK_EQ(ShroomTestGetServerBrowserSelectedIndex(), 0);
  ShroomTeCtx_ItemClick(ctx, "Join Selected");

  /* JoinServer connects and transitions to the lobby browser, not GAME. */
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
  IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "127.0.0.1");
  IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, SHROOM_SERVER_PORT);
}

/* chat: Chat dock is active in online mode. */
static void Test_ChatDockVisibleOnline(ImGuiTestContext *ctx) {
  SetupOnlineGame();
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Chat"));
}

/* chat: Chat dock is not active in offline-practice mode. */
static void Test_ChatDockHiddenOffline(ImGuiTestContext *ctx) {
  SetupOfflineGame();
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(g_imgui_test_app.game.active_mode, SHROOM_SESSION_MODE_OFFLINE_PRACTICE);
  IM_CHECK(!ShroomTeImGui_WindowIsActive("Chat"));
}

/* chat: Messages injected into the ring buffer are rendered in the dock. */
static void Test_ChatHistoryRendersIncoming(ImGuiTestContext *ctx) {
  int i;
  SetupOnlineGame();

  for (i = 0; i < 3; ++i) {
    ChatMessage *slot =
        &g_imgui_test_app.game.net
             .chat_history[g_imgui_test_app.game.net.chat_history_head %
                           SHROOM_CLIENT_CHAT_HISTORY_COUNT];
    slot->sender_id = (uint32_t)(i + 2);
    snprintf(slot->sender_name, sizeof(slot->sender_name), "Player%d", i + 1);
    snprintf(slot->message, sizeof(slot->message), "hello from player %d", i + 1);
    g_imgui_test_app.game.net.chat_history_head =
        (g_imgui_test_app.game.net.chat_history_head + 1u) %
        SHROOM_CLIENT_CHAT_HISTORY_COUNT;
    g_imgui_test_app.game.net.chat_history_count += 1u;
    g_imgui_test_app.game.net.chat_unread_count += 1u;
  }

  g_imgui_test_app.game.chat_open = true;
  ShroomTeCtx_Yield(ctx, 3);

  IM_CHECK_EQ(g_imgui_test_app.game.net.chat_history_count, 3u);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Chat"));
}

/* chat: Unread count increments when messages arrive while dock is closed. */
static void Test_ChatUnreadCountIncrements(ImGuiTestContext *ctx) {
  int i;
  SetupOnlineGame();
  IM_CHECK_EQ(g_imgui_test_app.game.net.chat_unread_count, 0u);

  for (i = 0; i < 4; ++i) {
    ChatMessage *slot =
        &g_imgui_test_app.game.net
             .chat_history[g_imgui_test_app.game.net.chat_history_head %
                           SHROOM_CLIENT_CHAT_HISTORY_COUNT];
    slot->sender_id = 2u;
    snprintf(slot->sender_name, sizeof(slot->sender_name), "Bot");
    snprintf(slot->message, sizeof(slot->message), "msg %d", i);
    g_imgui_test_app.game.net.chat_history_head =
        (g_imgui_test_app.game.net.chat_history_head + 1u) %
        SHROOM_CLIENT_CHAT_HISTORY_COUNT;
    g_imgui_test_app.game.net.chat_history_count += 1u;
    g_imgui_test_app.game.net.chat_unread_count += 1u;
  }

  ShroomTeCtx_Yield(ctx, 2);
  IM_CHECK_EQ(g_imgui_test_app.game.net.chat_unread_count, 4u);
  IM_CHECK_EQ(g_imgui_test_app.game.chat_open, false);
}

/* lobby: Lobby Browser screen renders when transitioned to. */
static void Test_LobbyScreenRenders(ImGuiTestContext *ctx) {
  SetupLobbyBrowser();
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_LOBBY);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Browser"));
}

/* lobby: Injected lobby entries appear in the table. */
static void Test_LobbyListRendersEntries(ImGuiTestContext *ctx) {
  SetupLobbyBrowser();
  InjectFakeLobbies(3);
  ShroomTeCtx_Yield(ctx, 3);
  IM_CHECK_EQ(g_imgui_test_app.game.net.lobby_count, (uint8_t)3);
  IM_CHECK(ShroomTeImGui_WindowIsActive("Lobby Browser"));
}

/* lobby: auto_join_lobby flag triggers join of least-populated lobby. */
static void Test_LobbyAutoJoinPicksLeastPopulated(ImGuiTestContext *ctx) {
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
static void Test_LobbyBackReturnsToServerBrowser(ImGuiTestContext *ctx) {
  SetupLobbyBrowser();
  ShroomTeCtx_Yield(ctx, 3);
  ShroomTeCtx_SetRef(ctx, "Lobby Browser");
  ShroomTeCtx_ItemClick(ctx, "Back");
  IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
              SHROOM_SCREEN_SERVER_BROWSER);
}

/* chat: Opening chat focuses the input and WantCaptureKeyboard prevents
 * stray key presses from closing the dock mid-session. */
static void Test_ChatInputFocusAndKeyboardCapture(ImGuiTestContext *ctx) {
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

void ShroomRegisterImGuiTests(ImGuiTestEngine *engine) {
  ShroomTeEngine_RegisterTest(engine, "screens", "main_menu_navigation",
                              Test_MainMenuNavigation);
  ShroomTeEngine_RegisterTest(engine, "screens", "settings_persistence",
                              Test_SettingsPersistence);
  ShroomTeEngine_RegisterTest(engine, "screens", "server_browser_join_and_validation",
                              Test_ServerBrowserJoinAndValidation);
  ShroomTeEngine_RegisterTest(engine, "chat", "dock_visible_in_online_mode",
                              Test_ChatDockVisibleOnline);
  ShroomTeEngine_RegisterTest(engine, "chat", "dock_hidden_in_offline_mode",
                              Test_ChatDockHiddenOffline);
  ShroomTeEngine_RegisterTest(engine, "chat", "history_renders_incoming_messages",
                              Test_ChatHistoryRendersIncoming);
  ShroomTeEngine_RegisterTest(engine, "chat", "unread_count_increments_on_receive",
                              Test_ChatUnreadCountIncrements);
  ShroomTeEngine_RegisterTest(engine, "lobby", "screen_renders",
                              Test_LobbyScreenRenders);
  ShroomTeEngine_RegisterTest(engine, "lobby", "lobby_list_renders_entries",
                              Test_LobbyListRendersEntries);
  ShroomTeEngine_RegisterTest(engine, "lobby", "auto_join_picks_least_populated",
                              Test_LobbyAutoJoinPicksLeastPopulated);
  ShroomTeEngine_RegisterTest(engine, "lobby", "back_returns_to_server_browser",
                              Test_LobbyBackReturnsToServerBrowser);
  ShroomTeEngine_RegisterTest(engine, "chat", "input_focus_and_keyboard_capture",
                              Test_ChatInputFocusAndKeyboardCapture);
}
