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
#include "shared/net_telemetry.h"
#include "shared/player_identity.h"
#include "shared/intermission.h"
#include "shared/protocol.h"
#include "shared/profiler.h"
#include "shared/sim.h"
#include "shared/snapshot_replication.h"
#include "shared/snapshot_scheduler.h"
#include "shared/world_replication.h"
#include "auth.h"
#include "account_auth.h"
#include "database.h"
#include "directory_registry.h"
#include "logger.h"
#include "lobby_capacity.h"
#include "match_persistence.h"
#include "rest_server.h"
#include "session_cleanup.h"
#include "snapshot_stats.h"
#include "input_admission.h"
#include "voice_relay.h"

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
  ShroomIntermissionState intermission;
  uint16_t last_intermission_second;
  uint32_t next_round_id;
  uint64_t persistence_round_started_ms;
  char persistence_round_uuid[96];
  ShroomPersistedParticipant persistence_participants[SHROOM_MAX_PARTICIPANTS];
  size_t persistence_participant_count;
  ShroomSnapshotScheduler snapshot_scheduler;
} ShroomLobby;

typedef struct ServerSession {
  bool active;
  bool authenticated;
  bool handshake_received;
  bool spectating;
  bool is_ready;
  bool entered_match;
  uint32_t lobby_id;
  uint32_t player_id;
  uint32_t user_id;
  uint32_t last_processed_input_sequence;
  ShroomInputAdmission input_admission;
  uint32_t last_logged_rtt_ms;
  uint32_t chat_message_count;
  uint32_t focused_entity_id; /* entity_id of piece being controlled; 0 = primary */
  uint64_t last_latency_log_time_ms;
  uint64_t chat_window_start_ms;
  ShroomPlayerState* player;
  char auth_token[SHROOM_AUTH_TOKEN_LENGTH + 1];
  char display_name[SHROOM_MAX_NAME_LENGTH];
  ShroomWorldReplicationPeerState world_replication;
  ShroomSnapshotInterestState snapshot_interest;
  ShroomSnapshotHistory snapshot_history;
  uint64_t acknowledged_snapshot_tick;
} ServerSession;

static ShroomLifecycle g_lifecycle;

typedef struct ServerConfig {
  char bind_host[64];
  char rest_bind_host[SHROOM_REST_BIND_MAX_LENGTH + 1u];
  char rest_certificate_path[SHROOM_REST_CERT_PATH_MAX_LENGTH + 1u];
  char database_path[256];
  char directory_host[SHROOM_DIRECTORY_HOST_LENGTH];
  char server_name[SHROOM_DIRECTORY_SERVER_NAME_LENGTH];
  enet_uint32 bind_address;
  uint16_t port;
  uint16_t rest_port;
  uint16_t directory_port;
  uint16_t snapshot_rate;
  float match_duration_seconds;
  bool directory_mode;
  bool smoke_test;
  bool benchmark;
  bool rest_enabled;
  uint32_t benchmark_ticks;
  uint32_t benchmark_bots;
} ServerConfig;

typedef struct DirectoryAdvertiser {
  ENetHost* host;
  ENetPeer* peer;
  uint64_t last_heartbeat_ms;
  uint64_t last_connect_attempt_ms;
  bool connected;
} DirectoryAdvertiser;

#define SHROOM_DIRECTORY_HEARTBEAT_INTERVAL_MS 5000ull
#define SHROOM_DIRECTORY_RECONNECT_INTERVAL_MS 2000ull
#define SHROOM_DIRECTORY_MAX_EVENTS_PER_TICK 128u

typedef struct ServerProfileStats {
  ShroomProfileWindow tick;
  ShroomProfileWindow enet_events;
  ShroomProfileWindow simulation;
  ShroomProfileWindow broadcast;
  uint64_t last_log_ms;
} ServerProfileStats;

static ServerProfileStats g_server_profile;
static ShroomNetTelemetry g_server_net_telemetry;
static uint64_t g_server_event_budget_exhaustions;
static uint64_t g_server_input_stale_rejections;
static uint64_t g_server_input_rate_rejections;
static uint32_t g_lobby_roster_generation;

static float g_match_duration_seconds = SHROOM_MATCH_DURATION_SECONDS;
static uint16_t g_snapshot_rate = SHROOM_SNAPSHOT_RATE;

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

static bool ParseUint32(const char* text, uint32_t* out_value) {
  char* end = NULL;
  unsigned long value;

  if ((text == NULL) || (text[0] == '\0')) {
    return false;
  }

  value = strtoul(text, &end, 10);
  if ((end == text) || (*end != '\0') || (value > UINT32_MAX)) {
    return false;
  }

  *out_value = (uint32_t)value;
  return true;
}

static bool ParseSnapshotRate(const char* text, uint16_t* out_rate) {
  uint32_t value;

  if (!ParseUint32(text, &value) || (value < SHROOM_SNAPSHOT_RATE_MIN) ||
      (value > SHROOM_SNAPSHOT_RATE_MAX)) {
    return false;
  }
  *out_rate = (uint16_t)value;
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
  printf("Usage: %s [--bind ADDRESS] [--port PORT] [--database PATH] [REST OPTIONS]\n",
         program_name);
  printf("\n");
  printf("Self-hosted server options:\n");
  printf("  --bind ADDRESS    Local bind IP address, default 0.0.0.0\n");
  printf("  --port PORT       UDP listen port, default %u\n", SHROOM_SERVER_PORT);
  printf("  --database PATH   SQLite database path, default shroomio.db\n");
  printf("  --rest-bind ADDRESS  HTTPS bind IP address, default 0.0.0.0\n");
  printf("  --rest-port PORT     HTTPS listen port, default %u\n", SHROOM_REST_DEFAULT_PORT);
  printf("  --rest-cert PATH     Combined PEM certificate and private key; enables HTTPS\n");
  printf("  --directory       Run the bounded directory service instead of a game server\n");
  printf("  --directory-port PORT  Directory UDP port, default %u\n", SHROOM_DIRECTORY_PORT);
  printf("  --snapshot-rate HZ  Snapshot rate %u-%u Hz, default %u\n", SHROOM_SNAPSHOT_RATE_MIN,
         SHROOM_SNAPSHOT_RATE_MAX, SHROOM_SNAPSHOT_RATE);
  printf("  --match-duration SECONDS  Match duration in seconds, default %.0f\n",
         SHROOM_MATCH_DURATION_SECONDS);
  printf("  --smoke-test      Start, initialize subsystems, then shut down cleanly\n");
  printf("  --benchmark       Run deterministic server simulation benchmark and exit\n");
  printf("  --benchmark-ticks N    Benchmark tick count, default 600\n");
  printf("  --benchmark-bots N     Benchmark bot/player count, default 8\n");
  printf("  --help            Show this help text\n");
  printf("\n");
  printf("Environment overrides:\n");
  printf("  SHROOM_SERVER_BIND, SHROOM_SERVER_PORT, SHROOM_SERVER_DB_PATH\n");
  printf("  SHROOM_SERVER_REST_BIND, SHROOM_SERVER_REST_PORT, SHROOM_SERVER_REST_CERT\n");
  printf("  SHROOM_DIRECTORY_HOST, SHROOM_DIRECTORY_PORT, SHROOM_SERVER_NAME\n");
  printf("  SHROOM_SERVER_SNAPSHOT_RATE\n");
}

