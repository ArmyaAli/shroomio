#include "game.h"
#include "screen.h"
#include "screen_background.h"
#include "server_browser_model.h"
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
#define SERVER_BROWSER_REFRESH_DELAY 0.35f
#define SERVER_BROWSER_STALE_SECONDS 30.0f

typedef enum ServerBrowserType {
  SERVER_BROWSER_TYPE_OFFICIAL = 0,
  SERVER_BROWSER_TYPE_SELF_HOSTED,
  SERVER_BROWSER_TYPE_DEMO,
} ServerBrowserType;

typedef struct ServerBrowserEntry {
  char name[64];
  char host[64];
  int player_count;
  int player_capacity;
  int ping_ms;
  int port;
  char map_label[64];
  ServerBrowserType type;
  bool metadata_known;
  bool reachable;
} ServerBrowserEntry;

typedef struct ServerBrowserState {
  ShroomServerBrowserModel model;
  int selected_index;
  int selected_recent_index;
  size_t server_count;
  size_t recent_count;
  char direct_host_input[64];
  char direct_port_input[8];
  char validation_message[128];
  char connection_error[128];
  float refresh_elapsed;
  float result_age;
  bool demo_mode;
  ServerBrowserEntry servers[SERVER_BROWSER_MAX_SERVERS];
  ServerBrowserEntry recent_servers[SERVER_BROWSER_MAX_RECENTS];
} ServerBrowserState;

