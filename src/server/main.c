#include <signal.h>
#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <enet/enet.h>

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

typedef struct ServerSession {
  bool active;
  bool authenticated;
  uint32_t player_id;
  uint32_t user_id;
  uint32_t last_processed_input_sequence;
  uint32_t last_logged_rtt_ms;
  uint32_t chat_message_count;
  uint64_t last_latency_log_time_ms;
  uint64_t chat_window_start_ms;
  ShroomPlayerState* player;
  char auth_token[SHROOM_AUTH_TOKEN_LENGTH + 1];
} ServerSession;

static ShroomLifecycle g_lifecycle;

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
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return ((uint64_t)now.tv_sec * 1000000000ull) + (uint64_t)now.tv_nsec;
}

static uint64_t GetTimeMillis(void) { return GetTimeNanos() / 1000000ull; }

static void SleepUntil(uint64_t target_time_nanos) {
  struct timespec sleep_time;
  uint64_t now = GetTimeNanos();
  uint64_t delta;

  if (now >= target_time_nanos) {
    return;
  }

  delta = target_time_nanos - now;
  sleep_time.tv_sec = (time_t)(delta / 1000000000ull);
  sleep_time.tv_nsec = (long)(delta % 1000000000ull);
  nanosleep(&sleep_time, 0);
}

static ENetPacket* CreatePacket(const void* data, size_t size, enet_uint32 flags) {
  return enet_packet_create(data, size, flags);
}

static ENetPacket* CreateProtocolPacket(const void* data, size_t size, ShroomPacketType type) {
  return CreatePacket(data, size,
                      ShroomPacketTypeUsesReliableDelivery(type) ? ENET_PACKET_FLAG_RELIABLE : 0);
}

static void SetDefaultPlayerName(ShroomPlayerState* player, const char* prefix) {
  if (player == NULL) {
    return;
  }

  snprintf(player->name, sizeof(player->name), "%s %u", prefix, player->player_id);
}

static void SendWelcome(ENetPeer* peer, const ServerSession* session,
                        const ShroomWorldState* world) {
  const ShroomWelcomePacket packet = {
      .header = {SHROOM_PACKET_WELCOME, SHROOM_ENET_CHANNEL_CONTROL, sizeof(ShroomWelcomePacket)},
      .protocol_version = SHROOM_PROTOCOL_VERSION,
      .player_id = session->player_id,
      .entity_id = session->player != 0 ? session->player->entity_id : 0,
      .server_tick_rate = (uint16_t)SHROOM_SERVER_TICK_RATE,
      .snapshot_rate = SHROOM_SNAPSHOT_RATE,
      .world_width = world->width,
      .world_height = world->height,
  };

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_WELCOME));
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

  if ((session == 0) || !session->active || (session->player == 0)) {
    return;
  }

  packet = (ShroomSnapshotPacket){
      .header = {SHROOM_PACKET_SNAPSHOT, SHROOM_ENET_CHANNEL_SNAPSHOT,
                 sizeof(ShroomSnapshotPacket)},
      .tick = world->tick,
      .last_processed_input_sequence = session->last_processed_input_sequence,
      .player_id = session->player_id,
      .entity_id = session->player->entity_id,
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
    };
    snprintf(packet.players[player_count - 1].name, sizeof(packet.players[player_count - 1].name),
             "%s", player->name);
  }

  packet.player_count = player_count;

  enet_peer_send(peer, SHROOM_ENET_CHANNEL_SNAPSHOT,
                 CreateProtocolPacket(&packet, sizeof(packet), SHROOM_PACKET_SNAPSHOT));
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