static void SanitizeServerName(char* name) {
  bool visible = false;

  for (size_t index = 0u; name[index] != '\0'; ++index) {
    const unsigned char character = (unsigned char)name[index];
    if ((character < 32u) || (character > 126u)) {
      name[index] = '_';
    }
    if (name[index] != ' ') {
      visible = true;
    }
  }
  if (!visible) {
    CopyConfigString(name, SHROOM_DIRECTORY_SERVER_NAME_LENGTH, "Shroomio Server");
  }
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
  const char* env_rest_bind = getenv("SHROOM_SERVER_REST_BIND");
  const char* env_rest_port = getenv("SHROOM_SERVER_REST_PORT");
  const char* env_rest_certificate = getenv("SHROOM_SERVER_REST_CERT");
  const char* env_directory_host = getenv("SHROOM_DIRECTORY_HOST");
  const char* env_directory_port = getenv("SHROOM_DIRECTORY_PORT");
  const char* env_server_name = getenv("SHROOM_SERVER_NAME");
  const char* env_snapshot_rate = getenv("SHROOM_SERVER_SNAPSHOT_RATE");

  *config = (ServerConfig){.port = (uint16_t)SHROOM_SERVER_PORT,
                           .rest_port = (uint16_t)SHROOM_REST_DEFAULT_PORT,
                           .directory_port = (uint16_t)SHROOM_DIRECTORY_PORT,
                           .snapshot_rate = (uint16_t)SHROOM_SNAPSHOT_RATE,
                           .match_duration_seconds = SHROOM_MATCH_DURATION_SECONDS,
                           .benchmark_ticks = 600u,
                           .benchmark_bots = 8u};
  CopyConfigString(config->bind_host, sizeof(config->bind_host), "0.0.0.0");
  CopyConfigString(config->rest_bind_host, sizeof(config->rest_bind_host), "0.0.0.0");
  CopyConfigString(config->database_path, sizeof(config->database_path), "shroomio.db");
  CopyConfigString(config->server_name, sizeof(config->server_name), "Shroomio Server");

  if ((env_bind != NULL) && (env_bind[0] != '\0')) {
    CopyConfigString(config->bind_host, sizeof(config->bind_host), env_bind);
  }
  if ((env_database != NULL) && (env_database[0] != '\0')) {
    CopyConfigString(config->database_path, sizeof(config->database_path), env_database);
  }
  if ((env_rest_bind != NULL) && (env_rest_bind[0] != '\0')) {
    CopyConfigString(config->rest_bind_host, sizeof(config->rest_bind_host), env_rest_bind);
  }
  if ((env_rest_port != NULL) && (env_rest_port[0] != '\0') &&
      !ParsePort(env_rest_port, &config->rest_port)) {
    fprintf(stderr, "Invalid SHROOM_SERVER_REST_PORT: %s\n", env_rest_port);
    return false;
  }
  if ((env_rest_certificate != NULL) && (env_rest_certificate[0] != '\0')) {
    CopyConfigString(config->rest_certificate_path, sizeof(config->rest_certificate_path),
                     env_rest_certificate);
    config->rest_enabled = true;
  }
  if ((env_port != NULL) && (env_port[0] != '\0') && !ParsePort(env_port, &config->port)) {
    fprintf(stderr, "Invalid SHROOM_SERVER_PORT: %s\n", env_port);
    return false;
  }
  if ((env_directory_host != NULL) && (env_directory_host[0] != '\0')) {
    CopyConfigString(config->directory_host, sizeof(config->directory_host), env_directory_host);
  }
  if ((env_directory_port != NULL) && (env_directory_port[0] != '\0') &&
      !ParsePort(env_directory_port, &config->directory_port)) {
    fprintf(stderr, "Invalid SHROOM_DIRECTORY_PORT: %s\n", env_directory_port);
    return false;
  }
  if ((env_server_name != NULL) && (env_server_name[0] != '\0')) {
    CopyConfigString(config->server_name, sizeof(config->server_name), env_server_name);
  }
  if ((env_snapshot_rate != NULL) && (env_snapshot_rate[0] != '\0') &&
      !ParseSnapshotRate(env_snapshot_rate, &config->snapshot_rate)) {
    fprintf(stderr, "Invalid SHROOM_SERVER_SNAPSHOT_RATE: %s (expected %u-%u)\n", env_snapshot_rate,
            SHROOM_SNAPSHOT_RATE_MIN, SHROOM_SNAPSHOT_RATE_MAX);
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
    } else if (strcmp(argv[i], "--rest-bind") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --rest-bind\n");
        return false;
      }
      ++i;
      CopyConfigString(config->rest_bind_host, sizeof(config->rest_bind_host), argv[i]);
    } else if (strcmp(argv[i], "--rest-port") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --rest-port\n");
        return false;
      }
      ++i;
      if (!ParsePort(argv[i], &config->rest_port)) {
        fprintf(stderr, "Invalid --rest-port: %s\n", argv[i]);
        return false;
      }
    } else if (strcmp(argv[i], "--rest-cert") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --rest-cert\n");
        return false;
      }
      ++i;
      CopyConfigString(config->rest_certificate_path, sizeof(config->rest_certificate_path),
                       argv[i]);
      config->rest_enabled = true;
    } else if (strcmp(argv[i], "--directory") == 0) {
      config->directory_mode = true;
    } else if (strcmp(argv[i], "--directory-port") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --directory-port\n");
        return false;
      }
      ++i;
      if (!ParsePort(argv[i], &config->directory_port)) {
        fprintf(stderr, "Invalid --directory-port: %s\n", argv[i]);
        return false;
      }
    } else if (strcmp(argv[i], "--snapshot-rate") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --snapshot-rate\n");
        return false;
      }
      ++i;
      if (!ParseSnapshotRate(argv[i], &config->snapshot_rate)) {
        fprintf(stderr, "Invalid --snapshot-rate: %s (expected %u-%u)\n", argv[i],
                SHROOM_SNAPSHOT_RATE_MIN, SHROOM_SNAPSHOT_RATE_MAX);
        return false;
      }
    } else if (strcmp(argv[i], "--smoke-test") == 0) {
      config->smoke_test = true;
    } else if (strcmp(argv[i], "--benchmark") == 0) {
      config->benchmark = true;
    } else if (strcmp(argv[i], "--benchmark-ticks") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --benchmark-ticks\n");
        return false;
      }
      ++i;
      if (!ParseUint32(argv[i], &config->benchmark_ticks) || (config->benchmark_ticks == 0u)) {
        fprintf(stderr, "Invalid --benchmark-ticks: %s\n", argv[i]);
        return false;
      }
    } else if (strcmp(argv[i], "--benchmark-bots") == 0) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --benchmark-bots\n");
        return false;
      }
      ++i;
      if (!ParseUint32(argv[i], &config->benchmark_bots) ||
          (config->benchmark_bots > SHROOM_MAX_PARTICIPANTS)) {
        fprintf(stderr, "Invalid --benchmark-bots: %s\n", argv[i]);
        return false;
      }
    } else if (strcmp(argv[i], "--match-duration") == 0) {
      char* end = NULL;
      double value;
      if ((i + 1) >= argc) {
        fprintf(stderr, "Missing value for --match-duration\n");
        return false;
      }
      ++i;
      value = strtod(argv[i], &end);
      if ((end == argv[i]) || (*end != '\0') || (value < 1.0) || (value > 86400.0)) {
        fprintf(stderr, "Invalid --match-duration: %s\n", argv[i]);
        return false;
      }
      config->match_duration_seconds = (float)value;
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

  SanitizeServerName(config->server_name);

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

static void ServerProfileMaybeLog(uint64_t now_ms) {
  if (!ShroomProfileEnabled()) {
    return;
  }
  if ((g_server_profile.last_log_ms != 0ull) &&
      ((now_ms - g_server_profile.last_log_ms) < 5000ull)) {
    return;
  }

  g_server_profile.last_log_ms = now_ms;
  LOG_INFO(
      "profile,server,tick_avg_ms=%.3f,tick_peak_ms=%.3f,enet_avg_ms=%.3f,"
      "enet_peak_ms=%.3f,simulation_avg_ms=%.3f,simulation_peak_ms=%.3f,"
      "broadcast_avg_ms=%.3f,broadcast_peak_ms=%.3f",
      ShroomProfileAverageMs(&g_server_profile.tick), g_server_profile.tick.peak_ms,
      ShroomProfileAverageMs(&g_server_profile.enet_events), g_server_profile.enet_events.peak_ms,
      ShroomProfileAverageMs(&g_server_profile.simulation), g_server_profile.simulation.peak_ms,
      ShroomProfileAverageMs(&g_server_profile.broadcast), g_server_profile.broadcast.peak_ms);
  ShroomProfileResetPeak(&g_server_profile.tick);
  ShroomProfileResetPeak(&g_server_profile.enet_events);
  ShroomProfileResetPeak(&g_server_profile.simulation);
  ShroomProfileResetPeak(&g_server_profile.broadcast);
}

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

static int RunServerBenchmark(const ServerConfig* config) {
  ShroomWorldState world;
  ShroomSnapshotScheduler snapshot_scheduler;
  const uint64_t spore_interval_ticks =
      (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_WORLD_REPLICATION_RATE);
  double sim_sum_ms = 0.0;
  double sim_peak_ms = 0.0;
  uint64_t estimated_snapshot_bytes = 0ull;
  uint64_t estimated_packet_count = 0ull;
  ShroomWorldReplicationPeerState* replication_states = NULL;
  ShroomWorldStateRecord* replication_records = NULL;
  uint64_t started_nanos;
  uint64_t elapsed_nanos;

  ShroomWorldInitWithSeed(&world, 42u + config->benchmark_bots);
  if (!ShroomSnapshotSchedulerInit(&snapshot_scheduler, (uint32_t)SHROOM_SERVER_TICK_RATE,
                                   config->snapshot_rate)) {
    return 1;
  }
  if (config->benchmark_bots > 0u) {
    replication_states = calloc(config->benchmark_bots, sizeof(*replication_states));
    replication_records = malloc(SHROOM_MAX_WORLD_STATE_RECORDS * sizeof(*replication_records));
    if ((replication_states == NULL) || (replication_records == NULL)) {
      free(replication_states);
      free(replication_records);
      return 1;
    }
  }
  for (uint32_t player = 0u; player < config->benchmark_bots; ++player) {
    ShroomPlayerState* spawned = ShroomWorldSpawnPlayer(&world, player + 1u, true);
    if (spawned != NULL) {
      spawned->input_direction = NormalizeInput(
          (ShroomVec2){(player % 3u) == 0u ? 1.0f : -0.35f, (player % 5u) == 0u ? 0.65f : -0.2f});
    }
  }

  started_nanos = GetTimeNanos();
  for (uint32_t tick = 0u; tick < config->benchmark_ticks; ++tick) {
    const uint64_t sim_start_nanos = GetTimeNanos();
    ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);
    const double sim_ms = ShroomProfileNanosToMs(GetTimeNanos() - sim_start_nanos);

    sim_sum_ms += sim_ms;
    if (sim_ms > sim_peak_ms) {
      sim_peak_ms = sim_ms;
    }

    if (ShroomSnapshotSchedulerStep(&snapshot_scheduler)) {
      estimated_packet_count += config->benchmark_bots;
      estimated_snapshot_bytes +=
          (uint64_t)config->benchmark_bots *
          (uint64_t)(ShroomSnapshotChunkCount(world.player_count) *
                         offsetof(ShroomSnapshotPacket, payload) +
                     world.player_count *
                         (sizeof(ShroomSnapshotRecordHeader) + 4u * sizeof(float)));
    }
    if ((spore_interval_ticks > 0u) && ((world.tick % spore_interval_ticks) == 0u)) {
      for (uint32_t player = 0u; player < config->benchmark_bots; ++player) {
        const ShroomWorldReplicationBatch batch = ShroomWorldReplicationBuild(
            &replication_states[player], &world, world.players[player].position, false,
            replication_records, SHROOM_MAX_WORLD_STATE_RECORDS);
        if (batch.keyframe || (batch.record_count > 0u)) {
          estimated_packet_count += ShroomWorldReplicationPacketCount(batch.record_count);
          estimated_snapshot_bytes += ShroomWorldReplicationPacketBytes(batch.record_count);
        }
      }
    }
  }
  elapsed_nanos = GetTimeNanos() - started_nanos;

  printf("scenario,players,ticks,elapsed_ms,avg_tick_ms,worst_tick_ms,estimated_packets,"
         "estimated_bytes,rtt_ms,cpu_time_ms,memory_kb\n");
  printf("server_bots,%u,%u,%.3f,%.3f,%.3f,%llu,%llu,0,%.3f,0\n", config->benchmark_bots,
         config->benchmark_ticks, ShroomProfileNanosToMs(elapsed_nanos),
         sim_sum_ms / (double)config->benchmark_ticks, sim_peak_ms,
         (unsigned long long)estimated_packet_count, (unsigned long long)estimated_snapshot_bytes,
         ShroomProfileNanosToMs(elapsed_nanos));
  free(replication_states);
  free(replication_records);
  return 0;
}

static ENetPacket* CreatePacket(const void* data, size_t size, enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static ENetPacket* CreateProtocolPacket(const void* data, size_t size, ShroomPacketType type) {
  enet_uint32 flags = 0u;
  if (ShroomPacketTypeUsesReliableDelivery(type)) {
    flags |= ENET_PACKET_FLAG_RELIABLE;
  }
  if (ShroomPacketTypeUsesUnsequencedDelivery(type)) {
    flags |= ENET_PACKET_FLAG_UNSEQUENCED;
  }
  return CreatePacket(data, size, flags);
}

static bool SendPacket(ENetPeer* peer, uint8_t channel, ShroomPacketType type, ENetPacket* packet) {
  const size_t peer_index = peer != NULL ? peer->incomingPeerID : SHROOM_NET_TELEMETRY_MAX_PEERS;
  const size_t bytes = packet != NULL ? packet->dataLength : 0u;

  if ((peer == NULL) || (packet == NULL) || (enet_peer_send(peer, channel, packet) != 0)) {
    ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel, type, bytes,
                                 GetTimeMillis());
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  ShroomNetTelemetryRecordSent(&g_server_net_telemetry, peer_index, channel, type, bytes,
                               GetTimeMillis());
  return true;
}

static bool SendVoiceRelayPacket(void* context, size_t peer_index, const void* data,
                                 size_t wire_size) {
  ENetHost* host = (ENetHost*)context;
  ENetPeer* peer;

  if ((host == NULL) || (peer_index >= host->peerCount)) {
    return false;
  }
  peer = &host->peers[peer_index];
  if (peer->state != ENET_PEER_STATE_CONNECTED) {
    return false;
  }
  return SendPacket(peer, SHROOM_ENET_CHANNEL_VOICE, SHROOM_PACKET_VOICE_FRAME,
                    CreateProtocolPacket(data, wire_size, SHROOM_PACKET_VOICE_FRAME));
}

/* Lightweight handshake ack — player/world data comes via LOBBY_JOINED. */
static void SendWelcome(ENetPeer* peer) {
  const ShroomWelcomePacket packet = {
      .header = {SHROOM_PACKET_WELCOME, SHROOM_ENET_CHANNEL_CONTROL, sizeof(ShroomWelcomePacket)},
      .protocol_version = SHROOM_PROTOCOL_VERSION,
      .server_tick_rate = (uint16_t)SHROOM_SERVER_TICK_RATE,
      .snapshot_rate = g_snapshot_rate,
  };

  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_WELCOME,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_WELCOME));
}

