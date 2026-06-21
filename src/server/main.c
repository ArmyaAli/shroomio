#include <signal.h>
#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <enet/enet.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "shared/lifecycle.h"
#include "shared/protocol.h"
#include "shared/sim.h"
#include "auth.h"
#include "logger.h"

static const char* const DATABASE_SCHEMA[] = {
    "PRAGMA foreign_keys = ON",
    "CREATE TABLE IF NOT EXISTS players ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "player_uuid TEXT NOT NULL UNIQUE,"
    "display_name TEXT NOT NULL,"
    "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "last_seen_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "total_sessions INTEGER NOT NULL DEFAULT 0,"
    "total_play_time_seconds INTEGER NOT NULL DEFAULT 0)",
    "CREATE TABLE IF NOT EXISTS sessions ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "session_uuid TEXT NOT NULL UNIQUE,"
    "started_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "ended_at TEXT,"
    "duration_seconds INTEGER,"
    "player_count INTEGER NOT NULL DEFAULT 0,"
    "bot_count INTEGER NOT NULL DEFAULT 0,"
    "status TEXT NOT NULL CHECK (status IN ('active', 'completed', 'aborted')) DEFAULT 'active')",
    "CREATE TABLE IF NOT EXISTS session_participants ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "session_id INTEGER NOT NULL,"
    "player_id INTEGER NOT NULL,"
    "joined_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "left_at TEXT,"
    "final_rank INTEGER,"
    "final_mass REAL,"
    "FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE,"
    "FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE,"
    "UNIQUE(session_id, player_id))",
    "CREATE TABLE IF NOT EXISTS player_stats ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "player_id INTEGER NOT NULL UNIQUE,"
    "total_games_played INTEGER NOT NULL DEFAULT 0,"
    "total_kills INTEGER NOT NULL DEFAULT 0,"
    "total_deaths INTEGER NOT NULL DEFAULT 0,"
    "total_mass_consumed REAL NOT NULL DEFAULT 0.0,"
    "total_mass_lost REAL NOT NULL DEFAULT 0.0,"
    "total_distance_traveled REAL NOT NULL DEFAULT 0.0,"
    "total_spores_consumed INTEGER NOT NULL DEFAULT 0,"
    "total_players_consumed INTEGER NOT NULL DEFAULT 0,"
    "highest_mass_achieved REAL NOT NULL DEFAULT 0.0,"
    "highest_rank_achieved INTEGER NOT NULL DEFAULT 999,"
    "longest_survival_seconds REAL NOT NULL DEFAULT 0.0,"
    "updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE)",
    "CREATE TABLE IF NOT EXISTS match_events ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "session_id INTEGER NOT NULL,"
    "event_type TEXT NOT NULL CHECK (event_type IN ('player_spawn', 'player_death', "
    "'player_consume_player', 'player_consume_spore', 'player_reach_mass', 'game_tick')),"
    "event_timestamp TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "tick_number INTEGER,"
    "actor_player_id INTEGER,"
    "target_player_id INTEGER,"
    "mass_value REAL,"
    "position_x REAL,"
    "position_y REAL,"
    "metadata TEXT,"
    "FOREIGN KEY (session_id) REFERENCES sessions(id) ON DELETE CASCADE,"
    "FOREIGN KEY (actor_player_id) REFERENCES players(id) ON DELETE SET NULL,"
    "FOREIGN KEY (target_player_id) REFERENCES players(id) ON DELETE SET NULL)",
    "CREATE TABLE IF NOT EXISTS users ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "player_id INTEGER NOT NULL UNIQUE,"
    "username TEXT NOT NULL UNIQUE,"
    "email TEXT,"
    "password_hash TEXT,"
    "discord_id TEXT UNIQUE,"
    "auth_method TEXT NOT NULL CHECK (auth_method IN ('password', 'discord', 'anonymous')),"
    "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "last_login_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "FOREIGN KEY (player_id) REFERENCES players(id) ON DELETE CASCADE)",
    "CREATE TABLE IF NOT EXISTS auth_tokens ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "user_id INTEGER NOT NULL,"
    "token TEXT NOT NULL UNIQUE,"
    "token_type TEXT NOT NULL DEFAULT 'jwt',"
    "expires_at TEXT NOT NULL,"
    "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),"
    "revoked_at TEXT,"
    "FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE)",
    "CREATE TABLE IF NOT EXISTS mushroom_species ("
    "species_id INTEGER PRIMARY KEY CHECK (species_id >= 0 AND species_id < 10),"
    "name TEXT NOT NULL,"
    "description TEXT NOT NULL,"
    "pattern_id INTEGER NOT NULL DEFAULT 0,"
    "rarity_tier INTEGER NOT NULL DEFAULT 0,"
    "cap_color_rgba INTEGER NOT NULL DEFAULT 0,"
    "unlocked_by_default INTEGER NOT NULL DEFAULT 1 CHECK (unlocked_by_default IN (0, 1)),"
    "sort_order INTEGER NOT NULL UNIQUE)",
    "CREATE VIEW IF NOT EXISTS leaderboard_by_mass AS "
    "SELECT p.display_name, ps.highest_mass_achieved, ps.total_games_played, ps.total_kills, "
    "p.last_seen_at FROM players p JOIN player_stats ps ON p.id = ps.player_id "
    "WHERE ps.highest_mass_achieved > 0 ORDER BY ps.highest_mass_achieved DESC",
    "CREATE VIEW IF NOT EXISTS leaderboard_by_kills AS "
    "SELECT p.display_name, ps.total_kills, ps.total_players_consumed, ps.total_games_played, "
    "p.last_seen_at FROM players p JOIN player_stats ps ON p.id = ps.player_id "
    "WHERE ps.total_kills > 0 ORDER BY ps.total_kills DESC",
    "CREATE INDEX IF NOT EXISTS idx_players_uuid ON players(player_uuid)",
    "CREATE INDEX IF NOT EXISTS idx_players_last_seen ON players(last_seen_at DESC)",
    "CREATE INDEX IF NOT EXISTS idx_sessions_started ON sessions(started_at DESC)",
    "CREATE INDEX IF NOT EXISTS idx_sessions_status ON sessions(status)",
    "CREATE INDEX IF NOT EXISTS idx_session_participants_session ON "
    "session_participants(session_id)",
    "CREATE INDEX IF NOT EXISTS idx_session_participants_player ON session_participants(player_id)",
    "CREATE INDEX IF NOT EXISTS idx_player_stats_player ON player_stats(player_id)",
    "CREATE INDEX IF NOT EXISTS idx_match_events_session ON match_events(session_id)",
    "CREATE INDEX IF NOT EXISTS idx_match_events_type ON match_events(event_type)",
    "CREATE INDEX IF NOT EXISTS idx_match_events_timestamp ON match_events(event_timestamp DESC)",
    "CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)",
    "CREATE INDEX IF NOT EXISTS idx_users_discord_id ON users(discord_id)",
    "CREATE INDEX IF NOT EXISTS idx_users_player_id ON users(player_id)",
    "CREATE INDEX IF NOT EXISTS idx_auth_tokens_token ON auth_tokens(token)",
    "CREATE INDEX IF NOT EXISTS idx_auth_tokens_user_id ON auth_tokens(user_id)",
    "CREATE INDEX IF NOT EXISTS idx_auth_tokens_expires_at ON auth_tokens(expires_at)",
    "CREATE INDEX IF NOT EXISTS idx_mushroom_species_sort_order ON mushroom_species(sort_order)",
};