static const char* kRecentServersPath = "server_browser_recent.txt";
static const char* const kSortItems[] = {"Name", "Players", "Ping"};
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

      switch (g_server_browser.model.sort_key) {
      case SHROOM_SERVER_SORT_PLAYERS:
        swap = g_server_browser.model.sort_descending ? (left->player_count < right->player_count)
                                                      : (left->player_count > right->player_count);
        break;
      case SHROOM_SERVER_SORT_PING:
        swap = g_server_browser.model.sort_descending ? (left->ping_ms < right->ping_ms)
                                                      : (left->ping_ms > right->ping_ms);
        break;
      case SHROOM_SERVER_SORT_NAME:
      default:
        swap = g_server_browser.model.sort_descending ? (strcmp(left->name, right->name) < 0)
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

static int FindServerIndex(const char* host, int port) {
  for (size_t index = 0; index < g_server_browser.server_count; ++index) {
    const ServerBrowserEntry* entry = &g_server_browser.servers[index];

    if ((strcmp(entry->host, host) == 0) && (entry->port == port)) {
      return (int)index;
    }
  }

  return g_server_browser.server_count > 0u ? 0 : -1;
}

static void SortServersPreservingSelection(void) {
  char selected_host[sizeof(g_server_browser.servers[0].host)] = {0};
  int selected_port = 0;

  if ((g_server_browser.selected_index >= 0) &&
      ((size_t)g_server_browser.selected_index < g_server_browser.server_count)) {
    const ServerBrowserEntry* selected = &g_server_browser.servers[g_server_browser.selected_index];
    CopyText(selected_host, sizeof(selected_host), selected->host);
    selected_port = selected->port;
  }

  SortEntries(g_server_browser.servers, g_server_browser.server_count);

  if (selected_host[0] != '\0') {
    g_server_browser.selected_index = FindServerIndex(selected_host, selected_port);
  } else if (g_server_browser.server_count > 0u) {
    g_server_browser.selected_index = 0;
  } else {
    g_server_browser.selected_index = -1;
  }
}

static const ServerBrowserEntry* GetSelectedServer(void) {
  if ((g_server_browser.selected_index < 0) ||
      ((size_t)g_server_browser.selected_index >= g_server_browser.server_count)) {
    return NULL;
  }

  return &g_server_browser.servers[g_server_browser.selected_index];
}

static bool ServerBrowserEntryIsJoinable(const ServerBrowserEntry* entry) {
  if ((entry == NULL) || !entry->metadata_known || !entry->reachable) {
    return false;
  }
  if (entry->player_capacity <= 0) {
    return false;
  }

  return entry->player_count < entry->player_capacity;
}

static void LoadDemoServers(void) {
  g_server_browser.server_count = 5;

  g_server_browser.servers[0] = (ServerBrowserEntry){
      "Local Development", "local.demo.invalid",     3,    32,   12, SHROOM_SERVER_PORT,
      "Demo / Arena",      SERVER_BROWSER_TYPE_DEMO, true, false};
  g_server_browser.servers[1] = (ServerBrowserEntry){
      "Canopy Clash EU", "eu.demo.invalid",        24,   32,   46, SHROOM_SERVER_PORT,
      "Demo / Arena",    SERVER_BROWSER_TYPE_DEMO, true, false};
  g_server_browser.servers[2] = (ServerBrowserEntry){
      "Spore Sprint NA", "na.demo.invalid",        17,   32,   78, SHROOM_SERVER_PORT,
      "Demo / Ranked",   SERVER_BROWSER_TYPE_DEMO, true, false};
  g_server_browser.servers[3] = (ServerBrowserEntry){
      "Outer Ring Learn", "practice.demo.invalid",  8,    16,   33, SHROOM_SERVER_PORT,
      "Demo / Practice",  SERVER_BROWSER_TYPE_DEMO, true, false};
  g_server_browser.servers[4] = (ServerBrowserEntry){
      "Center Rush",      "rush.demo.invalid",      32,   32,   61, SHROOM_SERVER_PORT,
      "Demo / High Risk", SERVER_BROWSER_TYPE_DEMO, true, false};
  SortServersPreservingSelection();
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
    fprintf(file, "%s|%s|%d|%s|%d\n", entry->name, entry->host, entry->port, entry->map_label,
            (int)entry->type);
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
    char* next_separator;

    line[strcspn(line, "\r\n")] = '\0';
    separator = strchr(line, '|');
    if (separator == NULL) {
      continue;
    }
    *separator = '\0';
    CopyText(entry->name, sizeof(entry->name), line);

    {
      const char* port_text = separator + 1;

      next_separator = strchr(port_text, '|');
      if (next_separator == NULL) {
        continue;
      }
      *next_separator = '\0';
      CopyText(entry->host, sizeof(entry->host), port_text);
    }

    {
      const char* port_value = next_separator + 1;
      char* const map_text = strchr(port_value, '|');
      char* type_text;

      if (map_text == NULL) {
        continue;
      }
      *map_text = '\0';
      entry->port = atoi(port_value);

      type_text = strchr(map_text + 1, '|');
      if (type_text != NULL) {
        *type_text = '\0';
        CopyText(entry->map_label, sizeof(entry->map_label), map_text + 1);
        entry->type = (ServerBrowserType)atoi(type_text + 1);
      } else {
        CopyText(entry->map_label, sizeof(entry->map_label), map_text + 1);
        entry->type = SERVER_BROWSER_TYPE_SELF_HOSTED;
      }
    }

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

static bool IsValidHostname(const char* host) {
  size_t length;
  size_t label_start = 0;
  bool has_dot = false;

  if ((host == NULL) || (host[0] == '\0')) {
    return false;
  }

  length = strnlen(host, 256);
  if ((length == 0u) || (length > 253u)) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    const char c = host[i];

    if (c == '.') {
      if ((i == label_start) || (i == length - 1u)) {
        return false;
      }
      has_dot = true;
      label_start = i + 1;
    } else if (c == ':') {
      break;
    } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                 (c == '-') || (c == '_'))) {
      return false;
    }
  }

  (void)has_dot;
  return true;
}