static void SendMushroomSpeciesCatalog(ENetPeer* peer, sqlite3* db) {
  static const char* const sql =
      "SELECT species_id, name, description, pattern_id, rarity_tier, cap_color_rgba "
      "FROM mushroom_species WHERE unlocked_by_default = 1 ORDER BY sort_order ASC";
  ShroomMushroomSpeciesCatalogPacket packet = {0};
  sqlite3_stmt* stmt = NULL;
  size_t packet_size;
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

  packet_size = offsetof(ShroomMushroomSpeciesCatalogPacket, species) +
                (size_t)count * sizeof(packet.species[0]);
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG,
                         (uint16_t)packet_size);
  packet.species_count = count;
  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG,
             CreateProtocolPacket(&packet, packet_size, SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG));
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

    if (player->alive && player->is_bot && (player->piece_index == 0u)) {
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
  const uint32_t max_bots_for_capacity =
      real_player_count >= SHROOM_MAX_PARTICIPANTS
          ? 0u
          : (uint32_t)SHROOM_MAX_PARTICIPANTS - real_player_count;

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

static int64_t ResolveSessionDatabasePlayerId(sqlite3* db, const ServerSession* session) {
  sqlite3_stmt* statement = NULL;
  int64_t player_id = 0;

  if ((db == NULL) || (session == NULL) || !session->authenticated || (session->user_id == 0u) ||
      (sqlite3_prepare_v2(db, "SELECT player_id FROM users WHERE id=?1", -1, &statement, NULL) !=
       SQLITE_OK)) {
    return 0;
  }
  sqlite3_bind_int64(statement, 1, session->user_id);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    player_id = sqlite3_column_int64(statement, 0);
  }
  sqlite3_finalize(statement);
  return player_id;
}

static void BeginLobbyPersistenceRound(ShroomLobby* lobby) {
  if (lobby == NULL) {
    return;
  }
  lobby->persistence_round_started_ms = GetTimeMillis();
  lobby->persistence_participant_count = 0u;
  memset(lobby->persistence_participants, 0, sizeof(lobby->persistence_participants));
  snprintf(lobby->persistence_round_uuid, sizeof(lobby->persistence_round_uuid), "%u-%u-%llu",
           lobby->lobby_id, lobby->next_round_id + 1u,
           (unsigned long long)lobby->persistence_round_started_ms);
}

static void RegisterPersistenceParticipant(ShroomLobby* lobby, sqlite3* db,
                                           const ServerSession* session) {
  int64_t database_player_id;

  if ((lobby == NULL) || (session == NULL) || session->spectating || (session->player_id == 0u)) {
    return;
  }
  database_player_id = ResolveSessionDatabasePlayerId(db, session);
  if (database_player_id == 0) {
    return;
  }
  for (size_t index = 0u; index < lobby->persistence_participant_count; ++index) {
    ShroomPersistedParticipant* participant = &lobby->persistence_participants[index];

    if (participant->runtime_player_id == session->player_id) {
      return;
    }
    if (participant->database_player_id == database_player_id) {
      participant->runtime_player_id = session->player_id;
      participant->disconnected = false;
      return;
    }
  }
  if (lobby->persistence_participant_count >= SHROOM_MAX_PARTICIPANTS) {
    return;
  }
  lobby->persistence_participants[lobby->persistence_participant_count++] =
      (ShroomPersistedParticipant){
          .database_player_id = database_player_id,
          .runtime_player_id = session->player_id,
      };
}

static int PersistenceParticipantRank(const ShroomLobby* lobby, uint32_t runtime_player_id);

static void CapturePersistenceParticipant(ShroomLobby* lobby, uint32_t runtime_player_id,
                                          bool disconnected) {
  if ((lobby == NULL) || (runtime_player_id == 0u)) {
    return;
  }
  for (size_t index = 0u; index < lobby->persistence_participant_count; ++index) {
    ShroomPersistedParticipant* participant = &lobby->persistence_participants[index];
    const ShroomRoundStats* stats;

    if (participant->runtime_player_id != runtime_player_id) {
      continue;
    }
    participant->disconnected = participant->disconnected || disconnected;
    participant->final_mass = ShroomWorldGetColonyMass(&lobby->world, runtime_player_id);
    participant->final_rank = PersistenceParticipantRank(lobby, runtime_player_id);
    stats = ShroomWorldGetRoundStats(&lobby->world, runtime_player_id);
    if (stats != NULL) {
      participant->round_stats.colony_mass = stats->colony_mass;
      if (stats->peak_mass > participant->round_stats.peak_mass) {
        participant->round_stats.peak_mass = stats->peak_mass;
      }
      participant->round_stats.survival_seconds += stats->survival_seconds;
      participant->round_stats.kills += stats->kills;
      participant->round_stats.spores_collected += stats->spores_collected;
      participant->round_stats.powerups_collected += stats->powerups_collected;
      participant->round_stats.center_zone_seconds += stats->center_zone_seconds;
      participant->round_stats.mid_zone_seconds += stats->mid_zone_seconds;
      participant->round_stats.outer_zone_seconds += stats->outer_zone_seconds;
      participant->round_stats.splits_used += stats->splits_used;
      participant->round_stats.ejects_used += stats->ejects_used;
    }
    return;
  }
}

static int PersistenceParticipantRank(const ShroomLobby* lobby, uint32_t runtime_player_id) {
  const float mass = ShroomWorldGetColonyMass(&lobby->world, runtime_player_id);
  const float objective_score = lobby->world.game_mode == SHROOM_GAME_MODE_KING_OF_HILL
                                    ? ShroomWorldGetObjectiveScore(&lobby->world, runtime_player_id)
                                    : 0.0f;
  int rank = 1;

  for (size_t index = 0u; index < lobby->world.player_count; ++index) {
    const ShroomPlayerState* other = &lobby->world.players[index];
    const uint32_t other_id = other->player_id;
    bool already_ranked = false;

    if (!other->alive || (other_id == 0u) || (other_id == runtime_player_id)) {
      continue;
    }
    for (size_t previous = 0u; previous < index; ++previous) {
      if (lobby->world.players[previous].alive &&
          (lobby->world.players[previous].player_id == other_id)) {
        already_ranked = true;
        break;
      }
    }
    if (already_ranked) {
      continue;
    }

    const float other_mass = ShroomWorldGetColonyMass(&lobby->world, other_id);
    const float other_objective_score = lobby->world.game_mode == SHROOM_GAME_MODE_KING_OF_HILL
                                            ? ShroomWorldGetObjectiveScore(&lobby->world, other_id)
                                            : 0.0f;
    if ((other_objective_score > objective_score) ||
        ((other_objective_score == objective_score) && (other_mass > mass)) ||
        ((other_objective_score == objective_score) && (other_mass == mass) &&
         (other_id < runtime_player_id))) {
      ++rank;
    }
  }
  return rank;
}

static void PersistCompletedLobbyRound(sqlite3* db, ShroomLobby* lobby) {
  ShroomCompletedMatch match;
  ShroomMatchPersistenceResult result;
  const uint64_t elapsed_ms = GetTimeMillis() >= lobby->persistence_round_started_ms
                                  ? GetTimeMillis() - lobby->persistence_round_started_ms
                                  : 0u;

  for (size_t index = 0u; index < lobby->persistence_participant_count; ++index) {
    ShroomPersistedParticipant* participant = &lobby->persistence_participants[index];
    if (!participant->disconnected) {
      CapturePersistenceParticipant(lobby, participant->runtime_player_id, false);
      participant->final_rank = PersistenceParticipantRank(lobby, participant->runtime_player_id);
    }
  }
  match = (ShroomCompletedMatch){
      .session_uuid = lobby->persistence_round_uuid,
      .lobby_id = lobby->lobby_id,
      .round_id = lobby->next_round_id + 1u,
      .game_mode = lobby->world.game_mode,
      .final_tick = lobby->world.tick,
      .duration_seconds = (uint32_t)(elapsed_ms / 1000u),
      .bot_count = CountLobbyAliveBots(lobby),
      .winner_runtime_player_id = lobby->world.podium_player_ids[0],
      .participants = lobby->persistence_participants,
      .participant_count = lobby->persistence_participant_count,
  };
  result = ShroomMatchPersistenceSave(db, &match);
  if (result == SHROOM_MATCH_PERSISTENCE_ERROR) {
    LOG_ERROR("failed to persist completed round lobby_id=%u round=%u", lobby->lobby_id,
              match.round_id);
  }
}

static void SendLobbyList(ENetPeer* peer, ShroomLobby* lobbies, const ENetHost* host) {
  ShroomLobbyListPacket packet = {0};
  size_t packet_size;
  uint8_t count = 0;
  size_t i;

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
        .max_players = (uint16_t)SHROOM_MAX_PLAYABLE_PARTICIPANTS,
        .spectator_count = CountLobbySpectators(host, lobby->lobby_id),
        .is_dynamic = lobby->is_dynamic ? 1u : 0u,
        .game_mode = (uint8_t)lobby->world.game_mode,
    };
    snprintf(packet.lobbies[count].name, sizeof(packet.lobbies[count].name), "%s", lobby->name);
    ++count;
  }
  packet_size =
      offsetof(ShroomLobbyListPacket, lobbies) + (size_t)count * sizeof(packet.lobbies[0]);
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_LIST, (uint16_t)packet_size);
  packet.lobby_count = count;
  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_LIST,
             CreateProtocolPacket(&packet, packet_size, SHROOM_PACKET_LOBBY_LIST));
}

static void SendLobbyJoined(ENetPeer* peer, const ServerSession* session,
                            const ShroomLobby* lobby) {
  ShroomLobbyJoinedPacket packet = {0};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOINED, sizeof(packet));
  packet.lobby_id = lobby->lobby_id;
  packet.spectating = session->spectating ? 1u : 0u;
  packet.game_mode = (uint8_t)lobby->world.game_mode;
  packet.player_id = session->player_id;
  packet.entity_id = session->player != NULL ? session->player->entity_id : 0u;
  packet.server_tick_rate = (uint16_t)SHROOM_SERVER_TICK_RATE;
  packet.snapshot_rate = g_snapshot_rate;
  packet.max_players = (uint16_t)SHROOM_MAX_PLAYABLE_PARTICIPANTS;
  packet.world_width = lobby->world.width;
  packet.world_height = lobby->world.height;
  snprintf(packet.lobby_name, sizeof(packet.lobby_name), "%s", lobby->name);
  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_JOINED,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_JOINED));
}