static bool InitializeDatabaseSchema(sqlite3* db) {
  for (size_t i = 0; i < sizeof(DATABASE_SCHEMA) / sizeof(DATABASE_SCHEMA[0]); ++i) {
    char* err_msg = NULL;
    if (sqlite3_exec(db, DATABASE_SCHEMA[i], NULL, NULL, &err_msg) != SQLITE_OK) {
      LOG_ERROR("failed to create schema: %s", err_msg);
      sqlite3_free(err_msg);
      return false;
    }
  }
  return true;
}

static bool ExecuteSqlFile(sqlite3* db, const char* path) {
  FILE* file;
  long file_size;
  char* sql;
  char* err_msg = NULL;
  size_t read_size;

  file = fopen(path, "rb");
  if (file == NULL) {
    LOG_ERROR("failed to open sql file: %s", path);
    return false;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    LOG_ERROR("failed to seek sql file: %s", path);
    return false;
  }
  file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    LOG_ERROR("failed to read sql file size: %s", path);
    return false;
  }
  rewind(file);

  sql = malloc((size_t)file_size + 1u);
  if (sql == NULL) {
    fclose(file);
    LOG_ERROR("failed to allocate sql file buffer: %s", path);
    return false;
  }

  read_size = fread(sql, 1u, (size_t)file_size, file);
  fclose(file);
  if (read_size != (size_t)file_size) {
    free(sql);
    LOG_ERROR("failed to read sql file: %s", path);
    return false;
  }
  sql[file_size] = '\0';

  if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
    LOG_ERROR("failed to execute sql file %s: %s", path, err_msg);
    sqlite3_free(err_msg);
    free(sql);
    return false;
  }
  free(sql);
  return true;
}

static const char* const kBotNamePrefixes[] = {
    "Mycelium", "Sporecap", "Hyphae", "Spore", "Fungal", "Myco", "Shroom", "Mold",
};
#define BOT_NAME_PREFIX_COUNT 8u
static const uint64_t kBotAdjustIntervalMs = 3000ull;

typedef struct ShroomLobby {
  uint32_t lobby_id;
  char name[SHROOM_LOBBY_MAX_NAME_LENGTH];
  ShroomWorldState world;
  bool active;
  bool is_dynamic;
  uint64_t empty_since_ms;
  uint64_t last_bot_adjust_ms;
} ShroomLobby;

typedef struct ServerSession {
  bool active;
  bool authenticated;
  bool handshake_received;
  bool spectating;
  uint32_t lobby_id;
  uint32_t player_id;
  uint32_t user_id;
  uint32_t last_processed_input_sequence;
  uint32_t last_logged_rtt_ms;
  uint32_t chat_message_count;
  uint32_t focused_entity_id; /* entity_id of piece being controlled; 0 = primary */
  uint64_t last_latency_log_time_ms;
  uint64_t chat_window_start_ms;
  ShroomPlayerState* player;
  char auth_token[SHROOM_AUTH_TOKEN_LENGTH + 1];
  char display_name[SHROOM_MAX_NAME_LENGTH];
} ServerSession;

static ShroomLifecycle g_lifecycle;

typedef struct ServerConfig {
  char bind_host[64];
  char database_path[256];
  enet_uint32 bind_address;
  uint16_t port;
  bool smoke_test;
} ServerConfig;

static bool ParsePort(const char* text, uint16_t* port) {
  char* end = NULL;
  unsigned long value;

  if ((text == NULL) || (text[0] == '\0')) {
    return false;
  }

  value = strtoul(text, &end, 10);
  if ((end == text) || (*end != '\0') || (value == 0ul) || (value > 65535ul)) {
    return false;
  }

  *port = (uint16_t)value;
  return true;
}

static void CopyConfigString(char* destination, size_t destination_size, const char* source) {
  if ((destination == NULL) || (destination_size == 0u)) {
    return;
  }
  if (source == NULL) {
    destination[0] = '\0';
    return;
  }
  snprintf(destination, destination_size, "%s", source);
}

static void PrintUsage(const char* program_name) {
  printf("Usage: %s [--bind ADDRESS] [--port PORT] [--database PATH]\n", program_name);
  printf("\n");
  printf("Self-hosted server options:\n");
  printf("  --bind ADDRESS    Local bind IP address, default 0.0.0.0\n");
  printf("  --port PORT       UDP listen port, default %u\n", SHROOM_SERVER_PORT);
  printf("  --database PATH   SQLite database path, default shroomio.db\n");
  printf("  --smoke-test      Start, initialize subsystems, then shut down cleanly\n");
  printf("  --help            Show this help text\n");
  printf("\n");
  printf("Environment overrides:\n");
  printf("  SHROOM_SERVER_BIND, SHROOM_SERVER_PORT, SHROOM_SERVER_DB_PATH\n");
}

static bool ResolveBindAddress(const char* bind_value, enet_uint32* bind_address) {
  unsigned int octets[4];
  char trailing;

  if ((bind_value == NULL) || (bind_value[0] == '\0') ||
      ((bind_value[0] == '*') && (bind_value[1] == '\0'))) {
    *bind_address = ENET_HOST_ANY;
    return true;
  }

  if (sscanf(bind_value, "%u.%u.%u.%u%c", &octets[0], &octets[1], &octets[2], &octets[3],
             &trailing) != 4) {
    return false;
  }
  for (size_t i = 0; i < 4; ++i) {
    if (octets[i] > 255u) {
      return false;
    }
  }
  if ((octets[0] == 0u) && (octets[1] == 0u) && (octets[2] == 0u) && (octets[3] == 0u)) {
    *bind_address = ENET_HOST_ANY;
    return true;
  }

  *bind_address =
      ENET_HOST_TO_NET_32((octets[0] << 24u) | (octets[1] << 16u) | (octets[2] << 8u) | octets[3]);
  return true;
}

static bool LoadServerConfig(ServerConfig* config, int argc, char** argv) {
  const char* env_bind = getenv("SHROOM_SERVER_BIND");
  const char* env_port = getenv("SHROOM_SERVER_PORT");
  const char* env_database = getenv("SHROOM_SERVER_DB_PATH");

  *config = (ServerConfig){.port = (uint16_t)SHROOM_SERVER_PORT};
  CopyConfigString(config->bind_host, sizeof(config->bind_host), "0.0.0.0");
  CopyConfigString(config->database_path, sizeof(config->database_path), "shroomio.db");

  if ((env_bind != NULL) && (env_bind[0] != '\0')) {
    CopyConfigString(config->bind_host, sizeof(config->bind_host), env_bind);
  }
  if ((env_database != NULL) && (env_database[0] != '\0')) {
    CopyConfigString(config->database_path, sizeof(config->database_path), env_database);
  }
  if ((env_port != NULL) && (env_port[0] != '\0') && !ParsePort(env_port, &config->port)) {
    fprintf(stderr, "Invalid SHROOM_SERVER_PORT: %s\n", env_port);
    return false;
  }

  int i = 1;
  while (i < argc) {
    if (strcmp(argv[i], "--help") == 0) {
      PrintUsage(argv[0]);
      exit(0);
    } else if (strcmp(argv[i], "--bind") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --bind\n");
        return false;
      }
      ++i;
      CopyConfigString(config->bind_host, sizeof(config->bind_host), argv[i]);
    } else if (strcmp(argv[i], "--port") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --port\n");
        return false;
      }
      ++i;
      if (!ParsePort(argv[i], &config->port)) {
        fprintf(stderr, "Invalid --port: %s\n", argv[i]);
        return false;
      }
    } else if (strcmp(argv[i], "--database") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --database\n");
        return false;
      }
      ++i;
      CopyConfigString(config->database_path, sizeof(config->database_path), argv[i]);
    } else if (strcmp(argv[i], "--smoke-test") == 0) {
      config->smoke_test = true;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      PrintUsage(argv[0]);
      return false;
    }
    ++i;
  }

  if (!ResolveBindAddress(config->bind_host, &config->bind_address)) {
    fprintf(stderr, "Invalid bind address: %s\n", config->bind_host);
    return false;
  }

  return true;
}

