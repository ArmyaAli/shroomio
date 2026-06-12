#include <stddef.h>
#include <string.h>

#include "game.h"
#include "screen.h"

#include "raygui.h"
#include "raylib.h"

typedef enum ServerBrowserViewState {
  SERVER_BROWSER_VIEW_LOADING = 0,
  SERVER_BROWSER_VIEW_READY,
  SERVER_BROWSER_VIEW_EMPTY,
  SERVER_BROWSER_VIEW_ERROR,
} ServerBrowserViewState;

typedef enum ServerBrowserResultState {
  SERVER_BROWSER_RESULT_READY = 0,
  SERVER_BROWSER_RESULT_EMPTY,
  SERVER_BROWSER_RESULT_ERROR,
} ServerBrowserResultState;

typedef enum ServerBrowserSortKey {
  SERVER_BROWSER_SORT_NAME = 0,
  SERVER_BROWSER_SORT_PLAYERS,
  SERVER_BROWSER_SORT_PING,
} ServerBrowserSortKey;

typedef struct ServerBrowserEntry {
  const char* name;
  int player_count;
  int player_capacity;
  int ping_ms;
  const char* map_label;
} ServerBrowserEntry;

typedef struct ServerBrowserState {
  ServerBrowserViewState view_state;
  ServerBrowserResultState pending_result;
  ServerBrowserSortKey sort_key;
  bool sort_descending;
  float loading_timer;
  int selected_index;
  size_t server_count;
  ServerBrowserEntry servers[6];
} ServerBrowserState;

static ServerBrowserState g_server_browser;

static void SortServerEntries(ServerBrowserState* browser) {
  for (size_t outer_index = 0; outer_index + 1 < browser->server_count; ++outer_index) {
    for (size_t inner_index = outer_index + 1; inner_index < browser->server_count; ++inner_index) {
      const ServerBrowserEntry* left = &browser->servers[outer_index];
      const ServerBrowserEntry* right = &browser->servers[inner_index];
      bool swap = false;

      switch (browser->sort_key) {
      case SERVER_BROWSER_SORT_PLAYERS:
        if (left->player_count != right->player_count) {
          swap = browser->sort_descending ? (left->player_count < right->player_count)
                                          : (left->player_count > right->player_count);
        } else if (left->ping_ms != right->ping_ms) {
          swap = left->ping_ms > right->ping_ms;
        } else {
          swap = strcmp(left->name, right->name) > 0;
        }
        break;
      case SERVER_BROWSER_SORT_PING:
        if (left->ping_ms != right->ping_ms) {
          swap = browser->sort_descending ? (left->ping_ms < right->ping_ms)
                                          : (left->ping_ms > right->ping_ms);
        } else if (left->player_count != right->player_count) {
          swap = left->player_count < right->player_count;
        } else {
          swap = strcmp(left->name, right->name) > 0;
        }
        break;
      case SERVER_BROWSER_SORT_NAME:
      default: {
        const int name_compare = strcmp(left->name, right->name);
        if (name_compare != 0) {
          swap = browser->sort_descending ? (name_compare < 0) : (name_compare > 0);
        } else if (left->ping_ms != right->ping_ms) {
          swap = left->ping_ms > right->ping_ms;
        } else {
          swap = left->player_count < right->player_count;
        }
      } break;
      }

      if (swap) {
        const ServerBrowserEntry temp = browser->servers[outer_index];
        browser->servers[outer_index] = browser->servers[inner_index];
        browser->servers[inner_index] = temp;
      }
    }
  }
}

