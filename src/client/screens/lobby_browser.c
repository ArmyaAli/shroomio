#include "client/game.h"
#include "client/imgui_wrapper.h"
#include "client/net.h"
#include "client/screen.h"
#include "client/screens/screen_background.h"

#include "raylib.h"

#define LOBBY_REFRESH_INTERVAL 5.0f

static float g_refresh_timer = 0.0f;
static bool g_handshake_seen = false;
static char g_create_name_buf[SHROOM_LOBBY_MAX_NAME_LENGTH] = {0};

static const char* LobbyStatusLabel(const ShroomLobbyEntry* entry) {
  if (entry->player_count >= entry->max_players) {
    return "Full";
  }
  return "Open";
}

static bool LobbyBrowserInit(ShroomScreenManager* manager) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }

  g_refresh_timer = 0.0f;
  g_handshake_seen = false;
  g_create_name_buf[0] = '\0';
  /* Do NOT send the query here — the ENet connection may not be established
   * yet. The update loop fires it once handshake_received flips to true. */
  return true;
}

static void LobbyBrowserUpdate(ShroomScreenManager* manager, float delta_time) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const ShroomVec2 no_input = {0};

  if (game == NULL) {
    return;
  }

  /* Pump ENet so WELCOME, LOBBY_LIST, and LOBBY_JOINED are received. */
  ClientNetUpdate(&game->net, no_input, false, no_input, 0, delta_time);

  /* Don't transition away on error/disconnect - let the modal handle it. */

  /* Fire the initial lobby list query the first frame after the handshake. */
  if (!g_handshake_seen && game->net.handshake_received) {
    g_handshake_seen = true;
    g_refresh_timer = 0.0f;
    ClientNetSendLobbyListQuery(&game->net);
  }

  /* Auto-refresh (manual browser only — skip while auto_join is pending). */
  if (g_handshake_seen && !game->auto_join_lobby) {
    g_refresh_timer += delta_time;
    if (g_refresh_timer >= LOBBY_REFRESH_INTERVAL) {
      g_refresh_timer = 0.0f;
      ClientNetSendLobbyListQuery(&game->net);
    }
  }

  /* Auto-join: pick the least-populated available lobby on first list. */
  if (game->auto_join_lobby && game->net.lobby_count > 0) {
    uint8_t best = 0;
    uint8_t i;

    for (i = 1; i < game->net.lobby_count; ++i) {
      if (game->net.lobby_list[i].player_count < game->net.lobby_list[best].player_count) {
        best = i;
      }
    }
    game->auto_join_lobby = false;
    if (game->net.lobby_list[best].player_count < game->net.lobby_list[best].max_players) {
      ClientNetSendLobbyJoin(&game->net, game->net.lobby_list[best].lobby_id, false);
    }
  }

  /* Joined — stage at the lobby roster panel before gameplay. */
  if (game->net.welcome_received || game->net.spectating) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY_ROSTER);
  }
}