static void HandleSignal(int signal_number) {
  (void)signal_number;
  ShroomLifecycleRequestShutdown(&g_lifecycle);
}

static ShroomVec2 NormalizeInput(ShroomVec2 input) {
  const float length_sqr = ShroomVec2LengthSqr(input);
  float scale;

  if (length_sqr <= 0.0001f) {
    return (ShroomVec2){0};
  }

  scale = 1.0f / __builtin_sqrtf(length_sqr);
  return ShroomVec2Scale(input, scale);
}

static uint64_t GetTimeNanos(void) {
#ifdef _WIN32
  return (uint64_t)GetTickCount64() * 1000000ull;
#else
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return ((uint64_t)now.tv_sec * 1000000000ull) + (uint64_t)now.tv_nsec;
#endif
}

static uint64_t GetTimeMillis(void) { return GetTimeNanos() / 1000000ull; }

static void SleepUntil(uint64_t target_time_nanos) {
  uint64_t now = GetTimeNanos();
  uint64_t delta;

  if (now >= target_time_nanos) {
    return;
  }

  delta = target_time_nanos - now;
#ifdef _WIN32
  Sleep((DWORD)(delta / 1000000ull));
#else
  struct timespec sleep_time;

  sleep_time.tv_sec = (time_t)(delta / 1000000000ull);
  sleep_time.tv_nsec = (long)(delta % 1000000000ull);
  nanosleep(&sleep_time, 0);
#endif
}

static ENetPacket* CreatePacket(const void* data, size_t size, enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static ENetPacket* CreateProtocolPacket(const void* data, size_t size, ShroomPacketType type) {
  return CreatePacket(data, size,
                      ShroomPacketTypeUsesReliableDelivery(type) ? ENET_PACKET_FLAG_RELIABLE : 0);
}

/* Lightweight handshake ack — player/world data comes via LOBBY_JOINED. */
static void SendWelcome(ENetPeer* peer) {
  const ShroomWelcomePacket packet = {
      .header = {SHROOM_PACKET_WELCOME, SHROOM_ENET_CHANNEL_CONTROL, sizeof(ShroomWelcomePacket)},
      .protocol_version = SHROOM_PROTOCOL_VERSION,
  };

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_WELCOME));
}

static void SendMushroomSpeciesCatalog(ENetPeer* peer, sqlite3* db) {
  static const char* const sql =
      "SELECT species_id, name, description, pattern_id, rarity_tier, cap_color_rgba "
      "FROM mushroom_species WHERE unlocked_by_default = 1 ORDER BY sort_order ASC";
  ShroomMushroomSpeciesCatalogPacket packet = {0};
  sqlite3_stmt* stmt = NULL;
  uint8_t count = 0;

  if ((peer == NULL) || (db == NULL)) {
    return;
  }

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("failed to prepare mushroom species catalog query: %s", sqlite3_errmsg(db));
    return;
  }

  while ((count < SHROOM_MAX_MUSHROOM_SPECIES) && (sqlite3_step(stmt) == SQLITE_ROW)) {
    ShroomMushroomSpeciesEntry* entry = &packet.species[count];
    const char* name = (const char*)sqlite3_column_text(stmt, 1);
    const char* description = (const char*)sqlite3_column_text(stmt, 2);

    entry->species_id = (uint8_t)sqlite3_column_int(stmt, 0);
    entry->pattern_id = (uint8_t)sqlite3_column_int(stmt, 3);
    entry->rarity_tier = (uint8_t)sqlite3_column_int(stmt, 4);
    entry->cap_color_rgba = (uint32_t)sqlite3_column_int64(stmt, 5);
    if (name != NULL) {
      strncpy(entry->name, name, sizeof(entry->name) - 1u);
    }
    if (description != NULL) {
      strncpy(entry->description, description, sizeof(entry->description) - 1u);
    }
    ++count;
  }
  sqlite3_finalize(stmt);

  if (count == 0) {
    LOG_WARN("mushroom species catalog is empty");
    return;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG, sizeof(packet));
  packet.species_count = count;
  enet_peer_send(
      peer, SHROOM_ENET_CHANNEL_CONTROL,
      CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG));
}

static uint16_t CountLobbyRealPlayers(const ENetHost* host, uint32_t lobby_id) {
  uint16_t count = 0;
  size_t i;

  for (i = 0; i < host->peerCount; ++i) {
    const ServerSession* s = (const ServerSession*)host->peers[i].data;

    if (s != NULL && s->active && !s->spectating && s->lobby_id == lobby_id) {
      ++count;
    }
  }
  return count;
}

static uint16_t CountLobbySpectators(const ENetHost* host, uint32_t lobby_id) {
  uint16_t count = 0;
  size_t i;

  for (i = 0; i < host->peerCount; ++i) {
    const ServerSession* s = (const ServerSession*)host->peers[i].data;

    if (s != NULL && s->active && s->spectating && s->lobby_id == lobby_id) {
      ++count;
    }
  }
  return count;
}

static uint16_t CountLobbyAliveBots(const ShroomLobby* lobby) {
  uint16_t count = 0;
  size_t i;

  if (lobby == NULL) {
    return 0;
  }

  for (i = 0; i < lobby->world.player_count; ++i) {
    const ShroomPlayerState* player = &lobby->world.players[i];

    if (player->alive && player->is_bot) {
      ++count;
    }
  }

  return count;
}

static uint16_t GetLobbyBotTarget(uint16_t real_player_count) {
  const uint32_t weighted_real_players =
      (uint32_t)real_player_count * (uint32_t)SHROOM_BOT_REAL_PLAYER_WEIGHT;
  uint32_t target = weighted_real_players >= SHROOM_BOT_TARGET_TOTAL
                        ? SHROOM_BOT_FLOOR
                        : SHROOM_BOT_TARGET_TOTAL - weighted_real_players;
  const uint32_t max_bots_for_capacity = real_player_count >= SHROOM_MAX_PLAYERS
                                             ? 0u
                                             : (uint32_t)SHROOM_MAX_PLAYERS - real_player_count;

  if (target < SHROOM_BOT_FLOOR && max_bots_for_capacity >= SHROOM_BOT_FLOOR) {
    target = SHROOM_BOT_FLOOR;
  }
  if (target > max_bots_for_capacity) {
    target = max_bots_for_capacity;
  }

  return (uint16_t)target;
}

static void AssignBotName(ShroomPlayerState* bot, uint32_t bot_number) {
  if (bot == NULL) {
    return;
  }

  snprintf(bot->name, sizeof(bot->name), "%s-%u",
           kBotNamePrefixes[(bot_number - 1u) % BOT_NAME_PREFIX_COUNT], bot_number);
}

