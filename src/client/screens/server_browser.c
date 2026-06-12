#include "game.h"
#include "screen.h"
#include "shared/config.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_BROWSER_MAX_SERVERS 6u
#define SERVER_BROWSER_MAX_RECENTS 5u

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
  ServerBrowserSortKey sort_key;
  bool sort_descending;
  int selected_index;
  int selected_recent_index;
  size_t server_count;
  size_t recent_count;
  char direct_host_input[64];
  char direct_port_input[8];
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

static void SortEntries(ServerBrowserEntry* entries, size_t count) {
  for (size_t outer = 0; outer + 1 < count; ++outer) {
    for (size_t inner = outer + 1; inner < count; ++inner) {
      bool swap = false;
      const ServerBrowserEntry* left = &entries[outer];
      const ServerBrowserEntry* right = &entries[inner];

      switch (g_server_browser.sort_key) {
      case SERVER_BROWSER_SORT_PLAYERS:
        swap = g_server_browser.sort_descending ? (left->player_count < right->player_count)
                                                : (left->player_count > right->player_count);
        break;
      case SERVER_BROWSER_SORT_PING:
        swap = g_server_browser.sort_descending ? (left->ping_ms < right->ping_ms)
                                                : (left->ping_ms > right->ping_ms);
        break;
      case SERVER_BROWSER_SORT_NAME:
      default:
        swap = g_server_browser.sort_descending ? (strcmp(left->name, right->name) < 0)
                                                : (strcmp(left->name, right->name) > 0);
        break;
      }

      if (swap) {
        const ServerBrowserEntry temporary = entries[outer];
        entries[outer] = entries[inner];
        entries[inner] = temporary;
      }
    }
  }
}

static void LoadSampleServers(void) {
  g_server_browser.server_count = 5;

  g_server_browser.servers[0] = (ServerBrowserEntry){"Local Development", "127.0.0.1",    3, 32, 12,
                                                     SHROOM_SERVER_PORT,  "Arena / Local"};
  g_server_browser.servers[1] = (ServerBrowserEntry){
      "Canopy Clash EU", "eu.shroomio.dev", 24, 32, 46, SHROOM_SERVER_PORT, "Arena / Public"};
  g_server_browser.servers[2] = (ServerBrowserEntry){
      "Spore Sprint NA", "na.shroomio.dev", 17, 32, 78, SHROOM_SERVER_PORT, "Arena / Ranked"};
  g_server_browser.servers[3] =
      (ServerBrowserEntry){"Outer Ring Learn", "practice.shroomio.dev", 8, 16, 33,
                           SHROOM_SERVER_PORT, "Practice / Casual"};
  g_server_browser.servers[4] = (ServerBrowserEntry){
      "Center Rush", "rush.shroomio.dev", 29, 32, 61, SHROOM_SERVER_PORT, "Arena / High Risk"};
  SortEntries(g_server_browser.servers, g_server_browser.server_count);
}