static void BroadcastLobbyRoster(ENetHost* host, uint32_t lobby_id) {
  ShroomLobbyRosterEntry entries[SHROOM_MAX_PARTICIPANTS] = {0};
  uint16_t roster_count = 0u;
  uint16_t chunk_count;
  uint32_t generation;
  bool match_started = false;
  size_t index;

  if ((host == NULL) || (lobby_id == 0u)) {
    return;
  }

  for (index = 0; index < host->peerCount && roster_count < SHROOM_MAX_PARTICIPANTS; ++index) {
    const ENetPeer* peer = &host->peers[index];
    const ServerSession* session = (const ServerSession*)peer->data;
    ShroomLobbyRosterEntry* entry;

    if ((peer->state != ENET_PEER_STATE_CONNECTED) || (session == NULL) || !session->active ||
        (session->lobby_id != lobby_id)) {
      continue;
    }
    entry = &entries[roster_count++];
    entry->player_id = session->player_id;
    entry->is_spectator = session->spectating ? 1u : 0u;
    entry->is_ready = session->is_ready ? 1u : 0u;
    entry->entered_match = session->entered_match ? 1u : 0u;
    if (session->entered_match) {
      match_started = true;
    }
  }

  if (g_lobby_roster_generation == UINT32_MAX) {
    g_lobby_roster_generation = 1u;
  } else {
    ++g_lobby_roster_generation;
  }
  generation = g_lobby_roster_generation;
  chunk_count = roster_count == 0u
                    ? 1u
                    : (uint16_t)((roster_count + SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET - 1u) /
                                 SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET);
  for (uint16_t chunk_index = 0u; chunk_index < chunk_count; ++chunk_index) {
    ShroomLobbyRosterPacket packet = {.lobby_id = lobby_id,
                                      .generation = generation,
                                      .total_player_count = roster_count,
                                      .chunk_index = chunk_index,
                                      .chunk_count = chunk_count,
                                      .match_started = match_started ? 1u : 0u};
    const size_t first_entry = (size_t)chunk_index * SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET;
    const size_t remaining = roster_count > first_entry ? roster_count - first_entry : 0u;
    const size_t entry_count = remaining < SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET
                                   ? remaining
                                   : SHROOM_LOBBY_ROSTER_ENTRIES_PER_PACKET;
    const size_t packet_size = SHROOM_LOBBY_ROSTER_PACKET_SIZE(entry_count);

    packet.entry_count = (uint16_t)entry_count;
    if (entry_count > 0u) {
      memcpy(packet.players, &entries[first_entry], entry_count * sizeof(entries[0]));
    }
    ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_ROSTER, (uint16_t)packet_size);
    for (index = 0; index < host->peerCount; ++index) {
      ENetPeer* peer = &host->peers[index];
      const ServerSession* session = (const ServerSession*)peer->data;

      if ((peer->state != ENET_PEER_STATE_CONNECTED) || (session == NULL) || !session->active ||
          (session->lobby_id != lobby_id)) {
        continue;
      }
      SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_ROSTER,
                 CreateProtocolPacket(&packet, packet_size, SHROOM_PACKET_LOBBY_ROSTER));
    }
  }
  enet_host_flush(host);
}

static const ShroomIntermissionVoter* FindIntermissionVoter(const ShroomIntermissionState* state,
                                                            uint32_t player_id) {
  for (uint16_t index = 0; index < state->voter_count; ++index) {
    if (state->voters[index].player_id == player_id) {
      return &state->voters[index];
    }
  }
  return NULL;
}

static void BroadcastIntermissionStatus(ENetHost* host, const ShroomLobby* lobby) {
  for (size_t index = 0; index < host->peerCount; ++index) {
    ENetPeer* peer = &host->peers[index];
    const ServerSession* session = (const ServerSession*)peer->data;
    const ShroomIntermissionVoter* voter;
    ShroomIntermissionStatusPacket packet;

    if ((peer->state != ENET_PEER_STATE_CONNECTED) || (session == NULL) || !session->active ||
        (session->lobby_id != lobby->lobby_id)) {
      continue;
    }
    voter = FindIntermissionVoter(&lobby->intermission, session->player_id);
    packet = (ShroomIntermissionStatusPacket){
        .round_id = lobby->intermission.round_id,
        .seconds_remaining = lobby->world.match_results_time_remaining,
        .eligible_count = lobby->intermission.eligible_count,
        .play_again_votes = lobby->intermission.play_again_votes,
        .return_to_lobby_votes = lobby->intermission.return_to_lobby_votes,
        .spectate_votes = lobby->intermission.spectate_votes,
        .resolved = lobby->intermission.resolved ? 1u : 0u,
        .decision = lobby->intermission.decision,
        .your_vote = voter != NULL ? voter->vote : SHROOM_REMATCH_VOTE_NONE,
        .can_vote = (voter != NULL) && voter->eligible && !lobby->intermission.resolved ? 1u : 0u,
    };
    ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_INTERMISSION_STATUS, sizeof(packet));
    SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_INTERMISSION_STATUS,
               CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_INTERMISSION_STATUS));
  }
}

static void BeginIntermission(ENetHost* host, ShroomLobby* lobby) {
  uint32_t player_ids[SHROOM_MAX_PARTICIPANTS];
  uint16_t count = 0u;

  for (size_t index = 0; (index < host->peerCount) && (count < SHROOM_MAX_PARTICIPANTS); ++index) {
    const ENetPeer* peer = &host->peers[index];
    const ServerSession* session = (const ServerSession*)peer->data;
    if ((peer->state == ENET_PEER_STATE_CONNECTED) && (session != NULL) && session->active &&
        !session->spectating && session->entered_match && (session->player_id != 0u) &&
        (session->lobby_id == lobby->lobby_id)) {
      player_ids[count++] = session->player_id;
    }
  }
  ShroomIntermissionBegin(&lobby->intermission, player_ids, count, ++lobby->next_round_id);
  lobby->last_intermission_second = UINT16_MAX;
  BroadcastIntermissionStatus(host, lobby);
}

static void ResolveIntermission(ENetHost* host, ShroomLobby* lobby, sqlite3* db) {
  const ShroomRematchVote decision = ShroomIntermissionResolve(&lobby->intermission);

  BroadcastIntermissionStatus(host, lobby);
  for (size_t index = 0; index < host->peerCount; ++index) {
    ServerSession* session = (ServerSession*)host->peers[index].data;
    if ((session == NULL) || !session->active || (session->lobby_id != lobby->lobby_id)) {
      continue;
    }
    session->is_ready = false;
    if (!session->spectating &&
        ShroomIntermissionPlayerContinuesMatch(&lobby->intermission, session->player_id)) {
      session->entered_match = true;
    } else {
      session->entered_match = false;
      if (decision == SHROOM_REMATCH_VOTE_SPECTATE && !session->spectating) {
        ShroomServerCleanupPlayer(&lobby->world, session->player_id, &session->player,
                                  &session->focused_entity_id);
        session->spectating = true;
      }
    }
  }
  ShroomWorldResetMatch(&lobby->world);
  BeginLobbyPersistenceRound(lobby);
  for (size_t index = 0; index < host->peerCount; ++index) {
    const ServerSession* session = (const ServerSession*)host->peers[index].data;
    if ((host->peers[index].state == ENET_PEER_STATE_CONNECTED) && (session != NULL) &&
        session->active && session->entered_match && (session->lobby_id == lobby->lobby_id)) {
      RegisterPersistenceParticipant(lobby, db, session);
    }
  }
  BroadcastLobbyRoster(host, lobby->lobby_id);
  LOG_INFO("intermission resolved: lobby_id=%u round=%u decision=%u", lobby->lobby_id,
           lobby->intermission.round_id, (unsigned)decision);
}

static void SendLobbyCreated(ENetPeer* peer, const ShroomLobby* lobby) {
  ShroomLobbyCreatedPacket packet = {0};

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_CREATED, sizeof(packet));
  packet.lobby_id = lobby->lobby_id;
  snprintf(packet.name, sizeof(packet.name), "%s", lobby->name);
  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_LOBBY_CREATED,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_LOBBY_CREATED));
}

static void SendPong(ENetPeer* peer, uint32_t nonce) {
  const ShroomPongPacket packet = {
      .header = {SHROOM_PACKET_PONG, SHROOM_ENET_CHANNEL_CONTROL, sizeof(ShroomPongPacket)},
      .nonce = nonce,
  };

  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_PONG,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_PONG));
}

static uint16_t CountAdvertisedPlayers(const ENetHost* host);

static void SendServerProbeResponse(const ENetHost* host, ENetPeer* peer,
                                    const ShroomServerProbePacket* probe) {
  ShroomServerProbeResponsePacket response = {0};

  if ((host == NULL) || (peer == NULL) || (probe == NULL) ||
      (probe->protocol_version != SHROOM_PROTOCOL_VERSION) || (probe->generation == 0u) ||
      (probe->nonce == 0u)) {
    return;
  }
  ShroomPacketHeaderInit(&response.header, SHROOM_PACKET_SERVER_PROBE_RESPONSE, sizeof(response));
  response.protocol_version = SHROOM_PROTOCOL_VERSION;
  response.generation = probe->generation;
  response.nonce = probe->nonce;
  response.player_count = CountAdvertisedPlayers(host);
  response.capacity = SHROOM_SERVER_MAX_CLIENTS;
  SendPacket(
      peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_SERVER_PROBE_RESPONSE,
      CreateProtocolPacket(&response, sizeof(response), SHROOM_PACKET_SERVER_PROBE_RESPONSE));
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

  SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_AUTH_RESPONSE,
             CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_AUTH_RESPONSE));
}

static void SendSnapshot(ENetPeer* peer, ServerSession* session, const ShroomWorldState* world) {
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotEncodedFrame encoded;
  uint16_t selected_indices[SHROOM_MAX_SNAPSHOT_PLAYERS];
  const ShroomSnapshotFrame* baseline;
  size_t selected_count;
  bool keyframe;

  if ((session == NULL) || !session->active) {
    return;
  }
  if (!session->spectating && session->player == NULL) {
    return;
  }

  selected_count = ShroomSnapshotSelectPlayers(
      &session->snapshot_interest, world, session->player_id, session->focused_entity_id,
      session->spectating, selected_indices, SHROOM_MAX_SNAPSHOT_PLAYERS);
  metadata = (ShroomSnapshotFrameMetadata){
      .tick = world->tick,
      .last_processed_input_sequence = session->last_processed_input_sequence,
      .player_id = session->player_id,
      .entity_id = (session->player != NULL) ? session->player->entity_id : 0u,
      .match_phase = (uint8_t)world->match_phase,
      .game_mode = (uint8_t)world->game_mode,
      .match_time_remaining = world->match_time_remaining,
      .objective_target_score = world->objective_target_score,
      .objective_controller_id = world->objective_controller_id,
      .objective_contested = world->objective_contested ? 1u : 0u,
  };
  memcpy(metadata.podium_player_ids, world->podium_player_ids, sizeof(metadata.podium_player_ids));
  memcpy(metadata.podium_masses, world->podium_masses, sizeof(metadata.podium_masses));

  for (uint16_t index = 0u; index < (uint16_t)selected_count; ++index) {
    const ShroomPlayerState* player = &world->players[selected_indices[index]];
    ShroomSnapshotPlayerState* snapshot = &players[index];

    *snapshot = (ShroomSnapshotPlayerState){
        .player_id = player->player_id,
        .entity_id = player->entity_id,
        .position_x = player->position.x,
        .position_y = player->position.y,
        .mass = player->mass,
        .radius = player->radius,
        .objective_score = ShroomWorldGetObjectiveScore(world, player->player_id),
        .alive = 1u,
        .is_bot = player->is_bot ? 1u : 0u,
        .piece_index = player->piece_index,
        .life_generation = player->life_generation,
        .effect_flags =
            (uint16_t)((player->speed_powerup_timer > 0.0f ? SHROOM_POWERUP_EFFECT_SPEED : 0u) |
                       (player->shield_powerup_timer > 0.0f ? SHROOM_POWERUP_EFFECT_SHIELD : 0u) |
                       (player->magnet_powerup_timer > 0.0f ? SHROOM_POWERUP_EFFECT_MAGNET : 0u) |
                       (player->decay_immune_powerup_timer > 0.0f
                            ? SHROOM_POWERUP_EFFECT_DECAY_IMMUNE
                            : 0u)),
    };
    ShroomServerPopulateSnapshotRoundStats(world, player->player_id, snapshot);
    snprintf(snapshot->name, sizeof(snapshot->name), "%s", player->name);
  }

  baseline =
      ShroomSnapshotHistoryFind(&session->snapshot_history, session->acknowledged_snapshot_tick);
  keyframe = (baseline == NULL) || ((world->tick % SHROOM_SNAPSHOT_KEYFRAME_TICKS) == 0u);
  if (!ShroomSnapshotEncodeFrame(&metadata, players, (uint16_t)selected_count, baseline, keyframe,
                                 &encoded)) {
    return;
  }
  for (uint16_t index = 0u; index < encoded.packet_count; ++index) {
    const size_t wire_size = encoded.packets[index].header.size;
    SendPacket(peer, SHROOM_ENET_CHANNEL_SNAPSHOT, SHROOM_PACKET_SNAPSHOT,
               CreateProtocolPacket(&encoded.packets[index], wire_size, SHROOM_PACKET_SNAPSHOT));
  }
  ShroomSnapshotHistoryStore(&session->snapshot_history, world->tick, players,
                             (uint16_t)selected_count);
}

