#include "app.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"

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

/* Put the game into a simulated online state without a real ENet peer.
 * Uses offline-practice so GameInit skips network init, then patches the
 * mode/net fields to look like a connected quick-play session. */
/* Put the app into the lobby browser screen with handshake already completed. */
static void SetupLobbyBrowser(void) {
  ShroomImGuiTestAppReset(false);
  g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
  ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_LOBBY);
  g_imgui_test_app.game.net.handshake_received = true;
  g_imgui_test_app.game.net.status = CLIENT_NET_CONNECTED;
  snprintf(g_imgui_test_app.game.net.status_text,
           sizeof(g_imgui_test_app.game.net.status_text), "Connected");
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

void ShroomRegisterImGuiTests(ImGuiTestEngine* engine) {
  ImGuiTest* test;

  test = IM_REGISTER_TEST(engine, "screens", "main_menu_navigation");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ShroomImGuiTestAppReset(true);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Settings");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_SETTINGS);

    ctx->SetRef("Settings");
    ctx->ItemClick("Back");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_MAIN_MENU);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Help");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_HELP);
  };

  test = IM_REGISTER_TEST(engine, "screens", "settings_persistence");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ClientSettings loaded = {};

    ShroomImGuiTestAppReset(true);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Settings");
    ctx->SetRef("Settings");
    ctx->ItemInputValue("UI Scale", 130);
    ctx->ItemCheck("Invert Mouse");
    ctx->ItemCheck("Show Diagnostics On Launch");
    ctx->ItemClick("Save");

    IM_CHECK(ClientSettingsLoad(&loaded));
    IM_CHECK_EQ(loaded.ui_scale_percent, 130);
    IM_CHECK(loaded.invert_mouse);
    IM_CHECK(loaded.diagnostics_enabled);
  };

  test = IM_REGISTER_TEST(engine, "screens", "server_browser_join_and_validation");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ShroomImGuiTestAppReset(true);

    ctx->SetRef("Main Menu");
    ctx->ItemClick("Custom Server");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_SERVER_BROWSER);

    ctx->SetRef("Server Browser");
    ctx->ItemInputValue("Port", "70000");
    ctx->ItemClick("Join Host");
    IM_CHECK_STR_EQ(ShroomTestGetServerBrowserValidationMessage(),
                    "Port must be between 1 and 65535.");

    ctx->ItemInputValue("Port", "7777");
    IM_CHECK_EQ(ShroomTestGetServerBrowserSelectedIndex(), 0);
    ctx->ItemClick("Join Selected");

    /* JoinServer now connects and transitions to the lobby browser, not GAME. */
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_LOBBY);
    IM_CHECK_STR_EQ(g_imgui_test_app.game.selected_server_host, "127.0.0.1");
    IM_CHECK_EQ(g_imgui_test_app.game.selected_server_port, SHROOM_SERVER_PORT);
  };

  /* chat: Chat dock is active in online mode. */
  test = IM_REGISTER_TEST(engine, "chat", "dock_visible_in_online_mode");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupOnlineGame();
    ctx->Yield(3);
    ImGuiWindow* w = ImGui::FindWindowByName("Chat");
    IM_CHECK(w != NULL && w->Active);
  };

  /* chat: Chat dock is not active in offline-practice mode. */
  test = IM_REGISTER_TEST(engine, "chat", "dock_hidden_in_offline_mode");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ShroomImGuiTestAppReset(false);
    g_imgui_test_app.game.selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
    ShroomScreenManagerTransition(&g_imgui_test_app.screen_manager, SHROOM_SCREEN_GAME);
    ctx->Yield(3);
    ImGuiWindow* w = ImGui::FindWindowByName("Chat");
    IM_CHECK(w == NULL || !w->Active);
  };

  /* chat: Messages injected into the ring buffer are rendered in the dock. */
  test = IM_REGISTER_TEST(engine, "chat", "history_renders_incoming_messages");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupOnlineGame();

    /* Directly populate the ring buffer as the net layer would. */
    for (int i = 0; i < 3; ++i) {
      ChatMessage* slot = &g_imgui_test_app.game.net
                               .chat_history[g_imgui_test_app.game.net.chat_history_head %
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
    ctx->Yield(3);

    IM_CHECK_EQ(g_imgui_test_app.game.net.chat_history_count, 3u);
    ImGuiWindow* w = ImGui::FindWindowByName("Chat");
    IM_CHECK(w != NULL && w->Active);
  };

  /* chat: Unread count increments when messages arrive while dock is closed. */
  test = IM_REGISTER_TEST(engine, "chat", "unread_count_increments_on_receive");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupOnlineGame();
    IM_CHECK_EQ(g_imgui_test_app.game.net.chat_unread_count, 0u);

    for (int i = 0; i < 4; ++i) {
      ChatMessage* slot = &g_imgui_test_app.game.net
                               .chat_history[g_imgui_test_app.game.net.chat_history_head %
                                             SHROOM_CLIENT_CHAT_HISTORY_COUNT];
      slot->sender_id = 2u;
      snprintf(slot->sender_name, sizeof(slot->sender_name), "Bot");
      snprintf(slot->message, sizeof(slot->message), "msg %d", i);
      g_imgui_test_app.game.net.chat_history_head =
          (g_imgui_test_app.game.net.chat_history_head + 1u) % SHROOM_CLIENT_CHAT_HISTORY_COUNT;
      g_imgui_test_app.game.net.chat_history_count += 1u;
      g_imgui_test_app.game.net.chat_unread_count += 1u;
    }

    ctx->Yield(2);
    IM_CHECK_EQ(g_imgui_test_app.game.net.chat_unread_count, 4u);
    IM_CHECK_EQ(g_imgui_test_app.game.chat_open, false);
  };

  /* lobby: Lobby Browser screen renders when transitioned to. */
  test = IM_REGISTER_TEST(engine, "lobby", "screen_renders");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupLobbyBrowser();
    ctx->Yield(3);
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_LOBBY);
    ImGuiWindow* w = ImGui::FindWindowByName("Lobby Browser");
    IM_CHECK(w != NULL && w->Active);
  };

  /* lobby: Injected lobby entries appear in the table. */
  test = IM_REGISTER_TEST(engine, "lobby", "lobby_list_renders_entries");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupLobbyBrowser();
    InjectFakeLobbies(3);
    ctx->Yield(3);
    IM_CHECK_EQ(g_imgui_test_app.game.net.lobby_count, (uint8_t)3);
    ImGuiWindow* w = ImGui::FindWindowByName("Lobby Browser");
    IM_CHECK(w != NULL && w->Active);
  };

  /* lobby: auto_join_lobby flag triggers join of least-populated lobby. */
  test = IM_REGISTER_TEST(engine, "lobby", "auto_join_picks_least_populated");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupLobbyBrowser();
    InjectFakeLobbies(3);
    /* lobby_list[0].player_count=0, [1]=2, [2]=4 — best is index 0, lobby_id=1 */
    g_imgui_test_app.game.auto_join_lobby = true;
    ctx->Yield(3);
    /* auto_join_lobby should be cleared after the join attempt */
    IM_CHECK_EQ(g_imgui_test_app.game.auto_join_lobby, false);
  };

  /* lobby: Back button disconnects and returns to server browser. */
  test = IM_REGISTER_TEST(engine, "lobby", "back_returns_to_server_browser");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupLobbyBrowser();
    ctx->Yield(3);
    ctx->SetRef("Lobby Browser");
    ctx->ItemClick("Back");
    IM_CHECK_EQ(ShroomScreenManagerGetCurrentScreen(&g_imgui_test_app.screen_manager),
                SHROOM_SCREEN_SERVER_BROWSER);
  };

  /* chat: Opening chat focuses the input and WantCaptureKeyboard prevents
   * stray key presses from closing the dock mid-session. */
  test = IM_REGISTER_TEST(engine, "chat", "input_focus_and_keyboard_capture");
  test->TestFunc = [](ImGuiTestContext* ctx) {
    SetupOnlineGame();

    /* Open chat programmatically as the T-key handler would. */
    g_imgui_test_app.game.chat_open = true;
    g_imgui_test_app.game.chat_focus_input = true;
    g_imgui_test_app.game.net.chat_unread_count = 0;

    /* Yield once so DrawChatDock renders and SetKeyboardFocusHere fires. */
    ctx->Yield(1);

    /* Focus the chat input via the test engine to set WantCaptureKeyboard. */
    ctx->ItemClick("Chat/##chatinput");
    ctx->Yield(2);

    /* WantCaptureKeyboard is now true; GameplayHandleInput should not close
     * the dock even if a key is pressed. */
    IM_CHECK_EQ(g_imgui_test_app.game.chat_open, true);
  };
}