static void LoadSampleServers(ServerBrowserState* browser) {
  browser->server_count = 5;
  browser->servers[0] = (ServerBrowserEntry){
      .name = "Local Development",
      .player_count = 3,
      .player_capacity = 32,
      .ping_ms = 12,
      .map_label = "Arena / Local",
  };
  browser->servers[1] = (ServerBrowserEntry){
      .name = "Canopy Clash EU",
      .player_count = 24,
      .player_capacity = 32,
      .ping_ms = 46,
      .map_label = "Arena / Public",
  };
  browser->servers[2] = (ServerBrowserEntry){
      .name = "Spore Sprint NA",
      .player_count = 17,
      .player_capacity = 32,
      .ping_ms = 78,
      .map_label = "Arena / Ranked",
  };
  browser->servers[3] = (ServerBrowserEntry){
      .name = "Outer Ring Learn",
      .player_count = 8,
      .player_capacity = 16,
      .ping_ms = 33,
      .map_label = "Practice / Casual",
  };
  browser->servers[4] = (ServerBrowserEntry){
      .name = "Center Rush",
      .player_count = 29,
      .player_capacity = 32,
      .ping_ms = 61,
      .map_label = "Arena / High Risk",
  };
  SortServerEntries(browser);
}

static void StartLoading(ServerBrowserState* browser, ServerBrowserResultState pending_result) {
  browser->view_state = SERVER_BROWSER_VIEW_LOADING;
  browser->pending_result = pending_result;
  browser->loading_timer = 0.45f;
}

static void ApplySortSelection(ServerBrowserState* browser, ServerBrowserSortKey sort_key) {
  if (browser->sort_key == sort_key) {
    browser->sort_descending = !browser->sort_descending;
  } else {
    browser->sort_key = sort_key;
    browser->sort_descending = sort_key == SERVER_BROWSER_SORT_PLAYERS;
  }

  if (browser->server_count > 0) {
    SortServerEntries(browser);
  }
}

static bool CanJoinSelectedServer(const ServerBrowserState* browser) {
  return (browser->view_state == SERVER_BROWSER_VIEW_READY) && (browser->server_count > 0) &&
         (browser->selected_index >= 0) &&
         ((size_t)browser->selected_index < browser->server_count);
}

static void JoinSelectedServer(ShroomScreenManager* manager, Game* game) {
  if (game != NULL) {
    game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
  }

  ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
}

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  (void)manager;

  g_server_browser = (ServerBrowserState){0};
  g_server_browser.sort_key = SERVER_BROWSER_SORT_PING;
  g_server_browser.selected_index = 0;
  LoadSampleServers(&g_server_browser);
  StartLoading(&g_server_browser, SERVER_BROWSER_RESULT_READY);
  return true;
}

static void ServerBrowserUpdate(ShroomScreenManager* manager, float delta_time) {
  (void)manager;

  if (g_server_browser.view_state != SERVER_BROWSER_VIEW_LOADING) {
    return;
  }

  g_server_browser.loading_timer -= delta_time;
  if (g_server_browser.loading_timer > 0.0f) {
    return;
  }

  switch (g_server_browser.pending_result) {
  case SERVER_BROWSER_RESULT_EMPTY:
    g_server_browser.view_state = SERVER_BROWSER_VIEW_EMPTY;
    break;
  case SERVER_BROWSER_RESULT_ERROR:
    g_server_browser.view_state = SERVER_BROWSER_VIEW_ERROR;
    break;
  case SERVER_BROWSER_RESULT_READY:
  default:
    g_server_browser.view_state = SERVER_BROWSER_VIEW_READY;
    if (g_server_browser.selected_index < 0) {
      g_server_browser.selected_index = 0;
    }
    break;
  }
}

static void DrawStatePanel(const ServerBrowserState* browser, Rectangle panel) {
  const int center_x = (int)(panel.x + (panel.width / 2.0f));
  const int center_y = (int)(panel.y + (panel.height / 2.0f));

  switch (browser->view_state) {
  case SERVER_BROWSER_VIEW_LOADING:
    DrawText("Refreshing server list...", center_x - 138, center_y - 18, 28, RAYWHITE);
    DrawText("Please wait while available servers are fetched.", center_x - 176, center_y + 18, 20,
             GRAY);
    break;
  case SERVER_BROWSER_VIEW_EMPTY:
    DrawText("No servers available", center_x - 124, center_y - 18, 28, RAYWHITE);
    DrawText("Try Refresh or return later when a server is online.", center_x - 196, center_y + 18,
             20, GRAY);
    break;
  case SERVER_BROWSER_VIEW_ERROR:
    DrawText("Server list unavailable", center_x - 142, center_y - 18, 28, ORANGE);
    DrawText("Refresh to retry the browser query or use Quick Play.", center_x - 188, center_y + 18,
             20, GRAY);
    break;
  case SERVER_BROWSER_VIEW_READY:
  default:
    break;
  }
}

