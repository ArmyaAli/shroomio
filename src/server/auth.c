#define _XOPEN_SOURCE 700
#include "auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sqlite3.h>

#include "logger.h"

#define AUTH_SALT_LENGTH 16
#define AUTH_HASH_LENGTH 64
#define AUTH_TOKEN_EXPIRY_HOURS 24

static void GenerateRandomBytes(uint8_t* buffer, size_t length) {
  FILE* fp = fopen("/dev/urandom", "rb");
  if (fp != NULL) {
    size_t read = fread(buffer, 1, length, fp);
    fclose(fp);
    if (read == length) {
      return;
    }
  }

  srand((unsigned int)time(NULL) ^ (unsigned int)((uintptr_t)buffer));
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = (uint8_t)(rand() & 0xFF);
  }
}

static void BytesToHex(const uint8_t* bytes, size_t length, char* hex) {
  for (size_t i = 0; i < length; ++i) {
    sprintf(hex + (i * 2), "%02x", bytes[i]);
  }
  hex[length * 2] = '\0';
}

static void HashPassword(const char* password, const char* salt, char* hash_out) {
  uint8_t hash[AUTH_HASH_LENGTH / 2];
  size_t pass_len = strlen(password);
  size_t salt_len = strlen(salt);

  for (size_t i = 0; i < sizeof(hash); ++i) {
    hash[i] = (uint8_t)(i * 31 + 17);
  }

  for (size_t round = 0; round < 1000; ++round) {
    for (size_t i = 0; i < pass_len; ++i) {
      hash[i % sizeof(hash)] ^= (uint8_t)password[i];
      hash[i % sizeof(hash)] = (uint8_t)((hash[i % sizeof(hash)] * 31 + 17) & 0xFF);
    }
    for (size_t i = 0; i < salt_len; ++i) {
      hash[(i + 7) % sizeof(hash)] ^= (uint8_t)salt[i];
      hash[(i + 7) % sizeof(hash)] = (uint8_t)((hash[(i + 7) % sizeof(hash)] * 37 + 13) & 0xFF);
    }
    for (size_t i = 0; i < sizeof(hash); ++i) {
      hash[i] = (uint8_t)((hash[i] ^ hash[(i + 1) % sizeof(hash)]) + (uint8_t)round) & 0xFF;
    }
  }

  BytesToHex(hash, sizeof(hash), hash_out);
}

static void GenerateSalt(char* salt_out) {
  uint8_t bytes[AUTH_SALT_LENGTH];
  GenerateRandomBytes(bytes, sizeof(bytes));
  BytesToHex(bytes, sizeof(bytes), salt_out);
}

static void GenerateToken(char* token_out) {
  uint8_t bytes[SHROOM_AUTH_TOKEN_LENGTH / 2];
  GenerateRandomBytes(bytes, sizeof(bytes));
  BytesToHex(bytes, sizeof(bytes), token_out);
}

static bool ValidateUsername(const char* username) {
  if (username == NULL) {
    return false;
  }
  size_t len = strlen(username);
  if (len < SHROOM_AUTH_USERNAME_MIN_LENGTH || len > SHROOM_AUTH_USERNAME_MAX_LENGTH) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    char c = username[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-')) {
      return false;
    }
  }
  return true;
}

static bool ValidatePassword(const char* password) {
  if (password == NULL) {
    return false;
  }
  return strlen(password) >= SHROOM_AUTH_PASSWORD_MIN_LENGTH;
}

void ShroomAuthInit(ShroomAuthContext* ctx, sqlite3* db) {
  if (ctx == NULL) {
    return;
  }
  ctx->db = db;
}

void ShroomAuthShutdown(ShroomAuthContext* ctx) {
  if (ctx == NULL) {
    return;
  }
  ctx->db = NULL;
}