static ShroomVec2 ReplicationInterestCenter(const ServerSession* session,
                                            const ShroomWorldState* world) {
  if ((session != NULL) && (session->player != NULL) && session->player->alive) {
    return session->player->position;
  }
  for (size_t index = 0u; index < world->player_count; ++index) {
    if (world->players[index].alive && (world->players[index].piece_index == 0u)) {
      return world->players[index].position;
    }
  }
  return (ShroomVec2){world->width * 0.5f, world->height * 0.5f};
}

static void SendWorldState(ENetPeer* peer, ServerSession* session, const ShroomWorldState* world) {
  ShroomWorldStateRecord records[SHROOM_MAX_WORLD_STATE_RECORDS];
  const ShroomWorldReplicationBatch batch = ShroomWorldReplicationBuild(
      &session->world_replication, world, ReplicationInterestCenter(session, world), false, records,
      SHROOM_MAX_WORLD_STATE_RECORDS);
  const uint16_t records_per_packet = ShroomWorldStatePacketMaxRecords();
  const uint16_t chunk_count = (uint16_t)ShroomWorldReplicationPacketCount(batch.record_count);

  if (!batch.keyframe && (batch.record_count == 0u)) {
    return;
  }
  for (uint16_t chunk_index = 0u; chunk_index < chunk_count; ++chunk_index) {
    const size_t record_start = (size_t)chunk_index * records_per_packet;
    const size_t remaining = batch.record_count - record_start;
    const uint8_t record_count =
        (uint8_t)(remaining < records_per_packet ? remaining : records_per_packet);
    const size_t packet_size = offsetof(ShroomWorldStatePacket, records) +
                               (size_t)record_count * sizeof(ShroomWorldStateRecord);
    ENetPacket* enet_packet = enet_packet_create(NULL, packet_size, 0u);
    ShroomWorldStatePacket* packet;

    if (enet_packet == NULL) {
      continue;
    }
    packet = (ShroomWorldStatePacket*)enet_packet->data;
    memset(packet, 0, offsetof(ShroomWorldStatePacket, records));
    ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_WORLD_STATE, (uint16_t)packet_size);
    packet->tick = batch.tick;
    packet->chunk_index = chunk_index;
    packet->chunk_count = chunk_count;
    packet->flags = batch.keyframe ? SHROOM_WORLD_STATE_FLAG_KEYFRAME : 0u;
    packet->record_count = record_count;
    if (record_count > 0u) {
      memcpy(packet->records, &records[record_start],
             (size_t)record_count * sizeof(ShroomWorldStateRecord));
    }
    SendPacket(peer, SHROOM_ENET_CHANNEL_SNAPSHOT, SHROOM_PACKET_WORLD_STATE, enet_packet);
  }
}

static void RemoveSessionPlayer(ServerSession* session, ShroomLobby* lobbies) {
  ShroomLobby* lobby = NULL;

  if (session == NULL) {
    return;
  }

  if ((lobbies != NULL) && (session->player_id != 0u)) {
    lobby = FindLobbyById(lobbies, session->lobby_id);
  }
  ShroomServerCleanupPlayer(lobby != NULL ? &lobby->world : NULL, session->player_id,
                            &session->player, &session->focused_entity_id);
}

static void DisconnectSession(ServerSession* session, ShroomLobby* lobbies) {
  if ((session == NULL) || !session->active) {
    return;
  }

  RemoveSessionPlayer(session, lobbies);
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

  char raw_name[SHROOM_MAX_NAME_LENGTH];
  memcpy(raw_name, packet->name, sizeof(raw_name));
  raw_name[sizeof(raw_name) - 1u] = '\0';
  ShroomSanitizePlayerName(session->display_name, raw_name);

  session->active = true;
  session->handshake_received = true;
  LOG_INFO("session activated: slot=%u", (unsigned)peer->incomingPeerID);
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
  if (!session->entered_match) {
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

  /* Human split pieces without focus keep their last direction; bot pieces keep AI. */
  if (world != NULL) {
    for (i = 0; i < world->player_count; ++i) {
      ShroomPlayerState* p = &world->players[i];
      if (p->alive && (p->player_id == session->player_id)) {
        p->ai_controlled = p->is_bot && (p->entity_id != session->focused_entity_id);
      }
    }
  }

  if (packet->split_requested && (world != NULL)) {
    ShroomWorldSplitPlayerToward(
        world, target_piece,
        NormalizeInput((ShroomVec2){packet->split_direction_x, packet->split_direction_y}));
  }
  if (packet->eject_requested && (world != NULL)) {
    ShroomWorldEjectMass(
        world, target_piece,
        NormalizeInput((ShroomVec2){packet->split_direction_x, packet->split_direction_y}));
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
      SendPacket(peer, SHROOM_ENET_CHANNEL_CHAT, SHROOM_PACKET_CHAT, out);
    }
  }
  enet_host_flush(host);
}

static void HandleReadyStatePacket(ENetHost* host, ServerSession* session,
                                   const ENetPacket* enet_packet) {
  const ShroomReadyStatePacket* packet;

  if ((host == NULL) || (session == NULL) || !session->active || (session->player == NULL) ||
      (enet_packet == NULL) || (enet_packet->dataLength < sizeof(ShroomReadyStatePacket))) {
    return;
  }

  packet = (const ShroomReadyStatePacket*)enet_packet->data;
  if ((session->lobby_id == 0u) || (packet->player_id != session->player_id)) {
    return;
  }
  session->is_ready = packet->is_ready != 0;

  LOG_INFO("ready state player_id=%u ready=%d", session->player_id, session->is_ready);
  BroadcastLobbyRoster(host, session->lobby_id);
}