static bool ParseDirectConnect(ServerBrowserEntry* entry) {
  char* end = NULL;
  long port;
  char host_buffer[64];
  char* colon_position;

  if (g_server_browser.direct_host_input[0] == '\0') {
    snprintf(g_server_browser.validation_message, sizeof(g_server_browser.validation_message), "%s",
             "Host or IP is required.");
    return false;
  }

  CopyText(host_buffer, sizeof(host_buffer), g_server_browser.direct_host_input);
  colon_position = strrchr(host_buffer, ':');

  if (colon_position != NULL) {
    const char* port_text = colon_position + 1;
    *colon_position = '\0';
    CopyText(g_server_browser.direct_host_input, sizeof(g_server_browser.direct_host_input),
             host_buffer);
    CopyText(g_server_browser.direct_port_input, sizeof(g_server_browser.direct_port_input),
             port_text);
  }

  if (!IsValidHostname(g_server_browser.direct_host_input)) {
    snprintf(g_server_browser.validation_message, sizeof(g_server_browser.validation_message), "%s",
             "Invalid hostname or IP address.");
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
  entry->type = SERVER_BROWSER_TYPE_SELF_HOSTED;
  g_server_browser.validation_message[0] = '\0';
  return true;
}

static void JoinServer(ShroomScreenManager* manager, Game* game, const ServerBrowserEntry* entry) {
  if (game != NULL) {
    CopyText(game->selected_server_host, sizeof(game->selected_server_host), entry->host);
    game->selected_server_port = (uint16_t)entry->port;
    /* Establish connection now; lobby browser will wait for handshake. */
    ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
  }

  AddRecentServer(entry);
  ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
}

static void BeginDiscoveryRefresh(void) {
  ShroomServerBrowserBeginRefresh(&g_server_browser.model);
  g_server_browser.refresh_elapsed = 0.0f;
}

static void FinishDiscoveryRefresh(void) {
  g_server_browser.server_count = 0u;
  g_server_browser.selected_index = -1;
  g_server_browser.result_age = 0.0f;

  if (g_server_browser.demo_mode) {
    LoadDemoServers();
    ShroomServerBrowserFinishRefresh(&g_server_browser.model, true, g_server_browser.server_count);
  } else {
    ShroomServerBrowserFinishRefresh(&g_server_browser.model, false, 0u);
  }
}

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;
  const char* demo_mode = getenv("SHROOM_SERVER_BROWSER_DEMO");

  g_server_browser = (ServerBrowserState){0};
  ShroomServerBrowserModelInit(&g_server_browser.model);
  g_server_browser.selected_index = -1;
  g_server_browser.demo_mode = (demo_mode != NULL) && (strcmp(demo_mode, "1") == 0);
  snprintf(g_server_browser.direct_host_input, sizeof(g_server_browser.direct_host_input), "%s",
           "127.0.0.1");
  snprintf(g_server_browser.direct_port_input, sizeof(g_server_browser.direct_port_input), "%u",
           SHROOM_SERVER_PORT);

  if (game != NULL && game->net.status == CLIENT_NET_ERROR && game->net.status_text[0] != '\0') {
    snprintf(g_server_browser.connection_error, sizeof(g_server_browser.connection_error), "%s",
             game->net.status_text);
  }

  LoadRecentServers();
  return true;
}