static void SaveRecentServers(void) {
  int file_descriptor;
  FILE* file;

  file_descriptor = open(kRecentServersPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (file_descriptor < 0) {
    return;
  }

  file = fdopen(file_descriptor, "w");
  if (file == NULL) {
    close(file_descriptor);
    return;
  }

  for (size_t index = 0; index < g_server_browser.recent_count; ++index) {
    const ServerBrowserEntry* entry = &g_server_browser.recent_servers[index];
    fprintf(file, "%s|%s|%d|%s\n", entry->name, entry->host, entry->port, entry->map_label);
  }

  fclose(file);
}

static void LoadRecentServers(void) {
  FILE* file;
  char line[256];

  g_server_browser.recent_count = 0;
  g_server_browser.selected_recent_index = -1;

  file = fopen(kRecentServersPath, "r");
  if (file == NULL) {
    return;
  }

  while ((g_server_browser.recent_count < SERVER_BROWSER_MAX_RECENTS) &&
         (fgets(line, sizeof(line), file) != NULL)) {
    ServerBrowserEntry* entry = &g_server_browser.recent_servers[g_server_browser.recent_count];
    char* separator;
    char* port_text;
    char* map_label;

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
    g_server_browser.recent_count += 1;
  }

  fclose(file);

  if (g_server_browser.recent_count > 0) {
    g_server_browser.selected_recent_index = 0;
  }
}

static void AddRecentServer(const ServerBrowserEntry* entry) {
  size_t target_index = g_server_browser.recent_count;

  for (size_t index = 0; index < g_server_browser.recent_count; ++index) {
    if ((strcmp(g_server_browser.recent_servers[index].host, entry->host) == 0) &&
        (g_server_browser.recent_servers[index].port == entry->port)) {
      target_index = index;
      break;
    }
  }

  if (target_index >= SERVER_BROWSER_MAX_RECENTS) {
    target_index = SERVER_BROWSER_MAX_RECENTS - 1;
  }

  for (size_t index = target_index; index > 0; --index) {
    g_server_browser.recent_servers[index] = g_server_browser.recent_servers[index - 1];
  }
  g_server_browser.recent_servers[0] = *entry;

  if (g_server_browser.recent_count < SERVER_BROWSER_MAX_RECENTS) {
    g_server_browser.recent_count += 1;
  }
  g_server_browser.selected_recent_index = 0;
  SaveRecentServers();
}

static bool ParseDirectConnect(ServerBrowserEntry* entry) {
  char* end = NULL;
  long port;

  if (g_server_browser.direct_host_input[0] == '\0') {
    snprintf(g_server_browser.validation_message, sizeof(g_server_browser.validation_message), "%s",
             "Host or IP is required.");
    return false;
  }

  port = strtol(g_server_browser.direct_port_input, &end, 10);
  if ((g_server_browser.direct_port_input[0] == '\0') || (end == NULL) || (*end != '\0') ||
      (port < 1) || (port > 65535)) {
    snprintf(g_server_browser.validation_message, sizeof(g_server_browser.validation_message), "%s",
             "Port must be between 1 and 65535.");
    return false;
  }

  *entry = (ServerBrowserEntry){0};
  snprintf(entry->name, sizeof(entry->name), "%.56s:%ld", g_server_browser.direct_host_input, port);
  CopyText(entry->host, sizeof(entry->host), g_server_browser.direct_host_input);
  CopyText(entry->map_label, sizeof(entry->map_label), "Direct Connect");
  entry->port = (int)port;
  g_server_browser.validation_message[0] = '\0';
  return true;
}

static void JoinServer(ShroomScreenManager* manager, Game* game, const ServerBrowserEntry* entry) {
  if (game != NULL) {
    game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    CopyText(game->selected_server_host, sizeof(game->selected_server_host), entry->host);
    game->selected_server_port = (uint16_t)entry->port;
  }

  AddRecentServer(entry);
  ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
}

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  (void)manager;

  g_server_browser = (ServerBrowserState){0};
  g_server_browser.sort_key = SERVER_BROWSER_SORT_PING;
  g_server_browser.selected_index = 0;
  snprintf(g_server_browser.direct_host_input, sizeof(g_server_browser.direct_host_input), "%s",
           "127.0.0.1");
  snprintf(g_server_browser.direct_port_input, sizeof(g_server_browser.direct_port_input), "%u",
           SHROOM_SERVER_PORT);

  LoadRecentServers();
  LoadSampleServers();
  return true;
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  bool join_clicked = false;

  ClearBackground((Color){18, 20, 32, 255});

  ShroomImGui_SetNextWindowPos((float)screen_width * 0.06f, (float)screen_height * 0.08f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize((float)screen_width * 0.88f, (float)screen_height * 0.84f,
                                SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Server Browser", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  if (ShroomImGui_Button("Refresh", 120.0f, 0.0f)) {
    LoadSampleServers();
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Sort Name", 120.0f, 0.0f)) {
    g_server_browser.sort_key = SERVER_BROWSER_SORT_NAME;
    g_server_browser.sort_descending = !g_server_browser.sort_descending;
    SortEntries(g_server_browser.servers, g_server_browser.server_count);
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Sort Players", 120.0f, 0.0f)) {
    g_server_browser.sort_key = SERVER_BROWSER_SORT_PLAYERS;
    g_server_browser.sort_descending = !g_server_browser.sort_descending;
    SortEntries(g_server_browser.servers, g_server_browser.server_count);
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Sort Ping", 120.0f, 0.0f)) {
    g_server_browser.sort_key = SERVER_BROWSER_SORT_PING;
    g_server_browser.sort_descending = !g_server_browser.sort_descending;
    SortEntries(g_server_browser.servers, g_server_browser.server_count);
  }

  if (ShroomImGui_BeginTable("servers", 4,
                             SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SCROLL_Y | SHROOM_IMGUI_TABLE_SIZING_STRETCH,
                             0.0f, 260.0f)) {
    ShroomImGui_TableSetupColumn("Name", 0.0f);
    ShroomImGui_TableSetupColumn("Players", 0.0f);
    ShroomImGui_TableSetupColumn("Ping", 0.0f);
    ShroomImGui_TableSetupColumn("Map", 0.0f);
    ShroomImGui_TableHeadersRow();

    for (size_t index = 0; index < g_server_browser.server_count; ++index) {
      const ServerBrowserEntry* entry = &g_server_browser.servers[index];

      ShroomImGui_TableNextRow();
      ShroomImGui_TableSetColumnIndex(0);
      if (ShroomImGui_Selectable(entry->name, g_server_browser.selected_index == (int)index,
                                 SHROOM_IMGUI_SELECTABLE_SPAN_ALL_COLUMNS, 0.0f, 0.0f)) {
        g_server_browser.selected_index = (int)index;
      }
      ShroomImGui_TableSetColumnIndex(1);
      ShroomImGui_Text(TextFormat("%d/%d", entry->player_count, entry->player_capacity));
      ShroomImGui_TableSetColumnIndex(2);
      ShroomImGui_Text(TextFormat("%d ms", entry->ping_ms));
      ShroomImGui_TableSetColumnIndex(3);
      ShroomImGui_Text(entry->map_label);
    }

    ShroomImGui_EndTable();
  }

  if ((g_server_browser.selected_index >= 0) &&
      ((size_t)g_server_browser.selected_index < g_server_browser.server_count)) {
    const ServerBrowserEntry* selected = &g_server_browser.servers[g_server_browser.selected_index];
    ShroomImGui_Text(
        TextFormat("Selected: %s (%s:%d)", selected->name, selected->host, selected->port));
  }

  if (ShroomImGui_Button("Join Selected", 150.0f, 0.0f)) {
    join_clicked = true;
  }
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Back", 120.0f, 0.0f)) {
    ShroomScreenManagerGoBack(manager);
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Direct Connect");
  ShroomImGui_SetNextItemWidth(260.0f);
  ShroomImGui_InputText("Host", g_server_browser.direct_host_input,
                        sizeof(g_server_browser.direct_host_input));
  ShroomImGui_SetNextItemWidth(140.0f);
  ShroomImGui_InputText("Port", g_server_browser.direct_port_input,
                        sizeof(g_server_browser.direct_port_input));
  if (ShroomImGui_Button("Join Host", 150.0f, 0.0f)) {
    ServerBrowserEntry direct_entry;

    if (ParseDirectConnect(&direct_entry)) {
      JoinServer(manager, game, &direct_entry);
    }
  }

  if (g_server_browser.validation_message[0] != '\0') {
    ShroomImGui_Text(g_server_browser.validation_message);
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Recent Servers");
  if (ShroomImGui_BeginTable("recent", 3,
                             SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SIZING_STRETCH,
                             0.0f, 0.0f)) {
    ShroomImGui_TableSetupColumn("Server", 0.0f);
    ShroomImGui_TableSetupColumn("Address", 0.0f);
    ShroomImGui_TableSetupColumn("Type", 0.0f);
    ShroomImGui_TableHeadersRow();

    for (size_t index = 0; index < g_server_browser.recent_count; ++index) {
      const ServerBrowserEntry* entry = &g_server_browser.recent_servers[index];

      ShroomImGui_TableNextRow();
      ShroomImGui_TableSetColumnIndex(0);
      if (ShroomImGui_Selectable(entry->name, g_server_browser.selected_recent_index == (int)index,
                                 SHROOM_IMGUI_SELECTABLE_SPAN_ALL_COLUMNS, 0.0f, 0.0f)) {
        g_server_browser.selected_recent_index = (int)index;
      }
      ShroomImGui_TableSetColumnIndex(1);
      ShroomImGui_Text(TextFormat("%s:%d", entry->host, entry->port));
      ShroomImGui_TableSetColumnIndex(2);
      ShroomImGui_Text(entry->map_label);
    }

    ShroomImGui_EndTable();
  }

  if ((g_server_browser.selected_recent_index >= 0) &&
      ((size_t)g_server_browser.selected_recent_index < g_server_browser.recent_count) &&
      ShroomImGui_Button("Join Recent", 150.0f, 0.0f)) {
    JoinServer(manager, game,
               &g_server_browser.recent_servers[g_server_browser.selected_recent_index]);
  }

  ShroomImGui_End();

  if (join_clicked && (g_server_browser.selected_index >= 0) &&
      ((size_t)g_server_browser.selected_index < g_server_browser.server_count)) {
    JoinServer(manager, game, &g_server_browser.servers[g_server_browser.selected_index]);
  }
}

static void ServerBrowserHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
    return;
  }

  if ((g_server_browser.selected_index > 0) && IsKeyPressed(KEY_UP)) {
    g_server_browser.selected_index -= 1;
  }
  if (((size_t)(g_server_browser.selected_index + 1) < g_server_browser.server_count) &&
      IsKeyPressed(KEY_DOWN)) {
    g_server_browser.selected_index += 1;
  }
}

void ShroomScreenRegisterServerBrowser(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_SERVER_BROWSER];
  screen->type = SHROOM_SCREEN_SERVER_BROWSER;
  screen->name = "Server Browser";
  screen->init = ServerBrowserInit;
  screen->draw = ServerBrowserDraw;
  screen->handle_input = ServerBrowserHandleInput;
}