static void HandleEnterMatchPacket(ENetHost* host, ServerSession* session, ShroomLobby* lobbies,
                                   sqlite3* db, const ENetPacket* enet_packet) {
  const ShroomEnterMatchPacket* packet;

  if ((host == NULL) || (session == NULL) || !session->active || (enet_packet == NULL) ||
      (enet_packet->dataLength < sizeof(ShroomEnterMatchPacket))) {
    return;
  }

  packet = (const ShroomEnterMatchPacket*)enet_packet->data;
  if ((session->lobby_id == 0u) || (packet->lobby_id != session->lobby_id) ||
      (!session->spectating && ((session->player == NULL) || !session->is_ready))) {
    return;
  }
  if (session->entered_match) {
    return;
  }

  session->entered_match = true;
  RegisterPersistenceParticipant(FindLobbyById(lobbies, session->lobby_id), db, session);
  session->focused_entity_id = 0u;
  if (session->player != NULL) {
    session->player->input_direction = (ShroomVec2){0};
    session->player->spawn_protection_timer = SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS;
  }
  LOG_INFO("match entry: player_id=%u lobby_id=%u spectating=%d", session->player_id,
           session->lobby_id, (int)session->spectating);
  BroadcastLobbyRoster(host, session->lobby_id);
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
  ShroomPlayerId removed_player_id = 0u;

  if (lobby == NULL) {
    return false;
  }

  for (i = lobby->world.player_count; i > 0; --i) {
    const ShroomPlayerState* player = &lobby->world.players[i - 1u];

    if (!player->alive || !player->is_bot || (player->piece_index != 0u)) {
      continue;
    }
    removed_player_id = player->player_id;
    break;
  }

  if (removed_player_id == 0u) {
    return false;
  }
  for (i = 0u; i < lobby->world.player_count; ++i) {
    ShroomPlayerState* player = &lobby->world.players[i];

    if (player->player_id == removed_player_id) {
      *player = (ShroomPlayerState){0};
    }
  }
  return true;
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
                                bool is_dynamic, ShroomGameMode game_mode,
                                uint32_t* next_player_id) {
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
      ShroomWorldSetGameMode(&lobby->world, game_mode);
      ShroomWorldSetMatchDuration(&lobby->world, g_match_duration_seconds);
      ShroomSnapshotSchedulerInit(&lobby->snapshot_scheduler, (uint32_t)SHROOM_SERVER_TICK_RATE,
                                  g_snapshot_rate);
      BeginLobbyPersistenceRound(lobby);
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

static void HandleLobbyJoin(ENetHost* host, ENetPeer* peer, ServerSession* session,
                            ShroomLobby* lobbies, ShroomVoiceRelay* voice_relay,
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

  if (!packet->spectate) {
    const uint16_t real_player_count = CountLobbyRealPlayers(host, lobby->lobby_id);
    const ShroomLobbyAdmissionPlan admission =
        ShroomLobbyPlanAdmission(real_player_count, CountLobbyAliveBots(lobby));

    if (!admission.accepted) {
      return;
    }
    for (uint16_t index = 0u; index < admission.bots_to_remove; ++index) {
      if (!RemoveLobbyBot(lobby)) {
        return;
      }
    }
  }

  session->lobby_id = lobby->lobby_id;
  memset(&session->world_replication, 0, sizeof(session->world_replication));
  memset(&session->snapshot_interest, 0, sizeof(session->snapshot_interest));
  memset(&session->snapshot_history, 0, sizeof(session->snapshot_history));
  session->acknowledged_snapshot_tick = 0u;
  session->spectating = (packet->spectate != 0);
  session->is_ready = false;
  session->entered_match = false;

  if (!session->spectating) {
    session->player = ShroomWorldSpawnPlayer(&lobby->world, (*next_player_id)++, false);
    if (session->player == NULL) {
      session->lobby_id = 0;
      return;
    }
    session->player_id = session->player->player_id;
    snprintf(session->player->name, sizeof(session->player->name), "%s", session->display_name);
  }

  ShroomVoiceRelaySetPeer(voice_relay, peer->incomingPeerID, true, session->lobby_id,
                          session->player_id);

  SendLobbyJoined(peer, session, lobby);
  LOG_INFO("lobby join: player_id=%u lobby_id=%u spectating=%d", session->player_id,
           lobby->lobby_id, (int)session->spectating);
  BroadcastLobbyRoster(host, lobby->lobby_id);
  if (lobby->world.match_phase == SHROOM_MATCH_PHASE_RESULTS) {
    BroadcastIntermissionStatus(host, lobby);
  }
}

static void HandleLobbyLeave(ENetHost* host, const ENetPeer* peer, ServerSession* session,
                             ShroomLobby* lobbies, ShroomVoiceRelay* voice_relay) {
  uint32_t lobby_id;

  if (session == NULL) {
    return;
  }
  lobby_id = session->lobby_id;
  {
    ShroomLobby* lobby = FindLobbyById(lobbies, lobby_id);
    if ((lobby != NULL) &&
        ShroomIntermissionRemoveVoter(&lobby->intermission, session->player_id)) {
      BroadcastIntermissionStatus(host, lobby);
    }
    if ((lobby != NULL) && session->entered_match) {
      CapturePersistenceParticipant(lobby, session->player_id, true);
    }
  }
  RemoveSessionPlayer(session, lobbies);
  LOG_INFO("lobby leave: player_id=%u lobby_id=%u", session->player_id, session->lobby_id);
  session->lobby_id = 0;
  memset(&session->world_replication, 0, sizeof(session->world_replication));
  memset(&session->snapshot_interest, 0, sizeof(session->snapshot_interest));
  memset(&session->snapshot_history, 0, sizeof(session->snapshot_history));
  session->acknowledged_snapshot_tick = 0u;
  session->spectating = false;
  session->is_ready = false;
  session->entered_match = false;
  session->player_id = 0;
  if (peer != NULL) {
    ShroomVoiceRelaySetPeer(voice_relay, peer->incomingPeerID, true, 0u, 0u);
  }
  BroadcastLobbyRoster(host, lobby_id);
}

static void HandleVoiceFramePacket(ENetHost* host, const ENetPeer* peer,
                                   const ServerSession* session, ShroomVoiceRelay* voice_relay,
                                   const ENetPacket* enet_packet, uint64_t now_ms) {
  if ((host == NULL) || (peer == NULL) || (session == NULL) || !session->active ||
      !session->handshake_received || (voice_relay == NULL) || (enet_packet == NULL)) {
    return;
  }

  ShroomVoiceRelaySetPeer(voice_relay, peer->incomingPeerID, true, session->lobby_id,
                          session->player_id);
  (void)ShroomVoiceRelayRoute(voice_relay, peer->incomingPeerID, enet_packet->data,
                              enet_packet->dataLength, now_ms, SendVoiceRelayPacket, host, NULL);
}

static void HandleLobbyCreate(ENetPeer* peer, const ServerSession* session, ShroomLobby* lobbies,
                              const ENetPacket* enet_packet, uint32_t* next_player_id,
                              uint32_t* next_lobby_id) {
  const ShroomLobbyCreatePacket* packet = (const ShroomLobbyCreatePacket*)enet_packet->data;
  const ShroomLobby* lobby;

  if (!session->handshake_received || (enet_packet->dataLength < sizeof(*packet))) {
    return;
  }

  if ((packet->game_mode != SHROOM_GAME_MODE_FFA) &&
      (packet->game_mode != SHROOM_GAME_MODE_KING_OF_HILL)) {
    return;
  }
  lobby = CreateLobby(lobbies, (*next_lobby_id)++, packet->name, true,
                      (ShroomGameMode)packet->game_mode, next_player_id);
  if (lobby == NULL) {
    return;
  }
  SendLobbyCreated(peer, lobby);
}

typedef struct ServerPacketContext {
  ENetHost* host;
  ENetPeer* peer;
  ServerSession* session;
  ShroomLobby* lobbies;
  ShroomAuthContext* auth_ctx;
  ShroomVoiceRelay* voice_relay;
  const ENetPacket* enet_packet;
  uint32_t* next_player_id;
  uint32_t* next_lobby_id;
  uint64_t now_ms;
} ServerPacketContext;

typedef void (*ServerPacketHandler)(ServerPacketContext* context);

typedef struct ServerPacketDispatchEntry {
  ShroomPacketType type;
  ServerPacketHandler handler;
} ServerPacketDispatchEntry;

static void DispatchHelloPacket(ServerPacketContext* context) {
  HandleHelloPacket(context->peer, context->session,
                    context->auth_ctx != NULL ? context->auth_ctx->db : NULL, context->enet_packet);
}

static void DispatchInputPacket(ServerPacketContext* context) {
  ShroomLobby* input_lobby = FindLobbyById(context->lobbies, context->session->lobby_id);
  HandleInputPacket(context->session, context->enet_packet,
                    input_lobby != NULL ? &input_lobby->world : NULL);
}

static void DispatchSnapshotAckPacket(ServerPacketContext* context) {
  const ShroomSnapshotAckPacket* packet =
      (const ShroomSnapshotAckPacket*)context->enet_packet->data;
  if ((packet->header.size != sizeof(*packet)) ||
      (context->enet_packet->dataLength != sizeof(*packet)) ||
      (ShroomSnapshotHistoryFind(&context->session->snapshot_history, packet->tick) == NULL)) {
    return;
  }
  if ((context->session->acknowledged_snapshot_tick == 0u) ||
      ShroomSnapshotTickIsNewer(packet->tick, context->session->acknowledged_snapshot_tick)) {
    context->session->acknowledged_snapshot_tick = packet->tick;
  }
}

static void DispatchAuthRequestPacket(ServerPacketContext* context) {
  HandleAuthRequestPacket(context->peer, context->session, context->auth_ctx, context->enet_packet);
}

static void DispatchPingPacket(ServerPacketContext* context) {
  SendPong(context->peer, ((const ShroomPingPacket*)context->enet_packet->data)->nonce);
}

static void DispatchServerProbePacket(ServerPacketContext* context) {
  if (context->enet_packet->dataLength == sizeof(ShroomServerProbePacket)) {
    SendServerProbeResponse(context->host, context->peer,
                            (const ShroomServerProbePacket*)context->enet_packet->data);
  }
}

static void DispatchChatPacket(ServerPacketContext* context) {
  HandleChatPacket(context->host, context->session, context->enet_packet, context->now_ms);
}

static void DispatchVoiceFramePacket(ServerPacketContext* context) {
  HandleVoiceFramePacket(context->host, context->peer, context->session, context->voice_relay,
                         context->enet_packet, context->now_ms);
}

static void DispatchLobbyListQuery(ServerPacketContext* context) {
  HandleLobbyListQuery(context->peer, context->session, context->lobbies, context->host);
}

static void DispatchLobbyJoin(ServerPacketContext* context) {
  HandleLobbyJoin(context->host, context->peer, context->session, context->lobbies,
                  context->voice_relay, context->enet_packet, context->next_player_id);
}

static void DispatchLobbyLeave(ServerPacketContext* context) {
  HandleLobbyLeave(context->host, context->peer, context->session, context->lobbies,
                   context->voice_relay);
}

static void DispatchLobbyCreate(ServerPacketContext* context) {
  HandleLobbyCreate(context->peer, context->session, context->lobbies, context->enet_packet,
                    context->next_player_id, context->next_lobby_id);
}

static void DispatchReadyState(ServerPacketContext* context) {
  HandleReadyStatePacket(context->host, context->session, context->enet_packet);
}

static void DispatchEnterMatch(ServerPacketContext* context) {
  HandleEnterMatchPacket(context->host, context->session, context->lobbies,
                         context->auth_ctx != NULL ? context->auth_ctx->db : NULL,
                         context->enet_packet);
}

static void DispatchRematchVote(ServerPacketContext* context) {
  const ShroomRematchVotePacket* packet =
      (const ShroomRematchVotePacket*)context->enet_packet->data;
  ShroomLobby* lobby = FindLobbyById(context->lobbies, context->session->lobby_id);

  if ((lobby == NULL) || !context->session->active || context->session->spectating ||
      (lobby->world.match_phase != SHROOM_MATCH_PHASE_RESULTS) ||
      (packet->round_id != lobby->intermission.round_id)) {
    return;
  }
  if (ShroomIntermissionCastVote(&lobby->intermission, context->session->player_id,
                                 (ShroomRematchVote)packet->vote)) {
    BroadcastIntermissionStatus(context->host, lobby);
  }
}

static const ServerPacketDispatchEntry kServerPacketDispatch[] = {
    {SHROOM_PACKET_HELLO, DispatchHelloPacket},
    {SHROOM_PACKET_INPUT, DispatchInputPacket},
    {SHROOM_PACKET_SNAPSHOT_ACK, DispatchSnapshotAckPacket},
    {SHROOM_PACKET_AUTH_REQUEST, DispatchAuthRequestPacket},
    {SHROOM_PACKET_PING, DispatchPingPacket},
    {SHROOM_PACKET_SERVER_PROBE, DispatchServerProbePacket},
    {SHROOM_PACKET_CHAT, DispatchChatPacket},
    {SHROOM_PACKET_VOICE_FRAME, DispatchVoiceFramePacket},
    {SHROOM_PACKET_LOBBY_LIST_QUERY, DispatchLobbyListQuery},
    {SHROOM_PACKET_LOBBY_JOIN, DispatchLobbyJoin},
    {SHROOM_PACKET_LOBBY_LEAVE, DispatchLobbyLeave},
    {SHROOM_PACKET_LOBBY_CREATE, DispatchLobbyCreate},
    {SHROOM_PACKET_READY_STATE, DispatchReadyState},
    {SHROOM_PACKET_ENTER_MATCH, DispatchEnterMatch},
    {SHROOM_PACKET_REMATCH_VOTE, DispatchRematchVote},
};

static const ServerPacketDispatchEntry* FindServerPacketDispatchEntry(ShroomPacketType type) {
  for (size_t index = 0; index < sizeof(kServerPacketDispatch) / sizeof(kServerPacketDispatch[0]);
       ++index) {
    if (kServerPacketDispatch[index].type == type) {
      return &kServerPacketDispatch[index];
    }
  }
  return NULL;
}

static void HandlePacket(ENetHost* host, ENetPeer* peer, ServerSession* session,
                         ShroomLobby* lobbies, ShroomAuthContext* auth_ctx,
                         ShroomVoiceRelay* voice_relay, const ENetPacket* enet_packet,
                         uint8_t channel_id, uint32_t* next_player_id, uint32_t* next_lobby_id,
                         uint64_t now_ms) {
  const ShroomPacketHeader* header;
  const ServerPacketDispatchEntry* entry;
  ShroomPacketType packet_type;
  size_t minimum_size;
  ServerPacketContext context;
  const size_t peer_index = peer != NULL ? peer->incomingPeerID : SHROOM_NET_TELEMETRY_MAX_PEERS;

  if ((enet_packet == 0) || (enet_packet->dataLength < sizeof(ShroomPacketHeader))) {
    ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel_id,
                                 (ShroomPacketType)0,
                                 enet_packet != NULL ? enet_packet->dataLength : 0u, now_ms);
    return;
  }

  header = (const ShroomPacketHeader*)enet_packet->data;
  if (!ShroomPacketHeaderUsesExpectedChannel(header, channel_id)) {
    ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel_id,
                                 (ShroomPacketType)header->type, enet_packet->dataLength, now_ms);
    return;
  }

  packet_type = (ShroomPacketType)header->type;
  minimum_size = ShroomPacketTypeMinimumSize(packet_type);
  if ((minimum_size == 0u) || (enet_packet->dataLength < minimum_size)) {
    ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel_id, packet_type,
                                 enet_packet->dataLength, now_ms);
    return;
  }

  entry = FindServerPacketDispatchEntry(packet_type);
  if (entry == NULL) {
    ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel_id, packet_type,
                                 enet_packet->dataLength, now_ms);
    return;
  }

  if (packet_type == SHROOM_PACKET_INPUT) {
    ShroomInputAdmissionResult admission_result;
    const ShroomInputPacket* input = (const ShroomInputPacket*)enet_packet->data;

    if ((session == NULL) || !session->active || session->spectating || !session->entered_match ||
        (session->player == NULL)) {
      ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel_id, packet_type,
                                   enet_packet->dataLength, now_ms);
      return;
    }
    admission_result =
        ShroomInputAdmissionCheck(&session->input_admission, input->sequence, now_ms);
    if (admission_result != SHROOM_INPUT_ADMITTED) {
      if (admission_result == SHROOM_INPUT_REJECTED_STALE) {
        g_server_input_stale_rejections += 1u;
      } else {
        g_server_input_rate_rejections += 1u;
      }
      ShroomNetTelemetryRecordDrop(&g_server_net_telemetry, peer_index, channel_id, packet_type,
                                   enet_packet->dataLength, now_ms);
      return;
    }
  }

  ShroomNetTelemetryRecordAccepted(&g_server_net_telemetry, peer_index, channel_id, packet_type,
                                   enet_packet->dataLength, now_ms);

  context = (ServerPacketContext){
      .host = host,
      .peer = peer,
      .session = session,
      .lobbies = lobbies,
      .auth_ctx = auth_ctx,
      .voice_relay = voice_relay,
      .enet_packet = enet_packet,
      .next_player_id = next_player_id,
      .next_lobby_id = next_lobby_id,
      .now_ms = now_ms,
  };
  entry->handler(&context);
}

