#include "game.h"
#include "client_paths.h"
#include "client_storage.h"
#include "layout.h"
#include "matchmaking_selector.h"
#include "screen.h"
#include "screen_background.h"
#include "server_browser_model.h"
#include "server_discovery.h"
#include "shared/config.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <io.h>
#endif

#define SERVER_BROWSER_MAX_SERVERS SHROOM_DIRECTORY_MAX_ENTRIES
#define SERVER_BROWSER_MAX_RECENTS 5u
#define SERVER_BROWSER_STALE_SECONDS 30.0f
#define SERVER_BROWSER_RECENTS_FILENAME "server_browser_recent.txt"
#define SERVER_BROWSER_RECENTS_LEGACY_PATH "server_browser_recent.txt"
#define SERVER_BROWSER_RECENTS_MAX_FILE_BYTES (64u * 1024u)

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
  ShroomServerDiscovery discovery;
  int selected_index;
  int selected_recent_index;
  size_t server_count;
  size_t recent_count;
  char direct_host_input[64];
  char direct_port_input[8];
  char validation_message[128];
  char connection_error[128];
  float result_age;
  bool demo_mode;
  bool directory_configured;
  bool has_recommendation;
  bool high_latency_recommendation;
  char recommended_host[64];
  uint16_t recommended_port;
  char directory_host[SHROOM_DIRECTORY_HOST_LENGTH];
  char recent_path[SHROOM_CLIENT_PATH_MAX];
  uint16_t directory_port;
  ServerBrowserEntry servers[SERVER_BROWSER_MAX_SERVERS];
  ServerBrowserEntry recent_servers[SERVER_BROWSER_MAX_RECENTS];
} ServerBrowserState;

static const char* const kSortItems[] = {"Name", "Players", "Ping"};
static ServerBrowserState g_server_browser;

static const char* DiscoveryStatusText(void) {
  switch (g_server_browser.model.discovery_state) {
  case SHROOM_SERVER_DISCOVERY_LOADING:
    return g_server_browser.discovery.state.phase == SHROOM_DISCOVERY_PROBING
               ? "Measuring live server latency..."
               : "Loading server directory...";
  case SHROOM_SERVER_DISCOVERY_READY:
    return "Live server results loaded.";
  case SHROOM_SERVER_DISCOVERY_FAILED:
    return g_server_browser.directory_configured
               ? "Server directory unavailable. Use Direct Connect or try again."
               : "Refresh failed: no server directory is configured. Use Direct Connect.";
  case SHROOM_SERVER_DISCOVERY_STALE:
    return "Server results are stale. Refresh before relying on them.";
  case SHROOM_SERVER_DISCOVERY_CANCELLED:
    return "Refresh cancelled.";
  case SHROOM_SERVER_DISCOVERY_EMPTY:
  default:
    return g_server_browser.directory_configured
               ? "The server directory is empty. Use Direct Connect or try again later."
               : "No discovery results. Use Direct Connect or a recent server.";
  }
}

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

static const ServerBrowserEntry* GetRecommendedServer(void) {
  if (!g_server_browser.has_recommendation) {
    return NULL;
  }
  for (size_t index = 0u; index < g_server_browser.server_count; ++index) {
    const ServerBrowserEntry* entry = &g_server_browser.servers[index];
    if ((entry->port == (int)g_server_browser.recommended_port) &&
        (strcmp(entry->host, g_server_browser.recommended_host) == 0)) {
      return entry;
    }
  }
  return NULL;
}

static const char* MatchmakingRecommendationText(void) {
  static char text[192];
  const ServerBrowserEntry* recommended = GetRecommendedServer();

  if (recommended == NULL) {
    return "No joinable live server is available for matchmaking.";
  }
  snprintf(text, sizeof(text), "Recommended: %s (%d ms, %d/%d players)", recommended->name,
           recommended->ping_ms, recommended->player_count, recommended->player_capacity);
  return text;
}