static ShroomLobby* FindLobbyById(ShroomLobby* lobbies, uint32_t lobby_id) {
  size_t i;

  if (lobby_id == 0) {
    return NULL;
  }
  for (i = 0; i < SHROOM_MAX_LOBBIES; ++i) {
    if (lobbies[i].active && lobbies[i].lobby_id == lobby_id) {
      return &lobbies[i];
    }
  }
  return NULL;
}

static void SendLobbyList(ENetPeer* peer, ShroomLobby* lobbies, const ENetHost* host) {
  ShroomLobbyListPacket packet = {0};
  uint8_t count = 0;
  size_t i;

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_LIST, sizeof(packet));
  for (i = 0; i < SHROOM_MAX_LOBBIES && count < SHROOM_MAX_LOBBIES; ++i) {
    ShroomLobby* lobby = &lobbies[i];
    uint16_t real_count;
    uint16_t bot_count;

    if (!lobby->active) {
      continue;
    }
    real_count = CountLobbyRealPlayers(host, lobby->lobby_id);
    bot_count = CountLobbyAliveBots(lobby);
    packet.lobbies[count] = (ShroomLobbyEntry){
        .lobby_id = lobby->lobby_id,
        .player_count = real_count,
        .bot_count = bot_count,
        .max_players = (uint16_t)(SHROOM_MAX_PLAYERS - SHROOM_BOT_FLOOR),
        .spectator_count = CountLobbySpectators(host, lobby->lobby_id),
        .is_dynamic = lobby->is_dynamic ? 1u : 0u,
    };
    snprintf(packet.lobbies[count].name, sizeof(packet.lobbies[count].name), "%s", lobby->name);
    ++count;
  }
  packet.lobby_count = count;
  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_LIST));
}

static void SendLobbyJoined(ENetPeer* peer, const ServerSession* session,
                            const ShroomLobby* lobby) {
  ShroomLobbyJoinedPacket packet = {0};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOINED, sizeof(packet));
  packet.lobby_id = lobby->lobby_id;
  packet.spectating = session->spectating ? 1u : 0u;
  packet.player_id = session->player_id;
  packet.entity_id = session->player != NULL ? session->player->entity_id : 0u;
  packet.server_tick_rate = (uint16_t)SHROOM_SERVER_TICK_RATE;
  packet.snapshot_rate = SHROOM_SNAPSHOT_RATE;
  packet.world_width = lobby->world.width;
  packet.world_height = lobby->world.height;
  snprintf(packet.lobby_name, sizeof(packet.lobby_name), "%s", lobby->name);
  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_JOINED));
}

static void SendLobbyCreated(ENetPeer* peer, const ShroomLobby* lobby) {
  ShroomLobbyCreatedPacket packet = {0};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_CREATED, sizeof(packet));
  packet.lobby_id = lobby->lobby_id;
  snprintf(packet.name, sizeof(packet.name), "%s", lobby->name);
  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_CREATED));
}

static void SendPong(ENetPeer* peer, uint32_t nonce) {
  const ShroomPongPacket packet = {
      .header = {SHROOM_PACKET_PONG, SHROOM_ENET_CHANNEL_CONTROL, sizeof(ShroomPongPacket)},
      .nonce = nonce,
  };

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_PONG));
}

static void SendAuthResponse(ENetPeer* peer, ShroomAuthResult result, uint32_t player_id,
                             const char* token, const char* message) {
  ShroomAuthResponsePacket packet = {0};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_AUTH_RESPONSE, sizeof(packet));
  packet.result = (uint8_t)result;
  packet.player_id = player_id;
  if (token != NULL) {
    strncpy(packet.token, token, sizeof(packet.token) - 1);
    packet.token[sizeof(packet.token) - 1] = '\0';
  }
  if (message != NULL) {
    strncpy(packet.message, message, sizeof(packet.message) - 1);
    packet.message[sizeof(packet.message) - 1] = '\0';
  }

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_AUTH_RESPONSE));
}

static void SendSnapshot(ENetPeer* peer, const ServerSession* session,
                         const ShroomWorldState* world) {
  ShroomSnapshotPacket packet;
  uint16_t player_count = 0;
  size_t index;

  if ((session == NULL) || !session->active) {
    return;
  }
  if (!session->spectating && session->player == NULL) {
    return;
  }

  packet = (ShroomSnapshotPacket){
      .header = {SHROOM_PACKET_SNAPSHOT, SHROOM_ENET_CHANNEL_SNAPSHOT,
                 sizeof(ShroomSnapshotPacket)},
      .tick = world->tick,
      .last_processed_input_sequence = session->last_processed_input_sequence,
      .player_id = session->player_id,
      .entity_id = (session->player != NULL) ? session->player->entity_id : 0u,
  };

  for (index = 0; (index < world->player_count) && (player_count < SHROOM_MAX_SNAPSHOT_PLAYERS);
       ++index) {
    const ShroomPlayerState* player = &world->players[index];

    if (!player->alive || (player->mass <= 0.0f)) {
      continue;
    }

    packet.players[player_count++] = (ShroomSnapshotPlayerState){
        .player_id = player->player_id,
        .entity_id = player->entity_id,
        .position_x = player->position.x,
        .position_y = player->position.y,
        .mass = player->mass,
        .radius = player->radius,
        .alive = 1u,
        .is_bot = player->is_bot ? 1u : 0u,
        .effect_flags =
            (uint16_t)((player->speed_powerup_timer > 0.0f ? SHROOM_POWERUP_EFFECT_SPEED : 0u) |
                       (player->shield_powerup_timer > 0.0f ? SHROOM_POWERUP_EFFECT_SHIELD : 0u)),
    };
    snprintf(packet.players[player_count - 1].name, sizeof(packet.players[player_count - 1].name),
             "%s", player->name);
  }

  packet.player_count = player_count;

  {
    const size_t trimmed_size = offsetof(ShroomSnapshotPacket, players) +
                                (size_t)player_count * sizeof(ShroomSnapshotPlayerState);
    packet.header.size = (uint16_t)trimmed_size;
    enet_peer_send(peer, SHROOM_ENET_CHANNEL_SNAPSHOT,
                   CreateProtocolPacket(&packet, trimmed_size, SHROOM_PACKET_SNAPSHOT));
  }
}

static void SendSporeState(ENetPeer* peer, const ShroomWorldState* world) {
  uint16_t spore_count = 0;
  size_t index;
  size_t packet_size;
  ENetPacket* enet_packet;

  for (index = 0; index < world->spore_count; ++index) {
    if (world->spores[index].active) {
      ++spore_count;
    }
  }

  packet_size = sizeof(ShroomPacketHeader) + sizeof(uint64_t) + sizeof(uint16_t) +
                sizeof(uint16_t) + ((size_t)spore_count * sizeof(ShroomSnapshotSporeState));
  enet_packet = enet_packet_create(NULL, packet_size, 0);

  {
    ShroomSporeStatePacket* packet = (ShroomSporeStatePacket*)enet_packet->data;
    uint16_t i = 0;

    ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_SPORE_STATE, (uint16_t)packet_size);
    packet->tick = world->tick;
    packet->spore_count = spore_count;
    packet->reserved = 0;

    for (index = 0; index < world->spore_count; ++index) {
      const ShroomSporeState* spore = &world->spores[index];

      if (!spore->active) {
        continue;
      }

      packet->spores[i++] = (ShroomSnapshotSporeState){
          .entity_id = spore->entity_id,
          .position_x = spore->position.x,
          .position_y = spore->position.y,
          .value = spore->value,
          .reserved = 0,
      };
    }
  }

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_SNAPSHOT, enet_packet);
}