static void SendDirectoryList(ENetPeer* peer, const ShroomDirectoryRegistry* registry,
                              uint32_t generation) {
  ShroomDirectoryServerEntry entries[SHROOM_DIRECTORY_MAX_ENTRIES];
  const size_t entry_count =
      ShroomDirectoryRegistryCopyActive(registry, entries, SHROOM_DIRECTORY_MAX_ENTRIES);
  const size_t chunk_count = ShroomDirectoryListChunkCount(entry_count);

  for (size_t chunk_index = 0u; chunk_index < chunk_count; ++chunk_index) {
    ShroomDirectoryListPacket packet = {0};
    const size_t packet_size =
        ShroomDirectoryBuildListPacket(entries, entry_count, generation, chunk_index, &packet);
    SendPacket(peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_DIRECTORY_LIST,
               CreateProtocolPacket(&packet, packet_size, SHROOM_PACKET_DIRECTORY_LIST));
  }
}

static int RunDirectoryServer(const ServerConfig* config) {
  ENetAddress address = {.host = config->bind_address, .port = config->directory_port};
  ENetHost* host;
  ShroomDirectoryRegistry registry;

  if (enet_initialize() != 0) {
    LOG_ERROR("failed to initialize ENet for directory service");
    return 1;
  }
  host = enet_host_create(&address, 128u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if (host == NULL) {
    LOG_ERROR("failed to create directory ENet host");
    enet_deinitialize();
    return 1;
  }

  ShroomDirectoryRegistryInit(&registry);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_START);
  LOG_INFO("shroomio directory listening on %s:%u/udp", config->bind_host, config->directory_port);
  if (config->smoke_test) {
    ShroomLifecycleRequestShutdown(&g_lifecycle);
  }

  while (ShroomLifecycleIsRunning(&g_lifecycle) &&
         !ShroomLifecycleIsShutdownRequested(&g_lifecycle)) {
    ENetEvent event;
    const uint64_t now_ms = GetTimeMillis();
    uint32_t serviced_events = 0u;

    while ((serviced_events < SHROOM_DIRECTORY_MAX_EVENTS_PER_TICK) &&
           (enet_host_service(host, &event, 10u) > 0)) {
      ++serviced_events;
      switch (event.type) {
      case ENET_EVENT_TYPE_RECEIVE:
        if ((event.channelID == SHROOM_ENET_CHANNEL_CONTROL) &&
            (event.packet->dataLength >= sizeof(ShroomPacketHeader))) {
          const ShroomPacketHeader* header = (const ShroomPacketHeader*)event.packet->data;
          char observed_host[SHROOM_DIRECTORY_HOST_LENGTH] = {0};
          if ((header->type == SHROOM_PACKET_DIRECTORY_HEARTBEAT) &&
              (enet_address_get_host_ip(&event.peer->address, observed_host,
                                        sizeof(observed_host)) == 0) &&
              ShroomDirectoryRegistryRegister(
                  &registry, (const ShroomDirectoryHeartbeatPacket*)event.packet->data,
                  event.packet->dataLength, observed_host, now_ms)) {
            const ShroomDirectoryHeartbeatPacket* heartbeat =
                (const ShroomDirectoryHeartbeatPacket*)event.packet->data;
            LOG_INFO("directory heartbeat endpoint=%s:%u players=%u/%u", observed_host,
                     heartbeat->server.port, heartbeat->server.player_count,
                     heartbeat->server.capacity);
          } else if ((header->type == SHROOM_PACKET_DIRECTORY_QUERY) &&
                     ShroomDirectoryQueryIsValid(
                         (const ShroomDirectoryQueryPacket*)event.packet->data,
                         event.packet->dataLength)) {
            const ShroomDirectoryQueryPacket* query =
                (const ShroomDirectoryQueryPacket*)event.packet->data;
            ShroomDirectoryRegistryEvictExpired(&registry, now_ms);
            SendDirectoryList(event.peer, &registry, query->generation);
            enet_host_flush(host);
          }
        }
        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_CONNECT:
      case ENET_EVENT_TYPE_DISCONNECT:
      case ENET_EVENT_TYPE_NONE:
      default:
        break;
      }
    }
    ShroomDirectoryRegistryEvictExpired(&registry, now_ms);
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_STOP);
  enet_host_destroy(host);
  enet_deinitialize();
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN);
  LOG_INFO("shroomio directory shutting down");
  return 0;
}

static uint16_t CountAdvertisedPlayers(const ENetHost* host) {
  size_t count = 0u;

  if (host == NULL) {
    return 0u;
  }
  for (size_t index = 0u; index < host->peerCount; ++index) {
    const ServerSession* session = (const ServerSession*)host->peers[index].data;
    if ((host->peers[index].state == ENET_PEER_STATE_CONNECTED) && (session != NULL) &&
        session->active && !session->spectating) {
      ++count;
    }
  }
  return count > UINT16_MAX ? UINT16_MAX : (uint16_t)count;
}

static void DirectoryAdvertiserConnect(DirectoryAdvertiser* advertiser, const ServerConfig* config,
                                       uint64_t now_ms) {
  ENetAddress address = {.port = config->directory_port};

  advertiser->last_connect_attempt_ms = now_ms;
  if (enet_address_set_host(&address, config->directory_host) != 0) {
    LOG_WARN("directory host could not be resolved: %s", config->directory_host);
    return;
  }
  advertiser->peer = enet_host_connect(advertiser->host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (advertiser->peer == NULL) {
    LOG_WARN("directory connection could not be started");
  }
}

static bool DirectoryAdvertiserInit(DirectoryAdvertiser* advertiser, const ServerConfig* config,
                                    uint64_t now_ms) {
  *advertiser = (DirectoryAdvertiser){0};
  if (config->directory_host[0] == '\0') {
    return true;
  }
  advertiser->host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if (advertiser->host == NULL) {
    LOG_WARN("directory advertiser host could not be created");
    return false;
  }
  DirectoryAdvertiserConnect(advertiser, config, now_ms);
  return true;
}

static void DirectoryAdvertiserUpdate(DirectoryAdvertiser* advertiser, const ServerConfig* config,
                                      const ENetHost* game_host, uint64_t now_ms) {
  ENetEvent event;

  if (advertiser->host == NULL) {
    return;
  }
  while (enet_host_service(advertiser->host, &event, 0u) > 0) {
    if (event.type == ENET_EVENT_TYPE_CONNECT) {
      advertiser->connected = true;
      advertiser->last_heartbeat_ms = 0ull;
      LOG_INFO("connected to directory %s:%u", config->directory_host, config->directory_port);
    } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      advertiser->connected = false;
      advertiser->peer = NULL;
      LOG_WARN("directory connection lost");
    } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
      enet_packet_destroy(event.packet);
    }
  }

  if ((advertiser->peer == NULL) &&
      ((now_ms - advertiser->last_connect_attempt_ms) >= SHROOM_DIRECTORY_RECONNECT_INTERVAL_MS)) {
    DirectoryAdvertiserConnect(advertiser, config, now_ms);
  }
  if (advertiser->connected &&
      ((advertiser->last_heartbeat_ms == 0ull) ||
       ((now_ms - advertiser->last_heartbeat_ms) >= SHROOM_DIRECTORY_HEARTBEAT_INTERVAL_MS))) {
    ShroomDirectoryHeartbeatPacket heartbeat = {0};

    ShroomPacketHeaderInit(&heartbeat.header, SHROOM_PACKET_DIRECTORY_HEARTBEAT, sizeof(heartbeat));
    heartbeat.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
    CopyConfigString(heartbeat.server.name, sizeof(heartbeat.server.name), config->server_name);
    heartbeat.server.port = config->port;
    heartbeat.server.player_count = CountAdvertisedPlayers(game_host);
    heartbeat.server.capacity = SHROOM_SERVER_MAX_CLIENTS;
    SendPacket(
        advertiser->peer, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_DIRECTORY_HEARTBEAT,
        CreateProtocolPacket(&heartbeat, sizeof(heartbeat), SHROOM_PACKET_DIRECTORY_HEARTBEAT));
    enet_host_flush(advertiser->host);
    advertiser->last_heartbeat_ms = now_ms;
  }
}

static void DirectoryAdvertiserShutdown(DirectoryAdvertiser* advertiser) {
  if (advertiser->host == NULL) {
    return;
  }
  if (advertiser->peer != NULL) {
    enet_peer_disconnect_now(advertiser->peer, 0u);
  }
  enet_host_destroy(advertiser->host);
  *advertiser = (DirectoryAdvertiser){0};
}