static void DrawServerRows(ServerBrowserState* browser, Rectangle panel) {
  const float header_y = panel.y + 18.0f;
  const float rows_start_y = panel.y + 56.0f;
  const float row_height = 46.0f;

  DrawText("Name", (int)panel.x + 18, (int)header_y, 20, LIGHTGRAY);
  DrawText("Players", (int)panel.x + 312, (int)header_y, 20, LIGHTGRAY);
  DrawText("Ping", (int)panel.x + 438, (int)header_y, 20, LIGHTGRAY);
  DrawText("Map / Label", (int)panel.x + 530, (int)header_y, 20, LIGHTGRAY);

  for (size_t index = 0; index < browser->server_count; ++index) {
    Rectangle row = {panel.x + 10.0f, rows_start_y + ((float)index * row_height),
                     panel.width - 20.0f, row_height - 4.0f};
    const ServerBrowserEntry* entry = &browser->servers[index];
    const bool selected = (int)index == browser->selected_index;
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), row);
    const Color row_color =
        selected ? Fade(SKYBLUE, 0.28f) : (hovered ? Fade(RAYWHITE, 0.08f) : Fade(BLACK, 0.18f));
    const Color outline = selected ? SKYBLUE : Fade(RAYWHITE, 0.08f);

    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      browser->selected_index = (int)index;
    }

    DrawRectangleRec(row, row_color);
    DrawRectangleLinesEx(row, 1.0f, outline);
    DrawText(entry->name, (int)row.x + 12, (int)row.y + 12, 20, RAYWHITE);
    DrawText(TextFormat("%d/%d", entry->player_count, entry->player_capacity), (int)row.x + 314,
             (int)row.y + 12, 20, RAYWHITE);
    DrawText(TextFormat("%d ms", entry->ping_ms), (int)row.x + 438, (int)row.y + 12, 20,
             entry->ping_ms <= 40 ? LIME : (entry->ping_ms <= 75 ? GOLD : ORANGE));
    DrawText(entry->map_label, (int)row.x + 530, (int)row.y + 12, 20, LIGHTGRAY);
  }
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  ServerBrowserState* browser = &g_server_browser;
  const int screen_width = GetScreenWidth();
  const Rectangle table_panel = {100.0f, 144.0f, (float)screen_width - 200.0f, 316.0f};
  const bool can_join = CanJoinSelectedServer(browser);
  const char* sort_order = browser->sort_descending ? "desc" : "asc";

  BeginDrawing();
  ClearBackground((Color){24, 28, 42, 255});

  DrawText("SERVER BROWSER", screen_width / 2 - 148, 44, 34, RAYWHITE);
  DrawText("Browse available servers, sort the list, and join intentionally.",
           screen_width / 2 - 280, 84, 22, GRAY);

  if (GuiButton((Rectangle){100, 108, 120, 28}, "REFRESH")) {
    LoadSampleServers(browser);
    StartLoading(browser, SERVER_BROWSER_RESULT_READY);
  }
  if (GuiButton((Rectangle){232, 108, 120, 28}, "SHOW EMPTY")) {
    StartLoading(browser, SERVER_BROWSER_RESULT_EMPTY);
  }
  if (GuiButton((Rectangle){364, 108, 120, 28}, "SHOW ERROR")) {
    StartLoading(browser, SERVER_BROWSER_RESULT_ERROR);
  }

  if (GuiButton((Rectangle){screen_width - 454, 108, 110, 28}, "SORT NAME")) {
    ApplySortSelection(browser, SERVER_BROWSER_SORT_NAME);
  }
  if (GuiButton((Rectangle){screen_width - 332, 108, 110, 28}, "SORT PLAYERS")) {
    ApplySortSelection(browser, SERVER_BROWSER_SORT_PLAYERS);
  }
  if (GuiButton((Rectangle){screen_width - 210, 108, 110, 28}, "SORT PING")) {
    ApplySortSelection(browser, SERVER_BROWSER_SORT_PING);
  }

  DrawText(TextFormat("Sort: %s (%s)",
                      browser->sort_key == SERVER_BROWSER_SORT_NAME
                          ? "name"
                          : (browser->sort_key == SERVER_BROWSER_SORT_PLAYERS ? "players" : "ping"),
                      sort_order),
           screen_width - 280, 468, 18, LIGHTGRAY);

  DrawRectangleRec(table_panel, Fade(BLACK, 0.26f));
  DrawRectangleLinesEx(table_panel, 1.0f, Fade(RAYWHITE, 0.16f));

  if (browser->view_state == SERVER_BROWSER_VIEW_READY) {
    DrawServerRows(browser, table_panel);
  } else {
    DrawStatePanel(browser, table_panel);
  }

  DrawRectangle(100, 490, screen_width - 200, 92, Fade(BLACK, 0.26f));
  DrawRectangleLines(100, 490, screen_width - 200, 92, Fade(RAYWHITE, 0.16f));
  if (can_join) {
    const ServerBrowserEntry* entry = &browser->servers[browser->selected_index];
    DrawText(TextFormat("Selected: %s", entry->name), 120, 512, 24, RAYWHITE);
    DrawText(TextFormat("Players %d/%d   Ping %d ms   %s", entry->player_count,
                        entry->player_capacity, entry->ping_ms, entry->map_label),
             120, 544, 20, LIGHTGRAY);
  } else if (browser->view_state == SERVER_BROWSER_VIEW_LOADING) {
    DrawText("Selection unavailable while the list is loading.", 120, 526, 22, GRAY);
  } else if (browser->view_state == SERVER_BROWSER_VIEW_EMPTY) {
    DrawText("No selection available. Refresh when servers are online.", 120, 526, 22, GRAY);
  } else {
    DrawText("Browser error state active. Refresh to restore the list.", 120, 526, 22, GRAY);
  }

  if (can_join && GuiButton((Rectangle){screen_width - 304, 512, 184, 48}, "JOIN SELECTED")) {
    JoinSelectedServer(manager, game);
  }

  if (GuiButton((Rectangle){screen_width - 304, 568, 184, 40}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }

  DrawText("Hotkeys: Up/Down select, Enter joins, R refreshes, Esc goes back", 100,
           GetScreenHeight() - 36, 18, GRAY);

  EndDrawing();
}