static void UpdateMatchmakingRecommendation(void) {
  ShroomMatchmakingCandidate candidates[SERVER_BROWSER_MAX_SERVERS];
  ShroomMatchmakingSelection selection;

  g_server_browser.has_recommendation = false;
  g_server_browser.high_latency_recommendation = false;
  g_server_browser.recommended_host[0] = '\0';
  g_server_browser.recommended_port = 0u;
  for (size_t index = 0u; index < g_server_browser.server_count; ++index) {
    const ServerBrowserEntry* entry = &g_server_browser.servers[index];
    candidates[index] = (ShroomMatchmakingCandidate){
        .host = entry->host,
        .port = entry->port > 0 ? (uint16_t)entry->port : 0u,
        .latency_ms = entry->ping_ms >= 0 ? (uint16_t)entry->ping_ms : 0u,
        .player_count = entry->player_count >= 0 ? (uint16_t)entry->player_count : 0u,
        .capacity = entry->player_capacity > 0 ? (uint16_t)entry->player_capacity : 0u,
        .reachable = entry->metadata_known && entry->reachable,
    };
  }
  if (ShroomMatchmakingSelect(candidates, g_server_browser.server_count,
                              ShroomMatchmakingDefaultWeights(), &selection)) {
    const ServerBrowserEntry* entry = &g_server_browser.servers[selection.candidate_index];
    g_server_browser.has_recommendation = true;
    g_server_browser.high_latency_recommendation = selection.high_latency_fallback;
    CopyText(g_server_browser.recommended_host, sizeof(g_server_browser.recommended_host),
             entry->host);
    g_server_browser.recommended_port = (uint16_t)entry->port;
  }
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

#ifdef TEST_MODE
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
#endif

static void SaveRecentServers(void) {
  char temporary_path[SHROOM_CLIENT_PATH_MAX + 64u];
  int file_descriptor;
  FILE* file;
  bool write_failed = false;

  if (g_server_browser.recent_path[0] == '\0') {
    return;
  }
  if (!ShroomClientStorageCreatePrivateTemporaryFile(g_server_browser.recent_path, temporary_path,
                                                     sizeof(temporary_path), &file_descriptor)) {
    return;
  }
  file =
#ifdef _WIN32
      _fdopen(file_descriptor, "w");
#else
      fdopen(file_descriptor, "w");
#endif
  if (file == NULL) {
#ifdef _WIN32
    _close(file_descriptor);
#else
    close(file_descriptor);
#endif
    remove(temporary_path);
    return;
  }

  for (size_t index = 0; index < g_server_browser.recent_count; ++index) {
    const ServerBrowserEntry* entry = &g_server_browser.recent_servers[index];
    if (fprintf(file, "%s|%s|%d|%s|%d\n", entry->name, entry->host, entry->port, entry->map_label,
                (int)entry->type) < 0) {
      write_failed = true;
      break;
    }
  }
  if (!write_failed && (fflush(file) != 0)) {
    write_failed = true;
  }
  if (!write_failed) {
#ifdef _WIN32
    write_failed = _commit(file_descriptor) != 0;
#else
    write_failed = fsync(file_descriptor) != 0;
#endif
  }
  if (fclose(file) != 0) {
    write_failed = true;
  }
  if (write_failed ||
      !ShroomClientStorageReplaceFile(temporary_path, g_server_browser.recent_path)) {
    remove(temporary_path);
  }
}

static bool ParseRecentServersFile(const char* path, ServerBrowserEntry* entries, size_t capacity,
                                   size_t* entry_count) {
  FILE* file;
  char line[256];
  size_t count = 0u;

  if ((path == NULL) || (entry_count == NULL)) {
    return false;
  }
  *entry_count = 0u;
  file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  while (fgets(line, sizeof(line), file) != NULL) {
    ServerBrowserEntry entry = {0};
    char* fields[5];
    char* cursor = line;
    char* end = NULL;
    long port;
    long type = SERVER_BROWSER_TYPE_SELF_HOSTED;

    if ((strchr(line, '\n') == NULL) || (count >= capacity)) {
      fclose(file);
      return false;
    }
    line[strcspn(line, "\r\n")] = '\0';
    for (size_t field = 0u; field < 5u; ++field) {
      fields[field] = cursor;
      if (field < 4u) {
        cursor = strchr(cursor, '|');
        if (cursor == NULL) {
          if (field == 3u) {
            fields[4] = NULL;
            break;
          }
          fclose(file);
          return false;
        }
        *cursor++ = '\0';
      }
    }
    if ((fields[0][0] == '\0') || (fields[1][0] == '\0') || (fields[2][0] == '\0') ||
        (fields[3][0] == '\0') || (strlen(fields[0]) >= sizeof(entry.name)) ||
        (strlen(fields[1]) >= sizeof(entry.host)) ||
        (strlen(fields[3]) >= sizeof(entry.map_label)) ||
        ((fields[4] != NULL) && (strchr(fields[4], '|') != NULL))) {
      fclose(file);
      return false;
    }
    errno = 0;
    port = strtol(fields[2], &end, 10);
    if ((errno == ERANGE) || (end == fields[2]) || (*end != '\0') || (port < 1) ||
        (port > UINT16_MAX)) {
      fclose(file);
      return false;
    }
    if (fields[4] != NULL) {
      errno = 0;
      type = strtol(fields[4], &end, 10);
      if ((errno == ERANGE) || (end == fields[4]) || (*end != '\0') ||
          (type < SERVER_BROWSER_TYPE_OFFICIAL) || (type > SERVER_BROWSER_TYPE_DEMO)) {
        fclose(file);
        return false;
      }
    }
    CopyText(entry.name, sizeof(entry.name), fields[0]);
    CopyText(entry.host, sizeof(entry.host), fields[1]);
    CopyText(entry.map_label, sizeof(entry.map_label), fields[3]);
    entry.port = (int)port;
    entry.type = (ServerBrowserType)type;
    if (entries != NULL) {
      entries[count] = entry;
    }
    ++count;
  }
  if (ferror(file)) {
    fclose(file);
    return false;
  }
  fclose(file);
  *entry_count = count;
  return true;
}

static bool ValidateRecentServersFile(const char* path, const void* context) {
  size_t count;

  (void)context;
  return ParseRecentServersFile(path, NULL, SERVER_BROWSER_MAX_RECENTS, &count);
}

static void LoadRecentServers(void) {
  size_t count = 0u;

  g_server_browser.recent_count = 0;
  g_server_browser.selected_recent_index = -1;
  if ((g_server_browser.recent_path[0] != '\0') &&
      ParseRecentServersFile(g_server_browser.recent_path, g_server_browser.recent_servers,
                             SERVER_BROWSER_MAX_RECENTS, &count)) {
    g_server_browser.recent_count = count;
  }
  if (count > 0u) {
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
  ShroomServerDiscoveryShutdown(&g_server_browser.discovery);
  if (game != NULL) {
    CopyText(game->selected_server_host, sizeof(game->selected_server_host), entry->host);
    game->selected_server_port = (uint16_t)entry->port;
    /* Establish connection now; lobby browser will wait for handshake. */
    ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port,
                  game->settings.player_name);
  }

  AddRecentServer(entry);
  ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
}

static void BeginDiscoveryRefresh(Game* game) {
#ifdef TEST_MODE
  (void)game;
#endif
  ShroomServerBrowserBeginRefresh(&g_server_browser.model);
  g_server_browser.server_count = 0u;
  g_server_browser.selected_index = -1;
  g_server_browser.result_age = 0.0f;
  UpdateMatchmakingRecommendation();
#ifndef TEST_MODE
  if (!ShroomServerDiscoveryBegin(&g_server_browser.discovery, g_server_browser.directory_host,
                                  g_server_browser.directory_port, enet_time_get())) {
    ShroomServerBrowserFinishRefresh(&g_server_browser.model, false, 0u);
    if (game != NULL) {
      ShroomQuickMatchDirectoryFailed(&game->quick_match);
    }
  }
#endif
}

#ifdef TEST_MODE
static void FinishDiscoveryRefresh(void) {
  ShroomServerDiscoveryShutdown(&g_server_browser.discovery);
  g_server_browser.server_count = 0u;
  g_server_browser.selected_index = -1;
  g_server_browser.result_age = 0.0f;

  if (g_server_browser.demo_mode) {
    LoadDemoServers();
    UpdateMatchmakingRecommendation();
    ShroomServerBrowserFinishRefresh(&g_server_browser.model, true, g_server_browser.server_count);
  } else if (g_server_browser.directory_configured) {
    UpdateMatchmakingRecommendation();
    ShroomServerBrowserFinishRefresh(&g_server_browser.model, true, 0u);
  } else {
    UpdateMatchmakingRecommendation();
    ShroomServerBrowserFinishRefresh(&g_server_browser.model, false, 0u);
  }
}
#endif

static void CopyDiscoveryResults(void) {
  const size_t result_count =
      ShroomServerDiscoveryStateResultCount(&g_server_browser.discovery.state);

  g_server_browser.server_count = 0u;
  for (size_t index = 0u;
       (index < result_count) && (g_server_browser.server_count < SERVER_BROWSER_MAX_SERVERS);
       ++index) {
    const ShroomDiscoveryCandidate* candidate =
        ShroomServerDiscoveryStateResult(&g_server_browser.discovery.state, index);
    ServerBrowserEntry* entry;

    if (candidate == NULL) {
      continue;
    }
    entry = &g_server_browser.servers[g_server_browser.server_count++];
    *entry = (ServerBrowserEntry){0};
    CopyText(entry->name, sizeof(entry->name), candidate->server.name);
    CopyText(entry->host, sizeof(entry->host), candidate->server.host);
    CopyText(entry->map_label, sizeof(entry->map_label), "Live / Arena");
    entry->port = candidate->server.port;
    entry->player_count = candidate->server.player_count;
    entry->player_capacity = candidate->server.capacity;
    entry->ping_ms = candidate->latency_ms;
    entry->type = SERVER_BROWSER_TYPE_OFFICIAL;
    entry->metadata_known = true;
    entry->reachable = true;
  }
  SortServersPreservingSelection();
  UpdateMatchmakingRecommendation();
}

static void PopulateQuickMatchCandidates(Game* game, uint64_t now_ms) {
  ShroomQuickMatchCandidate candidates[SERVER_BROWSER_MAX_SERVERS];

  if ((game == NULL) || (game->quick_match.phase != SHROOM_QUICK_MATCH_FINDING)) {
    return;
  }
  for (size_t index = 0u; index < g_server_browser.server_count; ++index) {
    const ServerBrowserEntry* entry = &g_server_browser.servers[index];
    ShroomQuickMatchCandidate* candidate = &candidates[index];
    *candidate = (ShroomQuickMatchCandidate){
        .port = (uint16_t)entry->port,
        .latency_ms = (uint16_t)entry->ping_ms,
        .player_count = (uint16_t)entry->player_count,
        .capacity = (uint16_t)entry->player_capacity,
        .reachable = entry->reachable,
    };
    CopyText(candidate->name, sizeof(candidate->name), entry->name);
    CopyText(candidate->host, sizeof(candidate->host), entry->host);
  }
  ShroomQuickMatchSetCandidates(&game->quick_match, candidates, g_server_browser.server_count,
                                now_ms);
}

static void ConnectQuickMatch(ShroomScreenManager* manager, Game* game) {
  const ShroomQuickMatchCandidate* selected;

  if ((game == NULL) || (game->quick_match.phase != SHROOM_QUICK_MATCH_CONNECTING)) {
    return;
  }
  selected = ShroomQuickMatchSelected(&game->quick_match);
  if (selected == NULL) {
    return;
  }
  ShroomServerDiscoveryShutdown(&g_server_browser.discovery);
  CopyText(game->selected_server_host, sizeof(game->selected_server_host), selected->host);
  game->selected_server_port = selected->port;
  game->auto_join_lobby = true;
  ClientNetShutdown(&game->net);
  ClientNetInit(&game->net, selected->host, selected->port, game->settings.player_name);
  ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
}

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
#ifdef TEST_MODE
  const char* demo_mode = getenv("SHROOM_SERVER_BROWSER_DEMO");
#endif
  const char* directory_host = getenv("SHROOM_DIRECTORY_HOST");
  const char* directory_port = getenv("SHROOM_DIRECTORY_PORT");
  char* end = NULL;
  unsigned long parsed_port = SHROOM_DIRECTORY_PORT;

  g_server_browser = (ServerBrowserState){0};
  ShroomClientPathsPrepareCacheFile(
      g_server_browser.recent_path, sizeof(g_server_browser.recent_path),
      SERVER_BROWSER_RECENTS_FILENAME, SERVER_BROWSER_RECENTS_LEGACY_PATH,
      SERVER_BROWSER_RECENTS_MAX_FILE_BYTES, ValidateRecentServersFile, NULL);
  ShroomServerBrowserModelInit(&g_server_browser.model);
  g_server_browser.selected_index = -1;
#ifdef TEST_MODE
  g_server_browser.demo_mode = (demo_mode != NULL) && (strcmp(demo_mode, "1") == 0);
#endif
  g_server_browser.directory_configured = (directory_host != NULL) && (directory_host[0] != '\0');
  CopyText(g_server_browser.directory_host, sizeof(g_server_browser.directory_host),
           g_server_browser.directory_configured ? directory_host : "");
  if ((directory_port != NULL) && (directory_port[0] != '\0')) {
    parsed_port = strtoul(directory_port, &end, 10);
    if ((end == directory_port) || (*end != '\0') || (parsed_port == 0u) ||
        (parsed_port > UINT16_MAX)) {
      parsed_port = SHROOM_DIRECTORY_PORT;
    }
  }
  g_server_browser.directory_port = (uint16_t)parsed_port;
  snprintf(g_server_browser.direct_host_input, sizeof(g_server_browser.direct_host_input), "%s",
           "127.0.0.1");
  snprintf(g_server_browser.direct_port_input, sizeof(g_server_browser.direct_port_input), "%u",
           SHROOM_SERVER_PORT);

  if (game != NULL && game->net.status == CLIENT_NET_ERROR && game->net.status_text[0] != '\0') {
    snprintf(g_server_browser.connection_error, sizeof(g_server_browser.connection_error), "%s",
             game->net.status_text);
  }

  LoadRecentServers();
  if ((game != NULL) && (game->quick_match.phase == SHROOM_QUICK_MATCH_FINDING)) {
    BeginDiscoveryRefresh(game);
  }
  return true;
}