int main(int argc, char** argv) {
  const uint64_t tick_interval_nanos = 1000000000ull / (uint64_t)SHROOM_SERVER_TICK_RATE;
  const uint64_t spore_interval_ticks =
      (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_WORLD_REPLICATION_RATE);
  ENetAddress address = {0};
  ENetHost* host;
  static ShroomLobby lobbies[SHROOM_MAX_LOBBIES] = {0};
  static ServerSession sessions[SHROOM_SERVER_MAX_CLIENTS] = {0};
  static ShroomVoiceRelay voice_relay;
  uint32_t next_player_id = 1;
  uint32_t next_lobby_id = 1;
  uint64_t next_tick_time;
  uint64_t last_health_log_ms = 0;
  sqlite3* db = NULL;
  sqlite3* account_db = NULL;
  ShroomAuthContext auth_ctx = {0};
  ShroomAccountAuth account_auth = {0};
  ShroomRestServer rest_server = {0};
  ServerConfig config;
  DirectoryAdvertiser directory_advertiser = {0};

  ShroomLifecycleInit(&g_lifecycle);
  ShroomVoiceRelayInit(&voice_relay);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_INIT);

  LoggerInit(LOG_LEVEL_INFO, 1);
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);

  if (!LoadServerConfig(&config, argc, argv)) {
    ShroomLifecycleSetError(&g_lifecycle, 1, "Invalid server configuration");
    return 1;
  }

  g_match_duration_seconds = config.match_duration_seconds;
  g_snapshot_rate = config.snapshot_rate;

  if (config.benchmark) {
    return RunServerBenchmark(&config);
  }
  if (config.directory_mode) {
    return RunDirectoryServer(&config);
  }

  if (sqlite3_open(config.database_path, &db) != SQLITE_OK) {
    LOG_ERROR("failed to open database: %s", sqlite3_errmsg(db));
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database initialization failed");
    return 1;
  }
  LOG_INFO("database initialized: path=%s", config.database_path);

  if (!ShroomDatabaseInitializeSchema(db)) {
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database schema creation failed");
    return 1;
  }
  if (!ShroomDatabaseSeedDefaults(db)) {
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database seed failed");
    return 1;
  }

  ShroomAuthInit(&auth_ctx, db);
  if (sqlite3_open(config.database_path, &account_db) != SQLITE_OK) {
    LOG_ERROR("failed to open account database connection: %s", sqlite3_errmsg(account_db));
    sqlite3_close(account_db);
    ShroomAuthShutdown(&auth_ctx);
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "Account database initialization failed");
    return 1;
  }
  sqlite3_busy_timeout(account_db, 5000);
  sqlite3_exec(account_db, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);
  ShroomAccountAuthInit(&account_auth, account_db);

  if (enet_initialize() != 0) {
    LOG_ERROR("failed to initialize ENet");
    ShroomAuthShutdown(&auth_ctx);
    ShroomAccountAuthShutdown(&account_auth);
    sqlite3_close(account_db);
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "ENet initialization failed");
    return 1;
  }

  /* Create fixed lobbies. */
  {
    uint32_t li;

    for (li = 0; li < SHROOM_LOBBY_DEFAULT_COUNT; ++li) {
      const ShroomGameMode mode = li < (SHROOM_LOBBY_DEFAULT_COUNT / 2u)
                                      ? SHROOM_GAME_MODE_FFA
                                      : SHROOM_GAME_MODE_KING_OF_HILL;
      CreateLobby(lobbies, next_lobby_id++, NULL, false, mode, &next_player_id);
    }
  }

  address.host = config.bind_address;
  address.port = config.port;
  host = enet_host_create(&address, SHROOM_SERVER_MAX_CLIENTS, SHROOM_ENET_CHANNEL_COUNT, 0, 0);
  if (host == 0) {
    LOG_ERROR("failed to create ENet host");
    enet_deinitialize();
    ShroomAuthShutdown(&auth_ctx);
    ShroomAccountAuthShutdown(&account_auth);
    sqlite3_close(account_db);
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 2, "ENet host creation failed");
    return 1;
  }

  if (config.rest_enabled &&
      !ShroomRestServerStart(&rest_server,
                             &(ShroomRestConfig){.bind_host = config.rest_bind_host,
                                                 .port = config.rest_port,
                                                 .certificate_path = config.rest_certificate_path,
                                                 .account_auth = &account_auth})) {
    enet_host_destroy(host);
    enet_deinitialize();
    ShroomAuthShutdown(&auth_ctx);
    ShroomAccountAuthShutdown(&account_auth);
    sqlite3_close(account_db);
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 2, "REST HTTPS listener initialization failed");
    return 1;
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_START);
  LOG_INFO("shroomio server listening on %s:%u/udp snapshot_rate=%u", config.bind_host, config.port,
           config.snapshot_rate);
  DirectoryAdvertiserInit(&directory_advertiser, &config, GetTimeMillis());
  if (config.smoke_test) {
    ShroomLifecycleRequestShutdown(&g_lifecycle);
  }
  next_tick_time = GetTimeNanos();

  while (ShroomLifecycleIsRunning(&g_lifecycle) &&
         !ShroomLifecycleIsShutdownRequested(&g_lifecycle)) {
    ENetEvent event;
    const uint64_t now_ms = GetTimeMillis();
    const bool profile_enabled = ShroomProfileEnabled();
    const uint64_t tick_start_nanos = profile_enabled ? GetTimeNanos() : 0ull;
    uint64_t phase_start_nanos = tick_start_nanos;
    uint32_t serviced_event_count = 0u;

    while ((serviced_event_count < SHROOM_SERVER_MAX_ENET_EVENTS_PER_TICK) &&
           (enet_host_service(host, &event, 0) > 0)) {
      serviced_event_count += 1u;
      switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        if (event.peer->incomingPeerID < SHROOM_SERVER_MAX_CLIENTS) {
          event.peer->data = &sessions[event.peer->incomingPeerID];
          ShroomVoiceRelaySetPeer(&voice_relay, event.peer->incomingPeerID, true, 0u, 0u);
          LOG_INFO("peer connected: slot=%u", (unsigned)event.peer->incomingPeerID);
        } else {
          LOG_WARN("rejected connection: no available slots");
          enet_peer_disconnect(event.peer, 0);
        }
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        HandlePacket(host, event.peer, (ServerSession*)event.peer->data, lobbies, &auth_ctx,
                     &voice_relay, event.packet, event.channelID, &next_player_id, &next_lobby_id,
                     now_ms);
        enet_packet_destroy(event.packet);
        break;
      case ENET_EVENT_TYPE_DISCONNECT: {
        ServerSession* disconnected_session = (ServerSession*)event.peer->data;
        const uint32_t disconnected_lobby_id =
            disconnected_session != NULL ? disconnected_session->lobby_id : 0u;
        ShroomLobby* disconnected_lobby = FindLobbyById(lobbies, disconnected_lobby_id);

        LOG_INFO("peer disconnected: slot=%u", (unsigned)event.peer->incomingPeerID);
        if ((disconnected_session != NULL) && (disconnected_lobby != NULL) &&
            ShroomIntermissionRemoveVoter(&disconnected_lobby->intermission,
                                          disconnected_session->player_id)) {
          BroadcastIntermissionStatus(host, disconnected_lobby);
        }
        if ((disconnected_session != NULL) && (disconnected_lobby != NULL) &&
            disconnected_session->entered_match) {
          CapturePersistenceParticipant(disconnected_lobby, disconnected_session->player_id, true);
        }
        DisconnectSession(disconnected_session, lobbies);
        ShroomVoiceRelaySetPeer(&voice_relay, event.peer->incomingPeerID, false, 0u, 0u);
        event.peer->data = 0;
        ShroomNetTelemetrySetPeerTransport(&g_server_net_telemetry, event.peer->incomingPeerID, 0u,
                                           0u, false);
        BroadcastLobbyRoster(host, disconnected_lobby_id);
      } break;
      case ENET_EVENT_TYPE_NONE:
      default:
        break;
      }
    }
    if (serviced_event_count == SHROOM_SERVER_MAX_ENET_EVENTS_PER_TICK) {
      g_server_event_budget_exhaustions += 1u;
    }
    DirectoryAdvertiserUpdate(&directory_advertiser, &config, host, now_ms);
    for (size_t peer_index = 0u; peer_index < host->peerCount; ++peer_index) {
      ENetPeer* peer = &host->peers[peer_index];
      const bool active = peer->state == ENET_PEER_STATE_CONNECTED;
      const size_t queued = active ? enet_list_size(&peer->outgoingCommands) +
                                         enet_list_size(&peer->outgoingSendReliableCommands)
                                   : 0u;
      const uint32_t bounded_queue = queued > UINT32_MAX ? UINT32_MAX : (uint32_t)queued;
      const uint16_t loss_basis_points =
          active ? (uint16_t)(((uint64_t)peer->packetLoss * 10000u) / ENET_PEER_PACKET_LOSS_SCALE)
                 : 0u;
      ShroomNetTelemetrySetPeerTransport(&g_server_net_telemetry, peer_index, bounded_queue,
                                         loss_basis_points, active);
    }
    if (profile_enabled) {
      ShroomProfileRecord(&g_server_profile.enet_events,
                          ShroomProfileNanosToMs(GetTimeNanos() - phase_start_nanos));
    }

    /* Tick and broadcast per-lobby. */
    {
      size_t li;

      for (li = 0; li < SHROOM_MAX_LOBBIES; ++li) {
        ShroomLobby* lobby = &lobbies[li];
        size_t pi;
        uint16_t real_count;
        uint16_t spec_count;
        double simulation_ms = 0.0;
        ShroomMatchPhase previous_phase;

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

        /* Roster players are present in snapshots but have not entered gameplay yet. */
        for (pi = 0; pi < host->peerCount; ++pi) {
          ServerSession* session = (ServerSession*)host->peers[pi].data;

          if ((host->peers[pi].state == ENET_PEER_STATE_CONNECTED) && (session != NULL) &&
              session->active && !session->spectating && !session->entered_match &&
              (session->lobby_id == lobby->lobby_id) && (session->player != NULL) &&
              session->player->alive) {
            session->player->spawn_protection_timer = SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS;
          }
        }

        AdjustLobbyBots(lobby, host, &next_player_id, now_ms);

        phase_start_nanos = profile_enabled ? GetTimeNanos() : 0ull;
        previous_phase = lobby->world.match_phase;
        ShroomWorldStep(&lobby->world, 1.0f / SHROOM_SERVER_TICK_RATE);
        if ((previous_phase == SHROOM_MATCH_PHASE_RUNNING) &&
            (lobby->world.match_phase == SHROOM_MATCH_PHASE_RESULTS)) {
          PersistCompletedLobbyRound(db, lobby);
          BeginIntermission(host, lobby);
        }
        if (lobby->world.match_phase == SHROOM_MATCH_PHASE_RESULTS) {
          uint16_t seconds = (uint16_t)lobby->world.match_results_time_remaining;
          if (seconds != lobby->last_intermission_second) {
            lobby->last_intermission_second = seconds;
            BroadcastIntermissionStatus(host, lobby);
          }
          if (ShroomIntermissionAllEligibleVoted(&lobby->intermission)) {
            ResolveIntermission(host, lobby, db);
          }
        }
        if (lobby->world.match_phase == SHROOM_MATCH_PHASE_RESET) {
          ResolveIntermission(host, lobby, db);
        }
        if (profile_enabled) {
          simulation_ms = ShroomProfileNanosToMs(GetTimeNanos() - phase_start_nanos);
        }

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

        phase_start_nanos = profile_enabled ? GetTimeNanos() : 0ull;
        if (ShroomSnapshotSchedulerStep(&lobby->snapshot_scheduler)) {
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
            ServerSession* s = (ServerSession*)host->peers[pi].data;

            if ((host->peers[pi].state != ENET_PEER_STATE_CONNECTED) || (s == NULL) || !s->active ||
                (s->lobby_id != lobby->lobby_id)) {
              continue;
            }
            SendWorldState(&host->peers[pi], s, &lobby->world);
          }
          enet_host_flush(host);
        }
        if (profile_enabled) {
          const double broadcast_ms = ShroomProfileNanosToMs(GetTimeNanos() - phase_start_nanos);
          ShroomProfileRecord(&g_server_profile.simulation, simulation_ms);
          ShroomProfileRecord(&g_server_profile.broadcast, broadcast_ms);
        }
      }
    }

    /* Session health log every 60 s. */
    if ((now_ms - last_health_log_ms) >= 60000ull) {
      size_t li;
      const uint64_t input_accepted =
          g_server_net_telemetry.by_type[SHROOM_PACKET_INPUT].accepted.packets;

      last_health_log_ms = now_ms;
      LOG_INFO("health input_accepted=%llu input_stale=%llu input_rate_limited=%llu "
               "event_budget_exhaustions=%llu",
               (unsigned long long)input_accepted,
               (unsigned long long)g_server_input_stale_rejections,
               (unsigned long long)g_server_input_rate_rejections,
               (unsigned long long)g_server_event_budget_exhaustions);
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

    if (profile_enabled) {
      ShroomProfileRecord(&g_server_profile.tick,
                          ShroomProfileNanosToMs(GetTimeNanos() - tick_start_nanos));
      ServerProfileMaybeLog(now_ms);
    }

    next_tick_time += tick_interval_nanos;
    SleepUntil(next_tick_time);
  }

  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_STOP);
  ShroomRestServerStop(&rest_server);
  DirectoryAdvertiserShutdown(&directory_advertiser);
  for (size_t peer_index = 0; peer_index < host->peerCount; ++peer_index) {
    DisconnectSession((ServerSession*)host->peers[peer_index].data, lobbies);
    ShroomVoiceRelaySetPeer(&voice_relay, peer_index, false, 0u, 0u);
    host->peers[peer_index].data = NULL;
  }
  enet_host_destroy(host);
  enet_deinitialize();
  ShroomAuthShutdown(&auth_ctx);
  ShroomAccountAuthShutdown(&account_auth);
  sqlite3_close(account_db);
  sqlite3_close(db);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN);
  LOG_INFO("shroomio server shutting down");
  return 0;
}