static void ServerBrowserHandleInput(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  ServerBrowserState* browser = &g_server_browser;

  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
    return;
  }

  if (IsKeyPressed(KEY_R)) {
    LoadSampleServers(browser);
    StartLoading(browser, SERVER_BROWSER_RESULT_READY);
    return;
  }

  if (browser->view_state != SERVER_BROWSER_VIEW_READY) {
    return;
  }

  if (IsKeyPressed(KEY_DOWN) && (browser->selected_index + 1 < (int)browser->server_count)) {
    browser->selected_index += 1;
  }
  if (IsKeyPressed(KEY_UP) && (browser->selected_index > 0)) {
    browser->selected_index -= 1;
  }
  if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_J)) && CanJoinSelectedServer(browser)) {
    JoinSelectedServer(manager, game);
  }
}

void ShroomScreenRegisterServerBrowser(ShroomScreenManager* manager) {
  if (manager == NULL) {
    return;
  }

  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_SERVER_BROWSER];
  screen->type = SHROOM_SCREEN_SERVER_BROWSER;
  screen->name = "Server Browser";
  screen->init = ServerBrowserInit;
  screen->update = ServerBrowserUpdate;
  screen->draw = ServerBrowserDraw;
  screen->handle_input = ServerBrowserHandleInput;
}