ShroomAuthResult ShroomAuthRegister(ShroomAuthContext* ctx, const char* username,
                                    const char* password, ShroomAuthUser* out_user) {
  if (ctx == NULL || ctx->db == NULL) {
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  if (!ValidateUsername(username) || !ValidatePassword(password)) {
    return SHROOM_AUTH_INVALID_INPUT;
  }

  sqlite3_stmt* stmt = NULL;
  const char* check_sql = "SELECT id FROM users WHERE username = ?";
  if (sqlite3_prepare_v2(ctx->db, check_sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("auth: failed to prepare check query: %s", sqlite3_errmsg(ctx->db));
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  ShroomAuthResult result = SHROOM_AUTH_SUCCESS;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return SHROOM_AUTH_USERNAME_TAKEN;
  }
  sqlite3_finalize(stmt);

  char salt[AUTH_SALT_LENGTH * 2 + 1];
  char hash[AUTH_HASH_LENGTH + 1];
  GenerateSalt(salt);
  HashPassword(password, salt, hash);

  char combined_hash[AUTH_HASH_LENGTH + AUTH_SALT_LENGTH * 2 + 2];
  snprintf(combined_hash, sizeof(combined_hash), "%s:%s", salt, hash);

  const char* insert_user_sql =
      "INSERT INTO players (player_uuid, display_name) VALUES (lower(hex(randomblob(16))), ?)";
  if (sqlite3_prepare_v2(ctx->db, insert_user_sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("auth: failed to prepare player insert: %s", sqlite3_errmsg(ctx->db));
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOG_ERROR("auth: failed to insert player: %s", sqlite3_errmsg(ctx->db));
    sqlite3_finalize(stmt);
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_finalize(stmt);

  int64_t player_id = sqlite3_last_insert_rowid(ctx->db);

  const char* insert_stats_sql = "INSERT INTO player_stats (player_id) VALUES (?)";
  if (sqlite3_prepare_v2(ctx->db, insert_stats_sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("auth: failed to prepare stats insert: %s", sqlite3_errmsg(ctx->db));
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_int64(stmt, 1, player_id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  const char* insert_auth_sql =
      "INSERT INTO users (player_id, username, password_hash, auth_method) VALUES (?, ?, ?, ?)";
  if (sqlite3_prepare_v2(ctx->db, insert_auth_sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("auth: failed to prepare user insert: %s", sqlite3_errmsg(ctx->db));
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_int64(stmt, 1, player_id);
  sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, combined_hash, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, "password", -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    LOG_ERROR("auth: failed to insert user: %s", sqlite3_errmsg(ctx->db));
    sqlite3_finalize(stmt);
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_finalize(stmt);

  if (out_user != NULL) {
    out_user->user_id = (uint32_t)sqlite3_last_insert_rowid(ctx->db);
    out_user->player_id = (uint32_t)player_id;
    snprintf(out_user->username, sizeof(out_user->username), "%s", username);
    out_user->auth_method = SHROOM_AUTH_PASSWORD;
  }

  LOG_INFO("auth: registered user '%s' (player_id=%d)", username, (int)player_id);
  return result;
}

ShroomAuthResult ShroomAuthLogin(ShroomAuthContext* ctx, const char* username, const char* password,
                                 ShroomAuthToken* out_token) {
  if (ctx == NULL || ctx->db == NULL) {
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  if (username == NULL || password == NULL) {
    return SHROOM_AUTH_INVALID_INPUT;
  }

  sqlite3_stmt* stmt = NULL;
  const char* sql =
      "SELECT u.id, u.player_id, u.password_hash FROM users u WHERE u.username = ? AND "
      "u.auth_method = 'password'";
  if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("auth: failed to prepare login query: %s", sqlite3_errmsg(ctx->db));
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  ShroomAuthResult result = SHROOM_AUTH_INVALID_CREDENTIALS;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    uint32_t user_id = (uint32_t)sqlite3_column_int64(stmt, 0);
    const char* stored_hash = (const char*)sqlite3_column_text(stmt, 2);

    if (stored_hash != NULL) {
      const char* colon = strchr(stored_hash, ':');
      if (colon != NULL) {
        size_t salt_len = (size_t)(colon - stored_hash);
        char salt[AUTH_SALT_LENGTH * 2 + 1];
        if (salt_len < sizeof(salt)) {
          char computed_hash[AUTH_HASH_LENGTH + 1];
          strncpy(salt, stored_hash, salt_len);
          salt[salt_len] = '\0';

          HashPassword(password, salt, computed_hash);

          if (strcmp(colon + 1, computed_hash) == 0) {
            char token[SHROOM_AUTH_TOKEN_LENGTH + 1];
            GenerateToken(token);

            time_t now = time(NULL);
            time_t expires = now + (AUTH_TOKEN_EXPIRY_HOURS * 3600);
            char expires_str[32];
            strftime(expires_str, sizeof(expires_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&expires));

            sqlite3_finalize(stmt);

            const char* insert_token_sql =
                "INSERT INTO auth_tokens (user_id, token, expires_at) VALUES (?, ?, ?)";
            if (sqlite3_prepare_v2(ctx->db, insert_token_sql, -1, &stmt, NULL) != SQLITE_OK) {
              LOG_ERROR("auth: failed to prepare token insert: %s", sqlite3_errmsg(ctx->db));
              return SHROOM_AUTH_DATABASE_ERROR;
            }
            sqlite3_bind_int64(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, token, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, expires_str, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
              LOG_ERROR("auth: failed to insert token: %s", sqlite3_errmsg(ctx->db));
              sqlite3_finalize(stmt);
              return SHROOM_AUTH_DATABASE_ERROR;
            }
            sqlite3_finalize(stmt);

            if (out_token != NULL) {
              snprintf(out_token->token, sizeof(out_token->token), "%s", token);
              out_token->user_id = user_id;
              out_token->expires_at = (uint64_t)expires;
            }

            const char* update_login_sql =
                "UPDATE users SET last_login_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = "
                "?";
            sqlite3_prepare_v2(ctx->db, update_login_sql, -1, &stmt, NULL);
            sqlite3_bind_int64(stmt, 1, user_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            LOG_INFO("auth: user '%s' logged in", username);
            return SHROOM_AUTH_SUCCESS;
          }
        }
      }
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

ShroomAuthResult ShroomAuthLoginAnonymous(ShroomAuthContext* ctx, const char* username,
                                          ShroomAuthToken* out_token) {
  if (ctx == NULL || ctx->db == NULL) {
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  if (!ValidateUsername(username)) {
    return SHROOM_AUTH_INVALID_INPUT;
  }

  sqlite3_stmt* stmt = NULL;
  const char* check_sql = "SELECT id FROM users WHERE username = ?";
  if (sqlite3_prepare_v2(ctx->db, check_sql, -1, &stmt, NULL) != SQLITE_OK) {
    LOG_ERROR("auth: failed to prepare check query: %s", sqlite3_errmsg(ctx->db));
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

  uint32_t user_id = 0;
  uint32_t player_id = 0;

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    user_id = (uint32_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);

    const char* get_player_sql = "SELECT player_id FROM users WHERE id = ?";
    if (sqlite3_prepare_v2(ctx->db, get_player_sql, -1, &stmt, NULL) != SQLITE_OK) {
      return SHROOM_AUTH_DATABASE_ERROR;
    }
    sqlite3_bind_int64(stmt, 1, user_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      // Player exists, player_id retrieved but not needed for anonymous login flow
    }
    sqlite3_finalize(stmt);
  } else {
    sqlite3_finalize(stmt);

    const char* insert_player_sql =
        "INSERT INTO players (player_uuid, display_name) VALUES (lower(hex(randomblob(16))), ?)";
    if (sqlite3_prepare_v2(ctx->db, insert_player_sql, -1, &stmt, NULL) != SQLITE_OK) {
      return SHROOM_AUTH_DATABASE_ERROR;
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      return SHROOM_AUTH_DATABASE_ERROR;
    }
    sqlite3_finalize(stmt);
    player_id = (uint32_t)sqlite3_last_insert_rowid(ctx->db);

    const char* insert_stats_sql = "INSERT INTO player_stats (player_id) VALUES (?)";
    sqlite3_prepare_v2(ctx->db, insert_stats_sql, -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, player_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    const char* insert_user_sql =
        "INSERT INTO users (player_id, username, auth_method) VALUES (?, ?, 'anonymous')";
    if (sqlite3_prepare_v2(ctx->db, insert_user_sql, -1, &stmt, NULL) != SQLITE_OK) {
      return SHROOM_AUTH_DATABASE_ERROR;
    }
    sqlite3_bind_int64(stmt, 1, player_id);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      return SHROOM_AUTH_DATABASE_ERROR;
    }
    sqlite3_finalize(stmt);
    user_id = (uint32_t)sqlite3_last_insert_rowid(ctx->db);
  }

  char token[SHROOM_AUTH_TOKEN_LENGTH + 1];
  GenerateToken(token);

  time_t now = time(NULL);
  time_t expires = now + (AUTH_TOKEN_EXPIRY_HOURS * 3600);
  char expires_str[32];
  strftime(expires_str, sizeof(expires_str), "%Y-%m-%dT%H:%M:%SZ", gmtime(&expires));

  const char* insert_token_sql =
      "INSERT INTO auth_tokens (user_id, token, expires_at) VALUES (?, ?, ?)";
  if (sqlite3_prepare_v2(ctx->db, insert_token_sql, -1, &stmt, NULL) != SQLITE_OK) {
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, token, -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, expires_str, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_finalize(stmt);

  if (out_token != NULL) {
    snprintf(out_token->token, sizeof(out_token->token), "%s", token);
    out_token->user_id = user_id;
    out_token->expires_at = (uint64_t)expires;
  }

  LOG_INFO("auth: anonymous login '%s' (user_id=%d)", username, (int)user_id);
  return SHROOM_AUTH_SUCCESS;
}

ShroomAuthResult ShroomAuthValidateToken(ShroomAuthContext* ctx, const char* token,
                                         ShroomAuthUser* out_user) {
  if (ctx == NULL || ctx->db == NULL || token == NULL) {
    return SHROOM_AUTH_INVALID_TOKEN;
  }

  sqlite3_stmt* stmt = NULL;
  const char* sql = "SELECT t.user_id, u.player_id, u.username, u.auth_method, t.expires_at "
                    "FROM auth_tokens t "
                    "JOIN users u ON t.user_id = u.id "
                    "WHERE t.token = ? AND t.revoked_at IS NULL";

  if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

  ShroomAuthResult result = SHROOM_AUTH_INVALID_TOKEN;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* expires_str = (const char*)sqlite3_column_text(stmt, 4);
    if (expires_str != NULL) {
      struct tm tm = {0};
      strptime(expires_str, "%Y-%m-%dT%H:%M:%SZ", &tm);
      time_t expires = timegm(&tm);
      if (time(NULL) < expires) {
        if (out_user != NULL) {
          out_user->user_id = (uint32_t)sqlite3_column_int64(stmt, 0);
          out_user->player_id = (uint32_t)sqlite3_column_int64(stmt, 1);
          const char* username = (const char*)sqlite3_column_text(stmt, 2);
          if (username != NULL) {
            snprintf(out_user->username, sizeof(out_user->username), "%s", username);
          }
          const char* method = (const char*)sqlite3_column_text(stmt, 3);
          if (method != NULL && strcmp(method, "password") == 0) {
            out_user->auth_method = SHROOM_AUTH_PASSWORD;
          } else if (method != NULL && strcmp(method, "discord") == 0) {
            out_user->auth_method = SHROOM_AUTH_DISCORD;
          } else {
            out_user->auth_method = SHROOM_AUTH_ANONYMOUS;
          }
        }
        result = SHROOM_AUTH_SUCCESS;
      } else {
        result = SHROOM_AUTH_TOKEN_EXPIRED;
      }
    }
  }

  sqlite3_finalize(stmt);
  return result;
}

ShroomAuthResult ShroomAuthRevokeToken(ShroomAuthContext* ctx, const char* token) {
  if (ctx == NULL || ctx->db == NULL || token == NULL) {
    return SHROOM_AUTH_INVALID_TOKEN;
  }

  sqlite3_stmt* stmt = NULL;
  const char* sql =
      "UPDATE auth_tokens SET revoked_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE token = ?";

  if (sqlite3_prepare_v2(ctx->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return SHROOM_AUTH_DATABASE_ERROR;
  }
  sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return SHROOM_AUTH_DATABASE_ERROR;
  }

  int changes = sqlite3_changes(ctx->db);
  sqlite3_finalize(stmt);

  return changes > 0 ? SHROOM_AUTH_SUCCESS : SHROOM_AUTH_INVALID_TOKEN;
}