static void ServerBrowserUpdate(ShroomScreenManager* manager, float delta_time) {
  (void)manager;

  if (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_LOADING) {
    g_server_browser.refresh_elapsed += delta_time;
    if (g_server_browser.refresh_elapsed >= SERVER_BROWSER_REFRESH_DELAY) {
      FinishDiscoveryRefresh();
    }
    return;
  }

  if ((g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_READY) ||
      (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_STALE)) {
    g_server_browser.result_age += delta_time;
    if (g_server_browser.result_age >= SERVER_BROWSER_STALE_SECONDS) {
      ShroomServerBrowserMarkStale(&g_server_browser.model);
    }
  }
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  bool join_clicked = false;

  ShroomScreenDrawFungalBackground((game == NULL) || game->settings.menu_animations_enabled);

  ShroomImGui_SetNextWindowPos((float)screen_width * 0.06f, (float)screen_height * 0.08f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize((float)screen_width * 0.88f, (float)screen_height * 0.84f,
                                SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(0.88f);
  if (!ShroomImGui_Begin("Server Browser", NULL,
                         SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                             SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                             SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

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
  ShroomImGui_SameLine();
  if (ShroomImGui_Button("Back", 120.0f, 0.0f)) {
    ShroomScreenManagerGoBack(manager);
  }
  if (g_server_browser.validation_message[0] != '\0') {
    ShroomImGui_Text(g_server_browser.validation_message);
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Server Discovery");
  ShroomImGui_BeginDisabled(g_server_browser.model.discovery_state ==
                            SHROOM_SERVER_DISCOVERY_LOADING);
  if (ShroomImGui_Button("Refresh", 120.0f, 0.0f)) {
    BeginDiscoveryRefresh();
  }
  ShroomImGui_EndDisabled();

  ShroomImGui_SameLine();
  switch (g_server_browser.model.discovery_state) {
  case SHROOM_SERVER_DISCOVERY_LOADING:
    ShroomImGui_Text("Loading server directory...");
    break;
  case SHROOM_SERVER_DISCOVERY_READY:
    ShroomImGui_Text("Demo results loaded; counts and ping are illustrative, not live.");
    break;
  case SHROOM_SERVER_DISCOVERY_FAILED:
    ShroomImGui_Text("Refresh failed: no server directory is configured. Use Direct Connect.");
    break;
  case SHROOM_SERVER_DISCOVERY_STALE:
    ShroomImGui_Text("Demo results are stale. Refresh before relying on them.");
    break;
  case SHROOM_SERVER_DISCOVERY_EMPTY:
  default:
    ShroomImGui_Text("No discovery results. Use Direct Connect or a recent server.");
    break;
  }

  if ((g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_READY) ||
      (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_STALE)) {
    ShroomImGui_Text(TextFormat("Source: development demo data | Refreshed %.0f seconds ago",
                                g_server_browser.result_age));
  } else {
    ShroomImGui_Text("Source: none | Last refresh: never completed");
  }

  if (g_server_browser.connection_error[0] != '\0') {
    ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.4f, 0.4f, 1.0f},
                            TextFormat("Connection failed: %s", g_server_browser.connection_error));
  }

  {
    int sort_index = (int)g_server_browser.model.sort_key;
    if (ShroomImGui_Combo("Sort by", &sort_index, kSortItems, 3)) {
      ShroomServerBrowserSetSort(&g_server_browser.model, (ShroomServerSortKey)sort_index);
      SortServersPreservingSelection();
    }
    ShroomImGui_SameLine();
    if (ShroomImGui_Button(g_server_browser.model.sort_descending ? "Descending" : "Ascending",
                           120.0f, 0.0f)) {
      ShroomServerBrowserSetSort(&g_server_browser.model, g_server_browser.model.sort_key);
      SortServersPreservingSelection();
    }
    ShroomImGui_SameLine();
    ShroomImGui_Text(
        TextFormat("Sort: %s / %s", ShroomServerBrowserSortLabel(g_server_browser.model.sort_key),
                   g_server_browser.model.sort_descending ? "Descending" : "Ascending"));
  }

  if (g_server_browser.server_count == 0u) {
    g_server_browser.selected_index = -1;
  } else if (g_server_browser.selected_index < 0) {
    g_server_browser.selected_index = 0;
  }

  if (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_STALE) {
    ShroomImGui_Text("Join is disabled for stale discovery data.");
  }

  if (ShroomImGui_BeginTable("servers", 5,
                             SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SCROLL_Y | SHROOM_IMGUI_TABLE_SIZING_STRETCH,
                             0.0f, 180.0f)) {
    ShroomImGui_TableSetupColumn("Name", 0.0f);
    ShroomImGui_TableSetupColumn("Players", 0.0f);
    ShroomImGui_TableSetupColumn("Ping", 0.0f);
    ShroomImGui_TableSetupColumn("Map", 0.0f);
    ShroomImGui_TableSetupColumn("Type", 0.0f);
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
      ShroomImGui_Text(entry->metadata_known
                           ? TextFormat("%d/%d", entry->player_count, entry->player_capacity)
                           : "Unknown");
      ShroomImGui_TableSetColumnIndex(2);
      ShroomImGui_Text(entry->metadata_known ? TextFormat("%d ms", entry->ping_ms) : "Unknown");
      ShroomImGui_TableSetColumnIndex(3);
      ShroomImGui_Text(entry->map_label);
      ShroomImGui_TableSetColumnIndex(4);
      ShroomImGui_Text(
          entry->type == SERVER_BROWSER_TYPE_DEMO
              ? "Demo (not live)"
              : (entry->type == SERVER_BROWSER_TYPE_OFFICIAL ? "Official" : "Community"));
    }

    ShroomImGui_EndTable();
  }

  {
    const ServerBrowserEntry* selected = GetSelectedServer();
    const bool can_join = g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_READY &&
                          ServerBrowserEntryIsJoinable(selected);

    ShroomImGui_Separator();
    ShroomImGui_Text("Selected Server");
    if (selected != NULL) {
      ShroomImGui_Text(TextFormat("Name: %s", selected->name));
      ShroomImGui_Text(TextFormat("Address: %s:%d", selected->host, selected->port));
      if (selected->metadata_known) {
        ShroomImGui_Text(
            TextFormat("Players: %d/%d", selected->player_count, selected->player_capacity));
        ShroomImGui_Text(TextFormat("Ping: %d ms", selected->ping_ms));
      } else {
        ShroomImGui_Text("Players: unknown");
        ShroomImGui_Text("Ping: unknown");
      }
      ShroomImGui_Text(TextFormat("Mode: %s", selected->map_label));
      if (!can_join) {
        if (selected->type == SERVER_BROWSER_TYPE_DEMO) {
          ShroomImGui_Text("Join unavailable: demo rows are not live endpoints.");
        } else if (!selected->metadata_known) {
          ShroomImGui_Text("Join unavailable: reachability and capacity are unknown.");
        } else if (selected->player_count >= selected->player_capacity) {
          ShroomImGui_Text("Join unavailable: server is full.");
        } else {
          ShroomImGui_Text("Join unavailable: server is unreachable or data is stale.");
        }
      }
    } else {
      ShroomImGui_Text("No server selected.");
    }

    ShroomImGui_BeginDisabled(!can_join);
    if (ShroomImGui_Button("Join Selected", 150.0f, 0.0f) && can_join) {
      join_clicked = true;
    }
    ShroomImGui_EndDisabled();
  }

  ShroomImGui_Separator();
  ShroomImGui_Text("Recent Servers");
  ShroomImGui_Text("Reachability, player capacity, and ping are unknown until connected.");
  if (ShroomImGui_BeginTable("recent", 4,
                             SHROOM_IMGUI_TABLE_BORDERS | SHROOM_IMGUI_TABLE_ROW_BG |
                                 SHROOM_IMGUI_TABLE_SIZING_STRETCH,
                             0.0f, 0.0f)) {
    ShroomImGui_TableSetupColumn("Server", 0.0f);
    ShroomImGui_TableSetupColumn("Address", 0.0f);
    ShroomImGui_TableSetupColumn("Mode", 0.0f);
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
      ShroomImGui_TableSetColumnIndex(3);
      ShroomImGui_Text(entry->type == SERVER_BROWSER_TYPE_OFFICIAL ? "Official" : "Community");
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

  if (join_clicked && ServerBrowserEntryIsJoinable(GetSelectedServer())) {
    JoinServer(manager, game, GetSelectedServer());
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
  screen->update = ServerBrowserUpdate;
  screen->draw = ServerBrowserDraw;
  screen->handle_input = ServerBrowserHandleInput;
}

#ifdef TEST_MODE
const char* ShroomTestGetServerBrowserValidationMessage(void) {
  return g_server_browser.validation_message;
}

int ShroomTestGetServerBrowserSelectedIndex(void) { return g_server_browser.selected_index; }

const char* ShroomTestGetServerBrowserSelectedHost(void) {
  const ServerBrowserEntry* selected = GetSelectedServer();

  return selected != NULL ? selected->host : "";
}

int ShroomTestGetServerBrowserRecentCount(void) { return (int)g_server_browser.recent_count; }

int ShroomTestGetServerBrowserDiscoveryState(void) {
  return (int)g_server_browser.model.discovery_state;
}

int ShroomTestGetServerBrowserServerCount(void) { return (int)g_server_browser.server_count; }

bool ShroomTestGetServerBrowserSortDescending(void) {
  return g_server_browser.model.sort_descending;
}

void ShroomTestMarkServerBrowserStale(void) {
  ShroomServerBrowserMarkStale(&g_server_browser.model);
}

void ShroomTestCompleteServerBrowserRefresh(bool demo_mode) {
  g_server_browser.demo_mode = demo_mode;
  FinishDiscoveryRefresh();
}
#endif