/* Draw a centred connection state modal showing current network status. */
static void DrawConnectionStateModal(ShroomScreenManager* manager, Game* game) {
  const float w = 400.0f;
  const float h = 200.0f;
  ShroomImGui_SetNextWindowPos((GetScreenWidth() - w) * 0.5f, (GetScreenHeight() - h) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(w, h, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.92f);
  if (ShroomImGui_Begin("Connection Status", NULL,
                        SHROOM_IMGUI_WINDOW_NO_TITLE_BAR | SHROOM_IMGUI_WINDOW_NO_RESIZE |
                            SHROOM_IMGUI_WINDOW_NO_MOVE | SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                            SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS |
                            SHROOM_IMGUI_WINDOW_NO_SCROLLBAR)) {
    const char* status_text = "Connecting...";
    Color status_color = (Color){200, 200, 200, 255};

    if (game->net.status == CLIENT_NET_ERROR) {
      status_text = "Connection Error";
      status_color = (Color){255, 100, 100, 255};
    } else if (game->net.status == CLIENT_NET_DISCONNECTED) {
      status_text = "Disconnected";
      status_color = (Color){255, 150, 100, 255};
    } else if (game->net.status == CLIENT_NET_CONNECTED && !game->net.handshake_received) {
      status_text = "Handshaking...";
      status_color = (Color){100, 200, 255, 255};
    } else if (game->net.status == CLIENT_NET_CONNECTING) {
      status_text = "Connecting...";
      status_color = (Color){200, 200, 200, 255};
    }

    ShroomImGui_TextColored((ShroomImGuiColor){status_color.r / 255.0f, status_color.g / 255.0f,
                                               status_color.b / 255.0f, status_color.a / 255.0f},
                            status_text);

    if (game->net.status_text[0] != '\0') {
      ShroomImGui_TextWrapped(game->net.status_text);
    }

    ShroomImGui_Spacing();

    if (game->net.status == CLIENT_NET_ERROR || game->net.status == CLIENT_NET_DISCONNECTED) {
      if (ShroomImGui_Button("Retry", 120.0f, 0.0f)) {
        ClientNetShutdown(&game->net);
        ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
      }
      ShroomImGui_SameLine();
      if (ShroomImGui_Button("Back", 120.0f, 0.0f)) {
        ClientNetShutdown(&game->net);
        game->auto_join_lobby = false;
        ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
      }
    } else {
      ShroomImGui_Text("Press Esc to cancel");
    }
  }
  ShroomImGui_End();
}

static void LobbyBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  uint8_t i;

  if (game == NULL) {
    return;
  }

  ShroomScreenDrawFungalBackground(game->settings.menu_animations_enabled);

  /* Show connection state modal during connection phases or errors. */
  if (game->auto_join_lobby || (!game->net.handshake_received) ||
      game->net.status == CLIENT_NET_ERROR || game->net.status == CLIENT_NET_DISCONNECTED) {
    DrawConnectionStateModal(manager, game);
    return;
  }

  ShroomImGui_SetNextWindowPos((GetScreenWidth() - 600.0f) * 0.5f,
                               (GetScreenHeight() - 480.0f) * 0.5f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(600.0f, 480.0f, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.88f);
  if (!ShroomImGui_Begin("Lobby Browser", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text(TextFormat("Lobbies  (refreshes every %.0fs)", LOBBY_REFRESH_INTERVAL));
  if (ShroomImGui_Button("Refresh", 80.0f, 0.0f)) {
    g_refresh_timer = 0.0f;
    ClientNetSendLobbyListQuery(&game->net);
  }
  ShroomImGui_Separator();

  if (game->net.lobby_count == 0) {
    ShroomImGui_Text("No lobbies found.");
  } else if (ShroomImGui_BeginTable("LobbyTable", 5,
                                    SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                        SHROOM_IMGUI_TABLE_SIZING_FIXED,
                                    570.0f, 220.0f)) {
    ShroomImGui_TableSetupColumn("Name", 180.0f);
    ShroomImGui_TableSetupColumn("Players", 80.0f);
    ShroomImGui_TableSetupColumn("Bots", 60.0f);
    ShroomImGui_TableSetupColumn("Status", 80.0f);
    ShroomImGui_TableSetupColumn("Actions", 140.0f);
    ShroomImGui_TableHeadersRow();

    for (i = 0; i < game->net.lobby_count; ++i) {
      const ShroomLobbyEntry* entry = &game->net.lobby_list[i];

      ShroomImGui_TableNextRow();
      ShroomImGui_TableSetColumnIndex(0);
      ShroomImGui_Text(entry->name);
      ShroomImGui_TableSetColumnIndex(1);
      ShroomImGui_Text(TextFormat("%u / %u", entry->player_count, entry->max_players));
      ShroomImGui_TableSetColumnIndex(2);
      ShroomImGui_Text(TextFormat("%u", entry->bot_count));
      ShroomImGui_TableSetColumnIndex(3);
      ShroomImGui_Text(LobbyStatusLabel(entry));
      ShroomImGui_TableSetColumnIndex(4);
      if (entry->player_count < entry->max_players) {
        if (ShroomImGui_Button(TextFormat("Join##%u", entry->lobby_id), 56.0f, 0.0f)) {
          ClientNetSendLobbyJoin(&game->net, entry->lobby_id, false);
        }
        ShroomImGui_SameLine();
      }
      if (ShroomImGui_Button(TextFormat("Watch##%u", entry->lobby_id), 60.0f, 0.0f)) {
        ClientNetSendLobbyJoin(&game->net, entry->lobby_id, true);
      }
    }
    ShroomImGui_EndTable();
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Create Lobby");
  ShroomImGui_SetNextItemWidth(260.0f);
  ShroomImGui_InputText("##lobbyname", g_create_name_buf, sizeof(g_create_name_buf));
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Create", 70.0f, 0.0f)) {
    ClientNetSendLobbyCreate(&game->net, g_create_name_buf, 0);
    g_create_name_buf[0] = '\0';
    g_refresh_timer = LOBBY_REFRESH_INTERVAL;
  }

  ShroomImGui_Separator();
  if (ShroomImGui_Button("Back", 80.0f, 0.0f)) {
    ClientNetShutdown(&game->net);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }

  ShroomImGui_End();
}

static void LobbyBrowserHandleInput(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  if (!ShroomImGui_WantCaptureKeyboard() && IsKeyPressed(KEY_ESCAPE)) {
    ClientNetShutdown(&game->net);
    game->auto_join_lobby = false;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
}

static void LobbyBrowserCleanup(ShroomScreenManager* manager) {
  (void)manager;
  g_refresh_timer = 0.0f;
  g_handshake_seen = false;
  g_create_name_buf[0] = '\0';
}

void ShroomScreenRegisterLobbyBrowser(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_LOBBY];
  screen->type = SHROOM_SCREEN_LOBBY;
  screen->name = "Lobby Browser";
  screen->init = LobbyBrowserInit;
  screen->update = LobbyBrowserUpdate;
  screen->draw = LobbyBrowserDraw;
  screen->handle_input = LobbyBrowserHandleInput;
  screen->cleanup = LobbyBrowserCleanup;
}