static void DisconnectSession(ServerSession* session) {
  if ((session == 0) || !session->active) {
    return;
  }

  if (session->player != 0) {
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

static void HandleHelloPacket(ENetPeer* peer, ServerSession* session, ShroomWorldState* world,
                              const ENetPacket* enet_packet, uint32_t* next_player_id) {
  const ShroomHelloPacket* packet = (const ShroomHelloPacket*)enet_packet->data;

  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }
  if (packet->protocol_version != SHROOM_PROTOCOL_VERSION) {
    enet_peer_disconnect(peer, 0);
    return;
  }
  if (session->active) {
    return;
  }

  session->player = ShroomWorldSpawnPlayer(world, (*next_player_id)++, false);
  if (session->player == 0) {
    enet_peer_disconnect(peer, 0);
    return;
  }

  session->active = true;
  session->player_id = session->player->player_id;
  snprintf(session->player->name, sizeof(session->player->name), "%s", packet->name);
  if (session->player->name[0] == '\0') {
    SetDefaultPlayerName(session->player, "Player");
  }
  SendWelcome(peer, session, world);
}

static void HandleInputPacket(ServerSession* session, const ENetPacket* enet_packet) {
  const ShroomInputPacket* packet = (const ShroomInputPacket*)enet_packet->data;

  if ((session == 0) || !session->active || (session->player == 0)) {
    return;
  }
  if (enet_packet->dataLength < sizeof(*packet)) {
    return;
  }

  session->last_processed_input_sequence = packet->sequence;
  ShroomPlayerSetInput(session->player,
                       NormalizeInput((ShroomVec2){packet->direction_x, packet->direction_y}));
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

  /* Broadcast to every connected peer. */
  for (index = 0; index < host->peerCount; ++index) {
    ENetPeer* peer = &host->peers[index];
    ENetPacket* out;

    if (peer->state != ENET_PEER_STATE_CONNECTED) {
      continue;
    }
    out = CreateProtocolPacket(&broadcast, sizeof(broadcast), SHROOM_PACKET_CHAT);
    if (out != NULL) {
      enet_peer_send(peer, SHROOM_ENET_CHANNEL_CHAT, out);
    }
  }
  enet_host_flush(host);
}

static void HandlePacket(ENetHost* host, ENetPeer* peer, ServerSession* session,
                         ShroomWorldState* world, ShroomAuthContext* auth_ctx,
                         const ENetPacket* enet_packet, uint8_t channel_id,
                         uint32_t* next_player_id, uint64_t now_ms) {
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
    HandleHelloPacket(peer, session, world, enet_packet, next_player_id);
    break;
  case SHROOM_PACKET_INPUT:
    HandleInputPacket(session, enet_packet);
    break;
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
  default:
    break;
  }
}

int main(void) {
  const uint64_t tick_interval_nanos = 1000000000ull / (uint64_t)SHROOM_SERVER_TICK_RATE;
  const uint64_t snapshot_interval_ticks =
      (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_SNAPSHOT_RATE);
  const uint64_t spore_interval_ticks =
      (uint64_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_SPORE_STATE_RATE);
  ENetAddress address = {0};
  ENetHost* host;
  ShroomWorldState world;
  static ServerSession sessions[SHROOM_SERVER_MAX_CLIENTS] = {0};
  uint32_t next_player_id = 1;
  uint64_t next_tick_time;
  sqlite3* db = NULL;
  ShroomAuthContext auth_ctx = {0};

  ShroomLifecycleInit(&g_lifecycle);
  ShroomLifecycleTransition(&g_lifecycle, SHROOM_LIFECYCLE_EVENT_INIT);

  LoggerInit(LOG_LEVEL_INFO, 1);
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);

  if (sqlite3_open("shroomio.db", &db) != SQLITE_OK) {
    LOG_ERROR("failed to open database: %s", sqlite3_errmsg(db));
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database initialization failed");
    return 1;
  }
  LOG_INFO("database initialized");

  if (!InitializeDatabaseSchema(db)) {
    sqlite3_close(db);
    ShroomLifecycleSetError(&g_lifecycle, 1, "Database schema creation failed");
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

  ShroomWorldInit(&world);
  while (world.player_count < SHROOM_BOT_COUNT) {
    ShroomPlayerState* bot = ShroomWorldSpawnPlayer(&world, next_player_id++, true);
    SetDefaultPlayerName(bot, "Bot");
  }

  address.host = ENET_HOST_ANY;
  address.port = SHROOM_SERVER_PORT;
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
  LOG_INFO("shroomio server listening on UDP %u", SHROOM_SERVER_PORT);
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
        HandlePacket(host, event.peer, (ServerSession*)event.peer->data, &world, &auth_ctx,
                     event.packet, event.channelID, &next_player_id, now_ms);
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

    ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);
    if ((snapshot_interval_ticks > 0) && ((world.tick % snapshot_interval_ticks) == 0)) {
      size_t index;

      for (index = 0; index < host->peerCount; ++index) {
        if (host->peers[index].state == ENET_PEER_STATE_CONNECTED) {
          LogPeerLatency(&host->peers[index], (ServerSession*)host->peers[index].data, now_ms);
          SendSnapshot(&host->peers[index], (ServerSession*)host->peers[index].data, &world);
        }
      }
      enet_host_flush(host);
    }
    if ((spore_interval_ticks > 0) && ((world.tick % spore_interval_ticks) == 0)) {
      size_t index;

      for (index = 0; index < host->peerCount; ++index) {
        if (host->peers[index].state == ENET_PEER_STATE_CONNECTED) {
          SendSporeState(&host->peers[index], &world);
        }
      }
      enet_host_flush(host);
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
