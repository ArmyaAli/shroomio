#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "game.h"
#include "screen.h"
#include "shared/config.h"

#include "raygui.h"
#include "raylib.h"

#define SERVER_BROWSER_MAX_SERVERS 6u
#define SERVER_BROWSER_MAX_RECENTS 5u
#define SERVER_BROWSER_PORT_TEXT_LENGTH 8u

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
  char name[64];
  char host[64];
  int player_count;
  int player_capacity;
  int ping_ms;
  int port;
  char map_label[64];
} ServerBrowserEntry;

typedef struct ServerBrowserState {
  ServerBrowserViewState view_state;
  ServerBrowserResultState pending_result;
  ServerBrowserSortKey sort_key;
  bool sort_descending;
  bool host_text_edit_mode;
  bool port_text_edit_mode;
  float loading_timer;
  int selected_index;
  size_t server_count;
  size_t recent_count;
  int selected_recent_index;
  char direct_host_input[64];
  char direct_port_input[SERVER_BROWSER_PORT_TEXT_LENGTH];
  char validation_message[128];
  ServerBrowserEntry servers[SERVER_BROWSER_MAX_SERVERS];
  ServerBrowserEntry recent_servers[SERVER_BROWSER_MAX_RECENTS];
} ServerBrowserState;

static const char* kRecentServersPath = "server_browser_recent.txt";

static ServerBrowserState g_server_browser;

static void CopyText(char* destination, size_t destination_size, const char* source) {
  size_t length;

  if ((destination == NULL) || (destination_size == 0u)) {
    return;
  }

  if (source == NULL) {
    destination[0] = '\0';
    return;
  }

  length = strnlen(source, destination_size - 1u);
  memcpy(destination, source, length);
  destination[length] = '\0';
}

static void SetEntryDisplayName(ServerBrowserEntry* entry, const char* host, int port) {
  size_t host_length;

  if (entry == NULL) {
    return;
  }

  host_length = strnlen(host, sizeof(entry->name) - 8u);
  memcpy(entry->name, host, host_length);
  snprintf(entry->name + host_length, sizeof(entry->name) - host_length, ":%d", port);
}

static void SortServerEntries(ServerBrowserState* browser, ServerBrowserEntry* entries,
                              size_t count) {
  for (size_t outer_index = 0; outer_index + 1 < count; ++outer_index) {
    for (size_t inner_index = outer_index + 1; inner_index < count; ++inner_index) {
      const ServerBrowserEntry* left = &entries[outer_index];
      const ServerBrowserEntry* right = &entries[inner_index];
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
        const ServerBrowserEntry temp = entries[outer_index];
        entries[outer_index] = entries[inner_index];
        entries[inner_index] = temp;
      }
    }
  }
}