static void SendPowerupState(ENetPeer* peer, const ShroomWorldState* world) {
  ShroomPowerupStatePacket packet = {0};
  uint16_t powerup_count = (uint16_t)world->powerup_count;

  if (powerup_count > SHROOM_MAX_POWERUPS) {
    powerup_count = SHROOM_MAX_POWERUPS;
  }

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_POWERUP_STATE, sizeof(packet));
  packet.tick = world->tick;
  packet.powerup_count = powerup_count;

  for (uint16_t index = 0; index < powerup_count; ++index) {
    const ShroomPowerupState* powerup = &world->powerups[index];

    packet.powerups[index] = (ShroomSnapshotPowerupState){
        .entity_id = powerup->entity_id,
        .position_x = powerup->position.x,
        .position_y = powerup->position.y,
        .type = (uint8_t)powerup->type,
        .active = powerup->active ? 1u : 0u,
    };
  }

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_SNAPSHOT,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_POWERUP_STATE));
}

static void DisconnectSession(ServerSession* session) {
  if ((session == NULL) || !session->active) {
    return;
  }

  if (session->player != NULL) {
    session->player->alive = false;
    session->player->input_direction = (ShroomVec2){0};
    session->player->mass = 0.0f;
    session->player->radius = 0.0f;
  }

  *session = (ServerSession){0};
}

static void LogPeerLatency(ENetPeer* peer, ServerSession* session, uint64_t now_ms) {
  uint32_t rtt_ms;

  if ((peer == 0) || (session == 0) || !session->active) {
    return;
  }

  rtt_ms = peer->roundTripTime;
  if (rtt_ms < SHROOM_LATENCY_WARNING_MS) {
    return;
  }
  if ((session->last_logged_rtt_ms == rtt_ms) &&
      ((now_ms - session->last_latency_log_time_ms) < 10000ull)) {
    return;
  }

  session->last_logged_rtt_ms = rtt_ms;
  session->last_latency_log_time_ms = now_ms;
  if (rtt_ms >= SHROOM_LATENCY_UNPLAYABLE_MS) {
    LOG_WARN("high latency peer: slot=%u player_id=%u rtt=%ums", (unsigned)peer->incomingPeerID,
             session->player_id, rtt_ms);
  } else {
    LOG_INFO("latency warning peer: slot=%u player_id=%u rtt=%ums", (unsigned)peer->incomingPeerID,
             session->player_id, rtt_ms);
  }
}