static void ServerBrowserUpdate(ShroomScreenManager* manager, float delta_time) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const uint64_t now_ms = enet_time_get();

  if (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_LOADING) {
    if (!g_server_browser.demo_mode) {
      ShroomServerDiscoveryUpdate(&g_server_browser.discovery, now_ms);
      if (g_server_browser.discovery.state.phase == SHROOM_DISCOVERY_COMPLETE) {
        CopyDiscoveryResults();
        ShroomServerBrowserFinishRefresh(&g_server_browser.model, true,
                                         g_server_browser.server_count);
        if ((game != NULL) && (g_server_browser.server_count == 0u) &&
            (g_server_browser.discovery.state.full_server_count > 0u)) {
          ShroomQuickMatchServersFull(&game->quick_match);
        } else {
          PopulateQuickMatchCandidates(game, now_ms);
        }
      } else if (g_server_browser.discovery.state.phase == SHROOM_DISCOVERY_FAILED) {
        ShroomServerBrowserFinishRefresh(&g_server_browser.model, false, 0u);
        if (game != NULL) {
          ShroomQuickMatchDirectoryFailed(&game->quick_match);
        }
      }
    }
    return;
  }

  if ((game != NULL) && (game->quick_match.phase == SHROOM_QUICK_MATCH_PREVIEW)) {
    ShroomQuickMatchUpdate(&game->quick_match, now_ms);
    ConnectQuickMatch(manager, game);
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

static void DrawQuickMatch(ShroomScreenManager* manager, Game* game) {
  const ShroomQuickMatchCandidate* selected = ShroomQuickMatchSelected(&game->quick_match);

  if (!ShroomLayoutBeginCenteredPanel("Quick Match", 480.0f, 420.0f, 0.92f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }
  ShroomLayoutHeading("Quick Match");
  ShroomImGui_TextWrapped(ShroomQuickMatchStatusText(&game->quick_match));
  if (selected != NULL) {
    ShroomImGui_Separator();
    ShroomImGui_Text(TextFormat("Selected: %s", selected->name));
    ShroomImGui_Text(TextFormat("%s:%u | %u ms | %u/%u players", selected->host, selected->port,
                                selected->latency_ms, selected->player_count, selected->capacity));
    if (game->quick_match.high_latency_fallback) {
      ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.75f, 0.25f, 1.0f},
                              "High latency: every live server exceeds 200 ms.");
    }
  }
  ShroomImGui_Spacing();
  if (ShroomQuickMatchIsActive(&game->quick_match)) {
    if (ShroomLayoutButtonFullWidth("Cancel Quick Match", 38.0f)) {
      ShroomServerDiscoveryCancel(&g_server_browser.discovery);
      ShroomQuickMatchCancel(&game->quick_match);
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
    }
  } else if (game->quick_match.phase == SHROOM_QUICK_MATCH_FAILED) {
    if (ShroomLayoutButtonFullWidth("Retry Quick Match", 38.0f)) {
      ShroomQuickMatchBegin(&game->quick_match);
      BeginDiscoveryRefresh(game);
    }
    if (ShroomLayoutButtonFullWidth("Browse Servers", 38.0f)) {
      ShroomQuickMatchInit(&game->quick_match);
    }
    if (ShroomLayoutButtonFullWidth("Back To Menu", 38.0f)) {
      ShroomQuickMatchCancel(&game->quick_match);
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
    }
  }
  ShroomImGui_End();
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  bool join_clicked = false;

  ShroomScreenDrawFungalBackground((game == NULL) || game->settings.menu_animations_enabled);

  if ((game != NULL) && (game->quick_match.phase != SHROOM_QUICK_MATCH_IDLE) &&
      (game->quick_match.phase != SHROOM_QUICK_MATCH_SUCCEEDED) &&
      (game->quick_match.phase != SHROOM_QUICK_MATCH_CANCELLED)) {
    DrawQuickMatch(manager, game);
    return;
  }

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
  if (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_LOADING) {
    if (ShroomImGui_Button("Cancel", 120.0f, 0.0f)) {
      ShroomServerDiscoveryCancel(&g_server_browser.discovery);
      ShroomServerBrowserCancelRefresh(&g_server_browser.model);
    }
  } else {
    if (ShroomImGui_Button("Refresh", 120.0f, 0.0f)) {
      BeginDiscoveryRefresh(game);
    }
  }

  ShroomImGui_SameLine();
  ShroomImGui_Text(DiscoveryStatusText());

  if ((g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_READY) ||
      (g_server_browser.model.discovery_state == SHROOM_SERVER_DISCOVERY_STALE)) {
    ShroomImGui_Text(TextFormat("Source: live directory probes | Refreshed %.0f seconds ago",
                                g_server_browser.result_age));
  } else {
    ShroomImGui_Text("Source: none | Last refresh: never completed");
  }

  {
    ShroomImGui_Text("Matchmaking Recommendation");
    ShroomImGui_Text(MatchmakingRecommendationText());
    if (GetRecommendedServer() != NULL) {
      if (g_server_browser.high_latency_recommendation) {
        ShroomImGui_TextColored((ShroomImGuiColor){1.0f, 0.75f, 0.25f, 1.0f},
                                "Warning: all reachable servers exceed 200 ms.");
      }
    }
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

static void ServerBrowserCleanup(ShroomScreenManager* manager) {
  (void)manager;
  ShroomServerDiscoveryShutdown(&g_server_browser.discovery);
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
  screen->cleanup = ServerBrowserCleanup;
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

const char* ShroomTestGetServerBrowserStatusText(void) { return DiscoveryStatusText(); }

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

void ShroomTestSetServerBrowserMatchmakingScenario(bool high_latency) {
  g_server_browser.server_count = 2u;
  g_server_browser.servers[0] = (ServerBrowserEntry){"Canopy Match",
                                                     "canopy.test",
                                                     8,
                                                     32,
                                                     high_latency ? 240 : 45,
                                                     SHROOM_SERVER_PORT,
                                                     "Live / Arena",
                                                     SERVER_BROWSER_TYPE_OFFICIAL,
                                                     true,
                                                     true};
  g_server_browser.servers[1] = (ServerBrowserEntry){"Spore Match",
                                                     "spore.test",
                                                     20,
                                                     32,
                                                     high_latency ? 280 : 90,
                                                     SHROOM_SERVER_PORT,
                                                     "Live / Arena",
                                                     SERVER_BROWSER_TYPE_OFFICIAL,
                                                     true,
                                                     true};
  ShroomServerBrowserFinishRefresh(&g_server_browser.model, true, g_server_browser.server_count);
  UpdateMatchmakingRecommendation();
}

const char* ShroomTestGetServerBrowserRecommendationText(void) {
  return MatchmakingRecommendationText();
}

bool ShroomTestServerBrowserRecommendationIsHighLatency(void) {
  return g_server_browser.has_recommendation && g_server_browser.high_latency_recommendation;
}
#endif