static void SaveRecentServers(const ServerBrowserState* browser) {
  const int file_descriptor = open(kRecentServersPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  FILE* file;

  if (file_descriptor < 0) {
    return;
  }

  file = fdopen(file_descriptor, "w");
  if (file == NULL) {
    close(file_descriptor);
    return;
  }

  for (size_t index = 0; index < browser->recent_count; ++index) {
    const ServerBrowserEntry* entry = &browser->recent_servers[index];
    fprintf(file, "%s|%s|%d|%s\n", entry->name, entry->host, entry->port, entry->map_label);
  }

  fclose(file);
}

static void LoadRecentServers(ServerBrowserState* browser) {
  FILE* file = fopen(kRecentServersPath, "r");
  char line[256];

  browser->recent_count = 0;
  browser->selected_recent_index = -1;

  if (file == NULL) {
    return;
  }

  while ((browser->recent_count < SERVER_BROWSER_MAX_RECENTS) &&
         (fgets(line, sizeof(line), file) != NULL)) {
    ServerBrowserEntry* entry = &browser->recent_servers[browser->recent_count];
    char* separator;
    const char* port_text;
    const char* map_label;

    line[strcspn(line, "\r\n")] = '\0';
    separator = strchr(line, '|');
    if (separator == NULL) {
      continue;
    }
    *separator = '\0';
    CopyText(entry->name, sizeof(entry->name), line);

    port_text = separator + 1;
    separator = strchr(port_text, '|');
    if (separator == NULL) {
      continue;
    }
    *separator = '\0';
    CopyText(entry->host, sizeof(entry->host), port_text);

    map_label = separator + 1;
    separator = strchr(map_label, '|');
    if (separator == NULL) {
      continue;
    }
    *separator = '\0';
    entry->port = atoi(map_label);
    map_label = separator + 1;
    CopyText(entry->map_label, sizeof(entry->map_label), map_label);

    entry->player_count = 0;
    entry->player_capacity = 0;
    entry->ping_ms = 0;

    browser->recent_count += 1;
  }

  fclose(file);

  if (browser->recent_count > 0) {
    browser->selected_recent_index = 0;
  }
}

static void AddRecentServer(ServerBrowserState* browser, const ServerBrowserEntry* entry) {
  ServerBrowserEntry saved_entry = *entry;
  size_t target_index = 0;

  for (size_t index = 0; index < browser->recent_count; ++index) {
    if ((strcmp(browser->recent_servers[index].host, entry->host) == 0) &&
        (browser->recent_servers[index].port == entry->port)) {
      target_index = index;
      break;
    }
    target_index = index + 1;
  }

  if (target_index >= SERVER_BROWSER_MAX_RECENTS) {
    target_index = SERVER_BROWSER_MAX_RECENTS - 1;
  }

  for (size_t index = target_index; index > 0; --index) {
    browser->recent_servers[index] = browser->recent_servers[index - 1];
  }
  browser->recent_servers[0] = saved_entry;

  if (browser->recent_count < SERVER_BROWSER_MAX_RECENTS) {
    browser->recent_count += 1;
  }
  browser->selected_recent_index = 0;
  SaveRecentServers(browser);
}

static void LoadSampleServers(ServerBrowserState* browser) {
  browser->server_count = 5;
  browser->servers[0] = (ServerBrowserEntry){.name = "Local Development",
                                             .host = "127.0.0.1",
                                             .player_count = 3,
                                             .player_capacity = 32,
                                             .ping_ms = 12,
                                             .port = SHROOM_SERVER_PORT,
                                             .map_label = "Arena / Local"};
  browser->servers[1] = (ServerBrowserEntry){.name = "Canopy Clash EU",
                                             .host = "eu.shroomio.dev",
                                             .player_count = 24,
                                             .player_capacity = 32,
                                             .ping_ms = 46,
                                             .port = SHROOM_SERVER_PORT,
                                             .map_label = "Arena / Public"};
  browser->servers[2] = (ServerBrowserEntry){.name = "Spore Sprint NA",
                                             .host = "na.shroomio.dev",
                                             .player_count = 17,
                                             .player_capacity = 32,
                                             .ping_ms = 78,
                                             .port = SHROOM_SERVER_PORT,
                                             .map_label = "Arena / Ranked"};
  browser->servers[3] = (ServerBrowserEntry){.name = "Outer Ring Learn",
                                             .host = "practice.shroomio.dev",
                                             .player_count = 8,
                                             .player_capacity = 16,
                                             .ping_ms = 33,
                                             .port = SHROOM_SERVER_PORT,
                                             .map_label = "Practice / Casual"};
  browser->servers[4] = (ServerBrowserEntry){.name = "Center Rush",
                                             .host = "rush.shroomio.dev",
                                             .player_count = 29,
                                             .player_capacity = 32,
                                             .ping_ms = 61,
                                             .port = SHROOM_SERVER_PORT,
                                             .map_label = "Arena / High Risk"};
  SortServerEntries(browser, browser->servers, browser->server_count);
}

static void StartLoading(ServerBrowserState* browser, ServerBrowserResultState pending_result) {
  browser->view_state = SERVER_BROWSER_VIEW_LOADING;
  browser->pending_result = pending_result;
  browser->loading_timer = 0.45f;
  browser->validation_message[0] = '\0';
}

static void ApplySortSelection(ServerBrowserState* browser, ServerBrowserSortKey sort_key) {
  if (browser->sort_key == sort_key) {
    browser->sort_descending = !browser->sort_descending;
  } else {
    browser->sort_key = sort_key;
    browser->sort_descending = sort_key == SERVER_BROWSER_SORT_PLAYERS;
  }

  if (browser->server_count > 0) {
    const ServerBrowserEntry selected =
        browser->servers[browser->selected_index >= 0 ? (size_t)browser->selected_index : 0];
    SortServerEntries(browser, browser->servers, browser->server_count);
    for (size_t index = 0; index < browser->server_count; ++index) {
      if ((strcmp(browser->servers[index].host, selected.host) == 0) &&
          (browser->servers[index].port == selected.port)) {
        browser->selected_index = (int)index;
        break;
      }
    }
  }
}

static bool CanJoinSelectedServer(const ServerBrowserState* browser) {
  return (browser->view_state == SERVER_BROWSER_VIEW_READY) && (browser->server_count > 0) &&
         (browser->selected_index >= 0) &&
         ((size_t)browser->selected_index < browser->server_count);
}

static bool ParseDirectConnect(ServerBrowserState* browser, ServerBrowserEntry* entry) {
  char* end = NULL;
  long port = 0;

  if (browser->direct_host_input[0] == '\0') {
    snprintf(browser->validation_message, sizeof(browser->validation_message),
             "Host or IP is required.");
    return false;
  }
  if (strchr(browser->direct_host_input, ' ') != NULL) {
    snprintf(browser->validation_message, sizeof(browser->validation_message),
             "Host or IP cannot contain spaces.");
    return false;
  }

  port = strtol(browser->direct_port_input, &end, 10);
  if ((browser->direct_port_input[0] == '\0') || (end == NULL) || (*end != '\0') || (port < 1) ||
      (port > 65535)) {
    snprintf(browser->validation_message, sizeof(browser->validation_message),
             "Port must be between 1 and 65535.");
    return false;
  }

  *entry = (ServerBrowserEntry){0};
  SetEntryDisplayName(entry, browser->direct_host_input, (int)port);
  CopyText(entry->host, sizeof(entry->host), browser->direct_host_input);
  entry->port = (int)port;
  CopyText(entry->map_label, sizeof(entry->map_label), "Direct Connect");
  browser->validation_message[0] = '\0';
  return true;
}

static void JoinServerEntry(ShroomScreenManager* manager, Game* game, ServerBrowserState* browser,
                            const ServerBrowserEntry* entry) {
  if (game != NULL) {
    game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    CopyText(game->selected_server_host, sizeof(game->selected_server_host), entry->host);
    game->selected_server_port = (uint16_t)entry->port;
  }

  AddRecentServer(browser, entry);
  ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
}

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  (void)manager;

  g_server_browser = (ServerBrowserState){0};
  g_server_browser.sort_key = SERVER_BROWSER_SORT_PING;
  g_server_browser.selected_index = 0;
  g_server_browser.selected_recent_index = -1;
  CopyText(g_server_browser.direct_host_input, sizeof(g_server_browser.direct_host_input),
           "127.0.0.1");
  snprintf(g_server_browser.direct_port_input, sizeof(g_server_browser.direct_port_input), "%u",
           SHROOM_SERVER_PORT);
  LoadRecentServers(&g_server_browser);
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
    DrawText("Refresh to retry the browser query or use Direct Connect.", center_x - 202,
             center_y + 18, 20, GRAY);
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

static void DrawRecentServers(ServerBrowserState* browser, Rectangle panel) {
  DrawText("Recent Servers", (int)panel.x + 18, (int)panel.y + 14, 22, RAYWHITE);

  if (browser->recent_count == 0) {
    DrawText("No recent servers yet.", (int)panel.x + 18, (int)panel.y + 48, 18, GRAY);
    DrawText("Joined servers will be saved here for later.", (int)panel.x + 18, (int)panel.y + 72,
             18, GRAY);
    return;
  }

  for (size_t index = 0; index < browser->recent_count; ++index) {
    const ServerBrowserEntry* entry = &browser->recent_servers[index];
    const int y = (int)panel.y + 44 + ((int)index * 34);
    Rectangle button = {panel.x + 14.0f, (float)y, 256.0f, 28.0f};
    if (GuiButton(button, entry->name)) {
      snprintf(browser->direct_host_input, sizeof(browser->direct_host_input), "%s", entry->host);
      snprintf(browser->direct_port_input, sizeof(browser->direct_port_input), "%d", entry->port);
      browser->selected_recent_index = (int)index;
      browser->validation_message[0] = '\0';
    }
  }
}

static void DrawDirectConnect(ServerBrowserState* browser, Rectangle panel,
                              ShroomScreenManager* manager, Game* game) {
  ServerBrowserEntry direct_entry;

  DrawText("Direct Connect", (int)panel.x + 18, (int)panel.y + 14, 22, RAYWHITE);
  DrawText("Host / IP", (int)panel.x + 18, (int)panel.y + 50, 18, LIGHTGRAY);
  if (GuiTextBox((Rectangle){panel.x + 18.0f, panel.y + 74.0f, 256.0f, 34.0f},
                 browser->direct_host_input, sizeof(browser->direct_host_input),
                 browser->host_text_edit_mode)) {
    browser->host_text_edit_mode = !browser->host_text_edit_mode;
  }

  DrawText("Port", (int)panel.x + 18, (int)panel.y + 122, 18, LIGHTGRAY);
  if (GuiTextBox((Rectangle){panel.x + 18.0f, panel.y + 146.0f, 120.0f, 34.0f},
                 browser->direct_port_input, sizeof(browser->direct_port_input),
                 browser->port_text_edit_mode)) {
    browser->port_text_edit_mode = !browser->port_text_edit_mode;
  }

  if (GuiButton((Rectangle){panel.x + 154.0f, panel.y + 146.0f, 120.0f, 34.0f}, "CONNECT")) {
    if (ParseDirectConnect(browser, &direct_entry)) {
      JoinServerEntry(manager, game, browser, &direct_entry);
    }
  }

  if (browser->validation_message[0] != '\0') {
    DrawText(browser->validation_message, (int)panel.x + 18, (int)panel.y + 190, 18, ORANGE);
  } else {
    DrawText("Enter a known host and port to bypass browsing.", (int)panel.x + 18,
             (int)panel.y + 190, 18, GRAY);
  }
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  ServerBrowserState* browser = &g_server_browser;
  const int screen_width = GetScreenWidth();
  const Rectangle utility_panel = {40.0f, 144.0f, 292.0f, 430.0f};
  const Rectangle recent_panel = {40.0f, 370.0f, 292.0f, 204.0f};
  const Rectangle table_panel = {356.0f, 144.0f, (float)screen_width - 396.0f, 316.0f};
  const bool can_join = CanJoinSelectedServer(browser);
  const char* sort_order = browser->sort_descending ? "desc" : "asc";

  BeginDrawing();
  ClearBackground((Color){24, 28, 42, 255});

  DrawText("SERVER BROWSER", screen_width / 2 - 148, 44, 34, RAYWHITE);
  DrawText("Browse available servers, use direct connect, or jump back to a recent target.",
           screen_width / 2 - 342, 84, 22, GRAY);

  DrawRectangleRec(utility_panel, Fade(BLACK, 0.26f));
  DrawRectangleLinesEx(utility_panel, 1.0f, Fade(RAYWHITE, 0.16f));
  DrawDirectConnect(browser, utility_panel, manager, game);

  DrawRectangleRec(recent_panel, Fade(BLACK, 0.26f));
  DrawRectangleLinesEx(recent_panel, 1.0f, Fade(RAYWHITE, 0.16f));
  DrawRecentServers(browser, recent_panel);

  if (GuiButton((Rectangle){356, 108, 120, 28}, "REFRESH")) {
    LoadSampleServers(browser);
    StartLoading(browser, SERVER_BROWSER_RESULT_READY);
  }
  if (GuiButton((Rectangle){488, 108, 120, 28}, "SHOW EMPTY")) {
    StartLoading(browser, SERVER_BROWSER_RESULT_EMPTY);
  }
  if (GuiButton((Rectangle){620, 108, 120, 28}, "SHOW ERROR")) {
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

  DrawRectangle(356, 490, screen_width - 396, 92, Fade(BLACK, 0.26f));
  DrawRectangleLines(356, 490, screen_width - 396, 92, Fade(RAYWHITE, 0.16f));
  if (can_join) {
    const ServerBrowserEntry* entry = &browser->servers[browser->selected_index];
    DrawText(TextFormat("Selected: %s", entry->name), 376, 512, 24, RAYWHITE);
    DrawText(TextFormat("Players %d/%d   Ping %d ms   %s:%d", entry->player_count,
                        entry->player_capacity, entry->ping_ms, entry->host, entry->port),
             376, 544, 20, LIGHTGRAY);
  } else if (browser->view_state == SERVER_BROWSER_VIEW_LOADING) {
    DrawText("Selection unavailable while the list is loading.", 376, 526, 22, GRAY);
  } else if (browser->view_state == SERVER_BROWSER_VIEW_EMPTY) {
    DrawText("No selection available. Use Direct Connect or Refresh.", 376, 526, 22, GRAY);
  } else {
    DrawText("Browser error state active. Use Direct Connect or Refresh.", 376, 526, 22, GRAY);
  }

  if (can_join && GuiButton((Rectangle){screen_width - 304, 512, 184, 48}, "JOIN SELECTED")) {
    JoinServerEntry(manager, game, browser, &browser->servers[browser->selected_index]);
  }
  if (GuiButton((Rectangle){screen_width - 304, 568, 184, 40}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }

  DrawText("Hotkeys: Up/Down select, Enter joins, R refreshes, Esc goes back", 40,
           GetScreenHeight() - 36, 18, GRAY);

  EndDrawing();
}

static void ServerBrowserHandleInput(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  ServerBrowserState* browser = &g_server_browser;
  ServerBrowserEntry direct_entry;

  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
    return;
  }
  if (IsKeyPressed(KEY_R)) {
    LoadSampleServers(browser);
    StartLoading(browser, SERVER_BROWSER_RESULT_READY);
    return;
  }
  if (IsKeyPressed(KEY_ENTER) && (browser->host_text_edit_mode || browser->port_text_edit_mode)) {
    if (ParseDirectConnect(browser, &direct_entry)) {
      JoinServerEntry(manager, game, browser, &direct_entry);
    }
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
    JoinServerEntry(manager, game, browser, &browser->servers[browser->selected_index]);
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