static void HandleHelloPacket(ENetPeer* peer, ServerSession* session, sqlite3* db,
                              const ENetPacket* enet_packet) {
  const ShroomHelloPacket* packet = (const ShroomHelloPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  if (packet->protocol_version != SHROOM_PROTOCOL_VERSION) {
    enet_peer_disconnect(peer, 0);
    return;
  }
  if (session->handshake_received) {
    return;
  }

  session->active = true;
  session->handshake_received = true;
  snprintf(session->display_name, sizeof(session->display_name), "%s", packet->name);
  if (session->display_name[0] == '\0') {
    snprintf(session->display_name, sizeof(session->display_name), "Player");
  }
  SendWelcome(peer);
  SendMushroomSpeciesCatalog(peer, db);
}

/* Update ai_controlled flags for all pieces of a player, and reset if the
 * focused piece no longer exists (merged or consumed). */
static void AdjustSessionAiControl(ServerSession* session, ShroomWorldState* world) {
  size_t i;
  bool focused_alive = false;

  if ((session == NULL) || !session->active || (session->player == NULL) || (world == NULL)) {
    return;
  }
  if (session->focused_entity_id == 0) {
    return;
  }

  /* Check whether the focused piece still exists. */
  for (i = 0; i < world->player_count; ++i) {
    const ShroomPlayerState* p = &world->players[i];
    if (p->alive && (p->entity_id == session->focused_entity_id)) {
      focused_alive = true;
      break;
    }
  }

  if (!focused_alive) {
    /* Focused piece gone: reset to primary and clear ai_controlled on all pieces. */
    session->focused_entity_id = 0;
    for (i = 0; i < world->player_count; ++i) {
      ShroomPlayerState* p = &world->players[i];
      if (p->alive && (p->player_id == session->player_id)) {
        p->ai_controlled = false;
      }
    }
  }
}

static void HandleInputPacket(ServerSession* session, const ENetPacket* enet_packet,
                              ShroomWorldState* world) {
  const ShroomInputPacket* packet = (const ShroomInputPacket*)enet_packet->data;
  ShroomPlayerState* target_piece;
  size_t i;

  if ((session == NULL) || !session->active || session->spectating || (session->player == NULL)) {
    return;
  }
  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  session->last_processed_input_sequence = packet->sequence;

  /* Resolve which piece the client is controlling. */
  target_piece = session->player;
  if (packet->focused_entity_id != 0 && world != NULL) {
    for (i = 0; i < world->player_count; ++i) {
      ShroomPlayerState* p = &world->players[i];
      if (p->alive && (p->player_id == session->player_id) &&
          (p->entity_id == packet->focused_entity_id)) {
        target_piece = p;
        break;
      }
    }
  }
  session->focused_entity_id = target_piece->entity_id;

  ShroomPlayerSetInput(target_piece,
                       NormalizeInput((ShroomVec2){packet->direction_x, packet->direction_y}));

  /* Mark all other pieces of this player as AI-controlled. */
  if (world != NULL) {
    for (i = 0; i < world->player_count; ++i) {
      ShroomPlayerState* p = &world->players[i];
      if (p->alive && (p->player_id == session->player_id)) {
        p->ai_controlled = (p->entity_id != session->focused_entity_id);
      }
    }
  }

  if (packet->split_requested && (world != NULL)) {
    ShroomWorldSplitPlayer(world, target_piece);
  }
}

static void HandleAuthRequestPacket(ENetPeer* peer, ServerSession* session,
                                    ShroomAuthContext* auth_ctx, const ENetPacket* enet_packet) {
  const ShroomAuthRequestPacket* packet = (const ShroomAuthRequestPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  if (session->authenticated) {
    SendAuthResponse(peer, SHROOM_AUTH_INVALID_INPUT, 0, NULL, "Already authenticated");
    return;
  }

  ShroomAuthMethod method = (ShroomAuthMethod)packet->auth_method;
  bool is_register = packet->is_register != 0;

  if (method == SHROOM_AUTH_PASSWORD) {
    if (is_register) {
      ShroomAuthUser user;
      ShroomAuthResult result =
          ShroomAuthRegister(auth_ctx, packet->username, packet->password, &user);
      if (result == SHROOM_AUTH_SUCCESS) {
        ShroomAuthToken token;
        result = ShroomAuthLogin(auth_ctx, packet->username, packet->password, &token);
        if (result == SHROOM_AUTH_SUCCESS) {
          session->authenticated = true;
          session->user_id = token.user_id;
          strncpy(session->auth_token, token.token, sizeof(session->auth_token) - 1);
          session->auth_token[sizeof(session->auth_token) - 1] = '\0';
          SendAuthResponse(peer, result, user.player_id, token.token, "Registration successful");
          LOG_INFO("Auth: User '%s' registered and logged in", packet->username);
        } else {
          SendAuthResponse(peer, result, 0, NULL, "Login after registration failed");
        }
      } else {
        const char* msg = (result == SHROOM_AUTH_USERNAME_TAKEN) ? "Username already taken"
                                                                 : "Registration failed";
        SendAuthResponse(peer, result, 0, NULL, msg);
      }
    } else {
      ShroomAuthToken token;
      ShroomAuthResult result =
          ShroomAuthLogin(auth_ctx, packet->username, packet->password, &token);
      if (result == SHROOM_AUTH_SUCCESS) {
        ShroomAuthUser user;
        ShroomAuthValidateToken(auth_ctx, token.token, &user);
        session->authenticated = true;
        session->user_id = token.user_id;
        strncpy(session->auth_token, token.token, sizeof(session->auth_token) - 1);
        session->auth_token[sizeof(session->auth_token) - 1] = '\0';
        SendAuthResponse(peer, result, user.player_id, token.token, "Login successful");
        LOG_INFO("Auth: User '%s' logged in", packet->username);
      } else {
        const char* msg = (result == SHROOM_AUTH_INVALID_CREDENTIALS)
                              ? "Invalid username or password"
                              : "Login failed";
        SendAuthResponse(peer, result, 0, NULL, msg);
      }
    }
  } else if (method == SHROOM_AUTH_ANONYMOUS) {
    ShroomAuthToken token;
    ShroomAuthResult result = ShroomAuthLoginAnonymous(auth_ctx, packet->username, &token);
    if (result == SHROOM_AUTH_SUCCESS) {
      ShroomAuthUser user;
      ShroomAuthValidateToken(auth_ctx, token.token, &user);
      session->authenticated = true;
      session->user_id = token.user_id;
      strncpy(session->auth_token, token.token, sizeof(session->auth_token) - 1);
      session->auth_token[sizeof(session->auth_token) - 1] = '\0';
      SendAuthResponse(peer, result, user.player_id, token.token, "Anonymous login successful");
      LOG_INFO("Auth: Anonymous user '%s' logged in", packet->username);
    } else {
      SendAuthResponse(peer, result, 0, NULL, "Anonymous login failed");
    }
  } else {
    SendAuthResponse(peer, SHROOM_AUTH_INVALID_INPUT, 0, NULL, "Unsupported auth method");
  }
}

static void HandleChatPacket(ENetHost* host, ServerSession* session, const ENetPacket* enet_packet,
                             uint64_t now_ms) {
  ShroomChatPacket broadcast = {0};
  size_t index;
  size_t msg_len;

  if ((host == NULL) || (session == NULL) || !session->active || (session->player == NULL) ||
      (enet_packet == NULL) || (enet_packet->dataLength < sizeof(ShroomChatPacket))) {
    return;
  }

  /* Rate limit: max SHROOM_CHAT_RATE_LIMIT_COUNT messages per window. */
  if (now_ms - session->chat_window_start_ms >= SHROOM_CHAT_RATE_LIMIT_WINDOW_MS) {
    session->chat_window_start_ms = now_ms;
    session->chat_message_count = 0;
  }
  if (session->chat_message_count >= SHROOM_CHAT_RATE_LIMIT_COUNT) {
    return;
  }
  session->chat_message_count += 1;

  /* Build broadcast packet from validated fields. */
  ShroomPacketHeaderInit(&broadcast.header, SHROOM_PACKET_CHAT, sizeof(broadcast));
  broadcast.sender_id = session->player_id;
  snprintf(broadcast.sender_name, sizeof(broadcast.sender_name), "%s",
           session->player->name[0] != '\0' ? session->player->name : "Unknown");

  /* Copy and null-terminate message from the incoming packet. */
  msg_len = enet_packet->dataLength - offsetof(ShroomChatPacket, message);
  if (msg_len >= sizeof(broadcast.message)) {
    msg_len = sizeof(broadcast.message) - 1u;
  }
  memcpy(broadcast.message, ((const ShroomChatPacket*)enet_packet->data)->message, msg_len);
  broadcast.message[msg_len] = '\0';
  /* Sanitize: replace control characters up to the null terminator only.
   * Iterating to msg_len would turn trailing null bytes into spaces and
   * cause wrapped blank lines in the log output. */
  for (size_t i = 0; broadcast.message[i] != '\0'; ++i) {
    if ((unsigned char)broadcast.message[i] < 0x20u) {
      broadcast.message[i] = ' ';
    }
  }

  LOG_INFO("chat player_id=%u name=%.31s msg=%s", session->player_id, broadcast.sender_name,
           broadcast.message);

  /* Broadcast only to peers in the same lobby. */
  for (index = 0; index < host->peerCount; ++index) {
    ENetPeer* peer = &host->peers[index];
    const ServerSession* peer_session = (const ServerSession*)peer->data;
    ENetPacket* out;

    if (peer->state != ENET_PEER_STATE_CONNECTED) {
      continue;
    }
    if ((peer_session == NULL) || (peer_session->lobby_id != session->lobby_id)) {
      continue;
    }
    out = CreateProtocolPacket(&broadcast, sizeof(broadcast), SHROOM_PACKET_CHAT);
    if (out != NULL) {
      enet_peer_send(peer, SHROOM_ENET_CHANNEL_CHAT, out);
    }
  }
  enet_host_flush(host);
}

static bool AddLobbyBot(ShroomLobby* lobby, uint32_t* next_player_id) {
  ShroomPlayerState* bot;
  uint32_t bot_number;

  if ((lobby == NULL) || (next_player_id == NULL)) {
    return false;
  }

  bot_number = *next_player_id;
  bot = ShroomWorldSpawnPlayer(&lobby->world, (*next_player_id)++, true);
  if (bot == NULL) {
    return false;
  }

  AssignBotName(bot, bot_number);
  return true;
}

static bool RemoveLobbyBot(ShroomLobby* lobby) {
  size_t i;

  if (lobby == NULL) {
    return false;
  }

  for (i = lobby->world.player_count; i > 0; --i) {
    ShroomPlayerState* player = &lobby->world.players[i - 1u];

    if (!player->alive || !player->is_bot) {
      continue;
    }

    player->alive = false;
    player->mass = 0.0f;
    player->radius = 0.0f;
    player->input_direction = (ShroomVec2){0};
    return true;
  }

  return false;
}

static void InitializeLobbyBots(ShroomLobby* lobby, uint32_t* next_player_id) {
  const uint16_t target = GetLobbyBotTarget(0);

  while (CountLobbyAliveBots(lobby) < target) {
    if (!AddLobbyBot(lobby, next_player_id)) {
      break;
    }
  }
}

static void AdjustLobbyBots(ShroomLobby* lobby, const ENetHost* host, uint32_t* next_player_id,
                            uint64_t now_ms) {
  uint16_t real_player_count;
  uint16_t target_bot_count;
  uint16_t current_bot_count;

  if ((lobby == NULL) || (host == NULL) || (next_player_id == NULL)) {
    return;
  }

  real_player_count = CountLobbyRealPlayers(host, lobby->lobby_id);
  target_bot_count = GetLobbyBotTarget(real_player_count);
  current_bot_count = CountLobbyAliveBots(lobby);

  if (current_bot_count == target_bot_count) {
    lobby->last_bot_adjust_ms = now_ms;
    return;
  }
  if ((lobby->last_bot_adjust_ms != 0) &&
      ((now_ms - lobby->last_bot_adjust_ms) < kBotAdjustIntervalMs)) {
    return;
  }

  lobby->last_bot_adjust_ms = now_ms;
  if (current_bot_count < target_bot_count) {
    AddLobbyBot(lobby, next_player_id);
  } else {
    RemoveLobbyBot(lobby);
  }
}

static ShroomLobby* CreateLobby(ShroomLobby* lobbies, uint32_t lobby_id, const char* name,
                                bool is_dynamic, uint32_t* next_player_id) {
  size_t i;

  for (i = 0; i < SHROOM_MAX_LOBBIES; ++i) {
    if (!lobbies[i].active) {
      ShroomLobby* lobby = &lobbies[i];

      *lobby = (ShroomLobby){0};
      lobby->active = true;
      lobby->lobby_id = lobby_id;
      lobby->is_dynamic = is_dynamic;
      if (name != NULL && name[0] != '\0') {
        snprintf(lobby->name, sizeof(lobby->name), "%s", name);
      } else {
        snprintf(lobby->name, sizeof(lobby->name), "Arena %u", lobby_id);
      }
      ShroomWorldInit(&lobby->world);
      InitializeLobbyBots(lobby, next_player_id);
      LOG_INFO("lobby created: id=%u name=%.31s dynamic=%d", lobby_id, lobby->name,
               (int)is_dynamic);
      return lobby;
    }
  }
  return NULL;
}

static void HandleLobbyListQuery(ENetPeer* peer, const ServerSession* session, ShroomLobby* lobbies,
                                 const ENetHost* host) {
  if (!session->handshake_received) {
    return;
  }
  SendLobbyList(peer, lobbies, host);
}

static void HandleLobbyJoin(ENetPeer* peer, ServerSession* session, ShroomLobby* lobbies,
                            const ENetPacket* enet_packet, uint32_t* next_player_id) {
  const ShroomLobbyJoinPacket* packet = (const ShroomLobbyJoinPacket*)enet_packet->data;
  ShroomLobby* lobby;

  if (!session->handshake_received || (enet_packet->dataLength < sizeof(*packet))) {
    return;
  }
  if (session->lobby_id != 0) {
    return; /* already in a lobby */
  }

  lobby = FindLobbyById(lobbies, packet->lobby_id);
  if (lobby == NULL) {
    return;
  }

  session->lobby_id = lobby->lobby_id;
  session->spectating = (packet->spectate != 0);

  if (!session->spectating) {
    session->player = ShroomWorldSpawnPlayer(&lobby->world, (*next_player_id)++, false);
    if (session->player == NULL) {
      session->lobby_id = 0;
      return;
    }
    session->player_id = session->player->player_id;
    snprintf(session->player->name, sizeof(session->player->name), "%s", session->display_name);
  }

  SendLobbyJoined(peer, session, lobby);
  LOG_INFO("lobby join: player_id=%u lobby_id=%u spectating=%d", session->player_id,
           lobby->lobby_id, (int)session->spectating);
}

static void HandleLobbyLeave(ServerSession* session) {
  if (session->player != NULL) {
    session->player->alive = false;
    session->player->mass = 0.0f;
    session->player->radius = 0.0f;
    session->player = NULL;
  }
  LOG_INFO("lobby leave: player_id=%u lobby_id=%u", session->player_id, session->lobby_id);
  session->lobby_id = 0;
  session->spectating = false;
  session->player_id = 0;
}

static void HandleLobbyCreate(ENetPeer* peer, const ServerSession* session, ShroomLobby* lobbies,
                              const ENetPacket* enet_packet, uint32_t* next_player_id,
                              uint32_t* next_lobby_id) {
  const ShroomLobbyCreatePacket* packet = (const ShroomLobbyCreatePacket*)enet_packet->data;
  const ShroomLobby* lobby;

  if (!session->handshake_received || (enet_packet->dataLength < sizeof(*packet))) {
    return;
  }

  lobby = CreateLobby(lobbies, (*next_lobby_id)++, packet->name, true, next_player_id);
  if (lobby == NULL) {
    return;
  }
  SendLobbyCreated(peer, lobby);
}

static void HandlePacket(ENetHost* host, ENetPeer* peer, ServerSession* session,
                         ShroomLobby* lobbies, ShroomAuthContext* auth_ctx,
                         const ENetPacket* enet_packet, uint8_t channel_id,
                         uint32_t* next_player_id, uint32_t* next_lobby_id, uint64_t now_ms) {
  const ShroomPacketHeader* header;

  if ((enet_packet == 0) || (enet_packet->dataLength < sizeof(ShroomPacketHeader))) {
    return;
  }

  header = (const ShroomPacketHeader*)enet_packet->data;
  if (!ShroomPacketHeaderUsesExpectedChannel(header, channel_id)) {
    return;
  }

  switch ((ShroomPacketType)header->type) {
  case SHROOM_PACKET_HELLO:
    HandleHelloPacket(peer, session, auth_ctx != NULL ? auth_ctx->db : NULL, enet_packet);
    break;
  case SHROOM_PACKET_INPUT: {
    ShroomLobby* input_lobby = FindLobbyById(lobbies, session->lobby_id);
    HandleInputPacket(session, enet_packet, input_lobby != NULL ? &input_lobby->world : NULL);
    break;
  }
  case SHROOM_PACKET_AUTH_REQUEST:
    HandleAuthRequestPacket(peer, session, auth_ctx, enet_packet);
    break;
  case SHROOM_PACKET_PING:
    if (enet_packet->dataLength >= sizeof(ShroomPingPacket)) {
      SendPong(peer, ((const ShroomPingPacket*)enet_packet->data)->nonce);
    }
    break;
  case SHROOM_PACKET_CHAT:
    HandleChatPacket(host, session, enet_packet, now_ms);
    break;
  case SHROOM_PACKET_LOBBY_LIST_QUERY:
    HandleLobbyListQuery(peer, session, lobbies, host);
    break;
  case SHROOM_PACKET_LOBBY_JOIN:
    HandleLobbyJoin(peer, session, lobbies, enet_packet, next_player_id);
    break;
  case SHROOM_PACKET_LOBBY_LEAVE:
    HandleLobbyLeave(session);
    break;
  case SHROOM_PACKET_LOBBY_CREATE:
    HandleLobbyCreate(peer, session, lobbies, enet_packet, next_player_id, next_lobby_id);
    break;
  default:
    break;
  }
}

int main(int argc, char** argv) {
  const uint64_t tick_interval_nanos = 1000000000ull / (uint64_t)SHROOM_SERVER_TICK_RATE;
  const uint64_t snapshot_interval_ticks =
      (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_SNAPSHOT_RATE);
  const uint64_t spore_interval_ticks =
      (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_SPORE_STATE_RATE);
  ENetAddress address = {0};
  ENetHost* host;
  static ShroomLobby lobbies[SHROOM_MAX_LOBBIES] = {0};
  static ServerSession sessions[SHROOM_SERVER_MAX_CLIENTS] = {0};
  uint32_t next_player_id = 1;
  uint32_t next_lobby_id = 1;
  uint64_t next_tick_time;
  uint64_t last_health_log_ms = 0;
  sqlite3* db = NULL;
  ShroomAuthContext auth_ctx = {0};
  ServerConfig config;

  ShroomLifecycleInit(&g_lifecycle);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_INIT);

  LoggerInit(LOG_LEVEL_INFO, 1);
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);

  if (!LoadServerConfig(&config, argc, argv)) {
    ShroomLifecycleSetError(&g_lifecycle, 1, "Invalid server configuration");
    return 1;
  }

  if (sqlite3_open(config.database_path, &db) != SQLITE_OK) {
    LOG_ERROR("failed to open database: %s", sqlite3_errmsg(db));
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database initialization failed");
    return 1;
  }
  LOG_INFO("database initialized: path=%s", config.database_path);

  if (!InitializeDatabaseSchema(db)) {
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database schema creation failed");
    return 1;
  }
  if (!ExecuteSqlFile(db, "database/seed.sql")) {
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database seed failed");
    return 1;
  }

  ShroomAuthInit(&auth_ctx, db);

  if (enet_initialize() != 0) {
    LOG_ERROR("failed to initialize ENet");
    ShroomAuthShutdown(&auth_ctx);
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "ENet initialization failed");
    return 1;
  }

  /* Create fixed lobbies. */
  {
    uint32_t li;

    for (li = 0; li < SHROOM_LOBBY_DEFAULT_COUNT; ++li) {
      CreateLobby(lobbies, next_lobby_id++, NULL, false, &next_player_id);
    }
  }

  address.host = config.bind_address;
  address.port = config.port;
  host = enet_host_create(&address, SHROOM_SERVER_MAX_CLIENTS, SHROOM_ENET_CHANNEL_COUNT, 0, 0);
  if (host == 0) {
    LOG_ERROR("failed to create ENet host");
    enet_deinitialize();
    ShroomAuthShutdown(&auth_ctx);
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 2, "ENet host creation failed");
    return 1;
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_START);
  LOG_INFO("shroomio server listening on %s:%u/udp", config.bind_host, config.port);
  if (config.smoke_test) {
    ShroomLifecycleRequestShutdown(&g_lifecycle);
  }
  next_tick_time = GetTimeNanos();

  while (ShroomLifecycleIsRunning(&g_lifecycle) &&
         !ShroomLifecycleIsShutdownRequested(&g_lifecycle)) {
    ENetEvent event;
    const uint64_t now_ms = GetTimeMillis();

    while (enet_host_service(host, &event, 0) > 0) {
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        if (event.peer->incomingPeerID < SHROOM_SERVER_MAX_CLIENTS) {
          event.peer->data = &sessions[event.peer->incomingPeerID];
          LOG_INFO("peer connected: slot=%u", (unsigned)event.peer->incomingPeerID);
        } else {
          LOG_WARN("rejected connection: no available slots");
          enet_peer_disconnect(event.peer, 0);
        }
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        HandlePacket(host, event.peer, (ServerSession*)event.peer->data, lobbies, &auth_ctx,
                     event.packet, event.channelID, &next_player_id, &next_lobby_id, now_ms);
        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT:
        LOG_INFO("peer disconnected: slot=%u", (unsigned)event.peer->incomingPeerID);
        DisconnectSession((ServerSession*)event.peer->data);
        event.peer->data = 0;
        break;
      case ENET_EVENT_TYPE_NONE:
      default:
        break;
      }
    }

    /* Tick and broadcast per-lobby. */
    {
      size_t li;

      for (li = 0; li < SHROOM_MAX_LOBBIES; ++li) {
        ShroomLobby* lobby = &lobbies[li];
        size_t pi;
        uint16_t real_count;
        uint16_t spec_count;

        if (!lobby->active) {
          continue;
        }

        real_count = CountLobbyRealPlayers(host, lobby->lobby_id);
        spec_count = CountLobbySpectators(host, lobby->lobby_id);

        /* Keep empty lobbies discoverable, but do not spend tick budget simulating
         * offscreen bots until someone is actually watching or playing there. */
        if ((real_count + spec_count) == 0u) {
          if (lobby->is_dynamic) {
            if (lobby->empty_since_ms == 0) {
              lobby->empty_since_ms = now_ms;
            } else if ((now_ms - lobby->empty_since_ms) >=
                       (SHROOM_LOBBY_DYNAMIC_EMPTY_TIMEOUT_S * 1000ull)) {
              LOG_INFO("lobby expired: id=%u name=%.31s", lobby->lobby_id, lobby->name);
              lobby->active = false;
            }
          }
          continue;
        }

        lobby->empty_since_ms = 0;

        AdjustLobbyBots(lobby, host, &next_player_id, now_ms);

        ShroomWorldStep(&lobby->world, 1.0f / SHROOM_SERVER_TICK_RATE);

        /* Reset ai_controlled for sessions whose focused piece merged or was consumed. */
        {
          size_t ai;
          for (ai = 0; ai < host->peerCount; ++ai) {
            ServerSession* s = (ServerSession*)host->peers[ai].data;
            if ((host->peers[ai].state == ENET_PEER_STATE_CONNECTED) && (s != NULL) && s->active &&
                (s->lobby_id == lobby->lobby_id)) {
              AdjustSessionAiControl(s, &lobby->world);
            }
          }
        }

        if ((snapshot_interval_ticks > 0) && ((lobby->world.tick % snapshot_interval_ticks) == 0)) {
          for (pi = 0; pi < host->peerCount; ++pi) {
            ServerSession* s = (ServerSession*)host->peers[pi].data;

            if ((host->peers[pi].state != ENET_PEER_STATE_CONNECTED) || (s == NULL) || !s->active ||
                (s->lobby_id != lobby->lobby_id)) {
              continue;
            }
            LogPeerLatency(&host->peers[pi], s, now_ms);
            SendSnapshot(&host->peers[pi], s, &lobby->world);
          }
          enet_host_flush(host);
        }

        if ((spore_interval_ticks > 0) && ((lobby->world.tick % spore_interval_ticks) == 0)) {
          for (pi = 0; pi < host->peerCount; ++pi) {
            const ServerSession* s = (const ServerSession*)host->peers[pi].data;

            if ((host->peers[pi].state != ENET_PEER_STATE_CONNECTED) || (s == NULL) || !s->active ||
                (s->lobby_id != lobby->lobby_id)) {
              continue;
            }
            SendSporeState(&host->peers[pi], &lobby->world);
            SendPowerupState(&host->peers[pi], &lobby->world);
          }
          enet_host_flush(host);
        }
      }
    }

    /* Session health log every 60 s. */
    if ((now_ms - last_health_log_ms) >= 60000ull) {
      size_t li;

      last_health_log_ms = now_ms;
      for (li = 0; li < SHROOM_MAX_LOBBIES; ++li) {
        if (lobbies[li].active) {
          uint16_t real = CountLobbyRealPlayers(host, lobbies[li].lobby_id);
          uint16_t spec = CountLobbySpectators(host, lobbies[li].lobby_id);
          uint16_t bots = CountLobbyAliveBots(&lobbies[li]);

          LOG_INFO("health lobby_id=%u real=%u bots=%u spectators=%u", lobbies[li].lobby_id, real,
                   bots, spec);
        }
      }
    }

    next_tick_time += tick_interval_nanos;
    SleepUntil(next_tick_time);
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_STOP);
  enet_host_destroy(host);
  enet_deinitialize();
  ShroomAuthShutdown(&auth_ctx);
  sqlite3_close(db);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN);
  LOG_INFO("shroomio server shutting down");
  return 0;
}
