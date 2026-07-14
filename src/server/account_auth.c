#include "account_auth.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <argon2.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "logger.h"

#define ACCOUNT_ARGON_TIME_COST 3u
#define ACCOUNT_ARGON_MEMORY_KIB 65536u
#define ACCOUNT_ARGON_PARALLELISM 1u
#define ACCOUNT_ARGON_SALT_LENGTH 16u
#define ACCOUNT_ARGON_HASH_LENGTH 32u
#define ACCOUNT_TOKEN_BYTES 32u
#define ACCOUNT_TOKEN_HASH_LENGTH 64u
#define ACCOUNT_FAMILY_BYTES 16u

static const char BASE64_URL_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static uint32_t ArgonTimeCost(void) {
#ifdef TEST_MODE
  if (getenv("SHROOM_VALGRIND") != NULL) {
    return 1u;
  }
#endif
  return ACCOUNT_ARGON_TIME_COST;
}

static uint32_t ArgonMemoryCost(void) {
#ifdef TEST_MODE
  if (getenv("SHROOM_VALGRIND") != NULL) {
    return 1024u;
  }
#endif
  return ACCOUNT_ARGON_MEMORY_KIB;
}

static void LockDatabase(ShroomAccountAuth* auth) {
  while (atomic_flag_test_and_set(&auth->database_lock)) {
  }
}

static void UnlockDatabase(ShroomAccountAuth* auth) { atomic_flag_clear(&auth->database_lock); }

static bool ExecuteSql(sqlite3* db, const char* sql) {
  char* error = NULL;
  const int result = sqlite3_exec(db, sql, NULL, NULL, &error);

  if (result != SQLITE_OK) {
    LOG_ERROR("account auth database operation failed: %s", error != NULL ? error : "unknown");
    sqlite3_free(error);
    return false;
  }
  return true;
}

static void Rollback(sqlite3* db) { sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); }

static void BytesToHex(const unsigned char* bytes, size_t length, char* output) {
  static const char HEX[] = "0123456789abcdef";

  for (size_t index = 0u; index < length; ++index) {
    output[index * 2u] = HEX[bytes[index] >> 4u];
    output[index * 2u + 1u] = HEX[bytes[index] & 0x0fu];
  }
  output[length * 2u] = '\0';
}

static bool RandomHex(size_t byte_count, char* output) {
  unsigned char bytes[ACCOUNT_TOKEN_BYTES];

  if ((byte_count > sizeof(bytes)) || (RAND_bytes(bytes, (int)byte_count) != 1)) {
    return false;
  }
  BytesToHex(bytes, byte_count, output);
  OPENSSL_cleanse(bytes, sizeof(bytes));
  return true;
}

static bool GenerateToken(char* output) {
  unsigned char bytes[ACCOUNT_TOKEN_BYTES];
  size_t input = 0u;
  size_t written = 0u;

  if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
    return false;
  }
  while ((input + 3u) <= sizeof(bytes)) {
    const uint32_t value =
        ((uint32_t)bytes[input] << 16u) | ((uint32_t)bytes[input + 1u] << 8u) | bytes[input + 2u];
    output[written++] = BASE64_URL_ALPHABET[(value >> 18u) & 63u];
    output[written++] = BASE64_URL_ALPHABET[(value >> 12u) & 63u];
    output[written++] = BASE64_URL_ALPHABET[(value >> 6u) & 63u];
    output[written++] = BASE64_URL_ALPHABET[value & 63u];
    input += 3u;
  }
  if (input < sizeof(bytes)) {
    uint32_t value = (uint32_t)bytes[input] << 16u;
    output[written++] = BASE64_URL_ALPHABET[(value >> 18u) & 63u];
    if ((input + 1u) < sizeof(bytes)) {
      value |= (uint32_t)bytes[input + 1u] << 8u;
      output[written++] = BASE64_URL_ALPHABET[(value >> 12u) & 63u];
      output[written++] = BASE64_URL_ALPHABET[(value >> 6u) & 63u];
    } else {
      output[written++] = BASE64_URL_ALPHABET[(value >> 12u) & 63u];
    }
  }
  output[written] = '\0';
  OPENSSL_cleanse(bytes, sizeof(bytes));
  return written == SHROOM_ACCOUNT_TOKEN_LENGTH;
}

static bool HashToken(const char* token, char* output) {
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_length = 0u;

  if ((token == NULL) || (strlen(token) != SHROOM_ACCOUNT_TOKEN_LENGTH) ||
      (EVP_Digest(token, strlen(token), digest, &digest_length, EVP_sha256(), NULL) != 1) ||
      (digest_length != 32u)) {
    return false;
  }
  BytesToHex(digest, digest_length, output);
  OPENSSL_cleanse(digest, sizeof(digest));
  return true;
}

static void FormatUtcTime(time_t timestamp, char* output, size_t output_size) {
  struct tm value;

#ifdef _WIN32
  gmtime_s(&value, &timestamp);
#else
  gmtime_r(&timestamp, &value);
#endif
  strftime(output, output_size, "%Y-%m-%dT%H:%M:%SZ", &value);
}

static void NormalizeEmail(const char* email, char* output) {
  size_t index = 0u;

  while ((index < SHROOM_ACCOUNT_EMAIL_MAX_LENGTH) && (email[index] != '\0')) {
    output[index] = (char)tolower((unsigned char)email[index]);
    ++index;
  }
  output[index] = '\0';
}

bool ShroomAccountValidateUsername(const char* username) {
  size_t length;

  if (username == NULL) {
    return false;
  }
  length = strlen(username);
  if ((length < 3u) || (length > 32u)) {
    return false;
  }
  for (size_t index = 0u; index < length; ++index) {
    const unsigned char character = (unsigned char)username[index];
    if (!(isalnum(character) || (character == '_') || (character == '-'))) {
      return false;
    }
  }
  return true;
}

bool ShroomAccountValidateEmail(const char* email) {
  const char* separator;
  const char* domain_dot;
  size_t length;

  if (email == NULL) {
    return false;
  }
  length = strlen(email);
  if ((length < 3u) || (length > SHROOM_ACCOUNT_EMAIL_MAX_LENGTH)) {
    return false;
  }
  separator = strchr(email, '@');
  if ((separator == NULL) || (separator == email) || (separator[1] == '\0') ||
      (strchr(separator + 1, '@') != NULL)) {
    return false;
  }
  domain_dot = strchr(separator + 1, '.');
  if ((domain_dot == NULL) || (domain_dot == separator + 1) || (domain_dot[1] == '\0')) {
    return false;
  }
  for (size_t index = 0u; index < length; ++index) {
    const unsigned char character = (unsigned char)email[index];
    if ((character <= 32u) || (character >= 127u) || (character == '"') || (character == '\\')) {
      return false;
    }
  }
  return true;
}

bool ShroomAccountValidatePassword(const char* password) {
  size_t length;

  if (password == NULL) {
    return false;
  }
  length = strlen(password);
  return (length >= SHROOM_ACCOUNT_PASSWORD_MIN_LENGTH) &&
         (length <= SHROOM_ACCOUNT_PASSWORD_MAX_LENGTH);
}

static bool HashPasswordWithSalt(const char* password, const unsigned char* salt, char* encoded,
                                 size_t encoded_size) {
  const uint32_t time_cost = ArgonTimeCost();
  const uint32_t memory_cost = ArgonMemoryCost();
  const size_t required =
      argon2_encodedlen(time_cost, memory_cost, ACCOUNT_ARGON_PARALLELISM,
                        ACCOUNT_ARGON_SALT_LENGTH, ACCOUNT_ARGON_HASH_LENGTH, Argon2_id);

  if ((password == NULL) || (encoded == NULL) || (encoded_size < required)) {
    return false;
  }
  return argon2id_hash_encoded(time_cost, memory_cost, ACCOUNT_ARGON_PARALLELISM, password,
                               strlen(password), salt, ACCOUNT_ARGON_SALT_LENGTH,
                               ACCOUNT_ARGON_HASH_LENGTH, encoded, encoded_size) == ARGON2_OK;
}

bool ShroomAccountHashPassword(const char* password, char* encoded, size_t encoded_size) {
  unsigned char salt[ACCOUNT_ARGON_SALT_LENGTH];
  bool success;

  if (!ShroomAccountValidatePassword(password) || (RAND_bytes(salt, sizeof(salt)) != 1)) {
    return false;
  }
  success = HashPasswordWithSalt(password, salt, encoded, encoded_size);
  OPENSSL_cleanse(salt, sizeof(salt));
  return success;
}

bool ShroomAccountVerifyPassword(const char* encoded, const char* password) {
  if ((encoded == NULL) || !ShroomAccountValidatePassword(password)) {
    return false;
  }
  return argon2id_verify(encoded, password, strlen(password)) == ARGON2_OK;
}

void ShroomAccountAuthInit(ShroomAccountAuth* auth, sqlite3* db) {
  if (auth == NULL) {
    return;
  }
  auth->db = db;
  atomic_flag_clear(&auth->database_lock);
}

void ShroomAccountAuthShutdown(ShroomAccountAuth* auth) {
  if (auth != NULL) {
    auth->db = NULL;
  }
}

static bool InsertToken(sqlite3* db, uint32_t user_id, const char* raw_token, const char* kind,
                        const char* family, uint32_t lifetime_seconds, char* digest_output) {
  sqlite3_stmt* statement = NULL;
  char expires_at[32];

  if (!HashToken(raw_token, digest_output)) {
    return false;
  }
  FormatUtcTime(time(NULL) + (time_t)lifetime_seconds, expires_at, sizeof(expires_at));
  if (sqlite3_prepare_v2(
          db,
          "INSERT INTO auth_tokens "
          "(user_id, token_hash, token_type, token_kind, refresh_family_id, expires_at) "
          "VALUES (?, ?, 'opaque', ?, ?, ?)",
          -1, &statement, NULL) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int64(statement, 1, user_id);
  sqlite3_bind_text(statement, 2, digest_output, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 3, kind, -1, SQLITE_STATIC);
  sqlite3_bind_text(statement, 4, family, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 5, expires_at, -1, SQLITE_TRANSIENT);
  const bool success = sqlite3_step(statement) == SQLITE_DONE;
  sqlite3_finalize(statement);
  return success;
}

static bool IssueTokenPair(sqlite3* db, uint32_t user_id, const char* family,
                           ShroomAccountTokenPair* tokens, char* refresh_digest) {
  char access_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];

  if (!GenerateToken(tokens->access_token) || !GenerateToken(tokens->refresh_token) ||
      !InsertToken(db, user_id, tokens->access_token, "access", family,
                   SHROOM_ACCOUNT_ACCESS_EXPIRES_SECONDS, access_digest) ||
      !InsertToken(db, user_id, tokens->refresh_token, "refresh", family,
                   SHROOM_ACCOUNT_REFRESH_EXPIRES_SECONDS, refresh_digest)) {
    OPENSSL_cleanse(tokens, sizeof(*tokens));
    return false;
  }
  tokens->access_expires_in = SHROOM_ACCOUNT_ACCESS_EXPIRES_SECONDS;
  tokens->refresh_expires_in = SHROOM_ACCOUNT_REFRESH_EXPIRES_SECONDS;
  return true;
}

static ShroomAccountResult FindDuplicate(sqlite3* db, const char* username, const char* email) {
  sqlite3_stmt* statement = NULL;
  ShroomAccountResult result = SHROOM_ACCOUNT_SUCCESS;

  if (sqlite3_prepare_v2(db, "SELECT username, email FROM users WHERE username = ? OR email = ?",
                         -1, &statement, NULL) != SQLITE_OK) {
    return SHROOM_ACCOUNT_DATABASE_ERROR;
  }
  sqlite3_bind_text(statement, 1, username, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, email, -1, SQLITE_TRANSIENT);
  while (sqlite3_step(statement) == SQLITE_ROW) {
    const char* existing_username = (const char*)sqlite3_column_text(statement, 0);
    const char* existing_email = (const char*)sqlite3_column_text(statement, 1);
    if ((existing_username != NULL) && (strcmp(existing_username, username) == 0)) {
      result = SHROOM_ACCOUNT_USERNAME_TAKEN;
      break;
    }
    if ((existing_email != NULL) && (strcmp(existing_email, email) == 0)) {
      result = SHROOM_ACCOUNT_EMAIL_TAKEN;
      break;
    }
  }
  sqlite3_finalize(statement);
  return result;
}

ShroomAccountResult ShroomAccountRegister(ShroomAccountAuth* auth, const char* username,
                                          const char* email, const char* password,
                                          ShroomAccount* account, ShroomAccountTokenPair* tokens) {
  char normalized_email[SHROOM_ACCOUNT_EMAIL_MAX_LENGTH + 1u];
  char encoded_password[256];
  char family[ACCOUNT_FAMILY_BYTES * 2u + 1u];
  char refresh_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];
  sqlite3_stmt* statement = NULL;
  ShroomAccountResult duplicate;
  int64_t player_id;
  int64_t user_id;

  if ((auth == NULL) || (auth->db == NULL) || (account == NULL) || (tokens == NULL) ||
      !ShroomAccountValidateUsername(username) || !ShroomAccountValidateEmail(email) ||
      !ShroomAccountValidatePassword(password)) {
    return SHROOM_ACCOUNT_INVALID_INPUT;
  }
  NormalizeEmail(email, normalized_email);
  if (!ShroomAccountHashPassword(password, encoded_password, sizeof(encoded_password)) ||
      !RandomHex(ACCOUNT_FAMILY_BYTES, family)) {
    return SHROOM_ACCOUNT_CRYPTO_ERROR;
  }

  LockDatabase(auth);
  if (!ExecuteSql(auth->db, "BEGIN IMMEDIATE")) {
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_DATABASE_ERROR;
  }
  duplicate = FindDuplicate(auth->db, username, normalized_email);
  if (duplicate != SHROOM_ACCOUNT_SUCCESS) {
    Rollback(auth->db);
    UnlockDatabase(auth);
    OPENSSL_cleanse(encoded_password, sizeof(encoded_password));
    return duplicate;
  }

  // Create the profile rows before the user because users owns the player foreign key.
  if (sqlite3_prepare_v2(auth->db,
                         "INSERT INTO players (player_uuid, display_name) "
                         "VALUES (lower(hex(randomblob(16))), ?)",
                         -1, &statement, NULL) != SQLITE_OK) {
    goto database_failure;
  }
  sqlite3_bind_text(statement, 1, username, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    goto database_failure;
  }
  sqlite3_finalize(statement);
  statement = NULL;
  player_id = sqlite3_last_insert_rowid(auth->db);

  if (sqlite3_prepare_v2(auth->db, "INSERT INTO player_stats (player_id) VALUES (?)", -1,
                         &statement, NULL) != SQLITE_OK) {
    goto database_failure;
  }
  sqlite3_bind_int64(statement, 1, player_id);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    goto database_failure;
  }
  sqlite3_finalize(statement);
  statement = NULL;

  if (sqlite3_prepare_v2(
          auth->db,
          "INSERT INTO users (player_id, username, email, password_hash, auth_method) "
          "VALUES (?, ?, ?, ?, 'password')",
          -1, &statement, NULL) != SQLITE_OK) {
    goto database_failure;
  }
  sqlite3_bind_int64(statement, 1, player_id);
  sqlite3_bind_text(statement, 2, username, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 3, normalized_email, -1, SQLITE_TRANSIENT);
  // encoded_password is an Argon2id verifier produced with a random salt, not plaintext.
  // codeql[cpp/cleartext-storage-database]
  sqlite3_bind_text(statement, 4, encoded_password, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    goto database_failure;
  }
  sqlite3_finalize(statement);
  statement = NULL;
  user_id = sqlite3_last_insert_rowid(auth->db);

  // Issue both digested session tokens inside the registration transaction.
  if (!IssueTokenPair(auth->db, (uint32_t)user_id, family, tokens, refresh_digest) ||
      sqlite3_prepare_v2(auth->db,
                         "SELECT p.player_uuid, u.created_at FROM users u "
                         "JOIN players p ON p.id = u.player_id WHERE u.id = ?",
                         -1, &statement, NULL) != SQLITE_OK) {
    goto database_failure;
  }
  sqlite3_bind_int64(statement, 1, user_id);
  if (sqlite3_step(statement) != SQLITE_ROW) {
    goto database_failure;
  }
  account->user_id = (uint32_t)user_id;
  snprintf(account->player_id, sizeof(account->player_id), "%s", sqlite3_column_text(statement, 0));
  snprintf(account->username, sizeof(account->username), "%s", username);
  snprintf(account->email, sizeof(account->email), "%s", normalized_email);
  snprintf(account->created_at, sizeof(account->created_at), "%s",
           sqlite3_column_text(statement, 1));
  sqlite3_finalize(statement);
  statement = NULL;
  if (!ExecuteSql(auth->db, "COMMIT")) {
    goto database_failure;
  }
  UnlockDatabase(auth);
  OPENSSL_cleanse(encoded_password, sizeof(encoded_password));
  LOG_INFO("account_auth event=register user_id=%u", account->user_id);
  return SHROOM_ACCOUNT_SUCCESS;

database_failure:
  sqlite3_finalize(statement);
  Rollback(auth->db);
  UnlockDatabase(auth);
  OPENSSL_cleanse(encoded_password, sizeof(encoded_password));
  OPENSSL_cleanse(tokens, sizeof(*tokens));
  return SHROOM_ACCOUNT_DATABASE_ERROR;
}

static bool RunDummyPasswordVerification(const char* password) {
  static const unsigned char DUMMY_SALT[ACCOUNT_ARGON_SALT_LENGTH] = {
      0x73, 0x68, 0x72, 0x6f, 0x6f, 0x6d, 0x69, 0x6f,
      0x2d, 0x61, 0x75, 0x74, 0x68, 0x2d, 0x76, 0x31};
  char encoded[256];
  const bool hashed = HashPasswordWithSalt(password, DUMMY_SALT, encoded, sizeof(encoded));
  const bool verified = hashed && ShroomAccountVerifyPassword(encoded, password);
  OPENSSL_cleanse(encoded, sizeof(encoded));
  return verified;
}

ShroomAccountResult ShroomAccountLogin(ShroomAccountAuth* auth, const char* identity,
                                       const char* password, ShroomAccountTokenPair* tokens) {
  sqlite3_stmt* statement = NULL;
  char stored_hash[256] = {0};
  uint32_t user_id = 0u;
  char family[ACCOUNT_FAMILY_BYTES * 2u + 1u];
  char refresh_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];

  if ((auth == NULL) || (auth->db == NULL) || (tokens == NULL) || (identity == NULL) ||
      (identity[0] == '\0') || !ShroomAccountValidatePassword(password)) {
    return SHROOM_ACCOUNT_INVALID_INPUT;
  }
  LockDatabase(auth);
  if (sqlite3_prepare_v2(auth->db,
                         "SELECT id, password_hash FROM users "
                         "WHERE auth_method = 'password' AND (username = ? OR email = ?)",
                         -1, &statement, NULL) != SQLITE_OK) {
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_DATABASE_ERROR;
  }
  sqlite3_bind_text(statement, 1, identity, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, identity, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    const char* database_hash = (const char*)sqlite3_column_text(statement, 1);
    user_id = (uint32_t)sqlite3_column_int64(statement, 0);
    if (database_hash != NULL) {
      snprintf(stored_hash, sizeof(stored_hash), "%s", database_hash);
    }
  }
  sqlite3_finalize(statement);
  UnlockDatabase(auth);

  if (user_id == 0u) {
    (void)RunDummyPasswordVerification(password);
    return SHROOM_ACCOUNT_INVALID_CREDENTIALS;
  }
  if (!ShroomAccountVerifyPassword(stored_hash, password)) {
    return SHROOM_ACCOUNT_INVALID_CREDENTIALS;
  }
  if (!RandomHex(ACCOUNT_FAMILY_BYTES, family)) {
    return SHROOM_ACCOUNT_CRYPTO_ERROR;
  }

  LockDatabase(auth);
  if (!ExecuteSql(auth->db, "BEGIN IMMEDIATE") ||
      !IssueTokenPair(auth->db, user_id, family, tokens, refresh_digest) ||
      sqlite3_prepare_v2(auth->db,
                         "UPDATE users SET last_login_at = "
                         "strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = ?",
                         -1, &statement, NULL) != SQLITE_OK) {
    goto login_database_failure;
  }
  sqlite3_bind_int64(statement, 1, user_id);
  if ((sqlite3_step(statement) != SQLITE_DONE) || !ExecuteSql(auth->db, "COMMIT")) {
    goto login_database_failure;
  }
  sqlite3_finalize(statement);
  UnlockDatabase(auth);
  OPENSSL_cleanse(stored_hash, sizeof(stored_hash));
  LOG_INFO("account_auth event=login user_id=%u", user_id);
  return SHROOM_ACCOUNT_SUCCESS;

login_database_failure:
  sqlite3_finalize(statement);
  Rollback(auth->db);
  UnlockDatabase(auth);
  OPENSSL_cleanse(stored_hash, sizeof(stored_hash));
  OPENSSL_cleanse(tokens, sizeof(*tokens));
  return SHROOM_ACCOUNT_DATABASE_ERROR;
}

ShroomAccountResult ShroomAccountRefresh(ShroomAccountAuth* auth, const char* refresh_token,
                                         ShroomAccountTokenPair* tokens) {
  char token_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];
  char next_refresh_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];
  char family[ACCOUNT_FAMILY_BYTES * 2u + 1u] = {0};
  sqlite3_stmt* statement = NULL;
  uint32_t user_id = 0u;
  bool unavailable = false;

  if ((auth == NULL) || (auth->db == NULL) || (tokens == NULL) ||
      !HashToken(refresh_token, token_digest)) {
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }
  LockDatabase(auth);
  if (!ExecuteSql(auth->db, "BEGIN IMMEDIATE") ||
      sqlite3_prepare_v2(
          auth->db,
          "SELECT user_id, refresh_family_id, revoked_at IS NOT NULL, rotated_at IS NOT NULL, "
          "expires_at <= strftime('%Y-%m-%dT%H:%M:%SZ', 'now') "
          "FROM auth_tokens WHERE token_hash = ? AND token_kind = 'refresh'",
          -1, &statement, NULL) != SQLITE_OK) {
    goto refresh_database_failure;
  }
  sqlite3_bind_text(statement, 1, token_digest, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    const char* database_family = (const char*)sqlite3_column_text(statement, 1);
    user_id = (uint32_t)sqlite3_column_int64(statement, 0);
    unavailable = (sqlite3_column_int(statement, 2) != 0) ||
                  (sqlite3_column_int(statement, 3) != 0) ||
                  (sqlite3_column_int(statement, 4) != 0);
    if (database_family != NULL) {
      snprintf(family, sizeof(family), "%s", database_family);
    }
  }
  sqlite3_finalize(statement);
  statement = NULL;
  if ((user_id == 0u) || (family[0] == '\0')) {
    Rollback(auth->db);
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }
  if (unavailable) {
    if (sqlite3_prepare_v2(auth->db,
                           "UPDATE auth_tokens SET revoked_at = COALESCE(revoked_at, "
                           "strftime('%Y-%m-%dT%H:%M:%SZ', 'now')) WHERE refresh_family_id = ?",
                           -1, &statement, NULL) != SQLITE_OK) {
      goto refresh_database_failure;
    }
    sqlite3_bind_text(statement, 1, family, -1, SQLITE_TRANSIENT);
    if ((sqlite3_step(statement) != SQLITE_DONE) || !ExecuteSql(auth->db, "COMMIT")) {
      goto refresh_database_failure;
    }
    sqlite3_finalize(statement);
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }

  if (!IssueTokenPair(auth->db, user_id, family, tokens, next_refresh_digest) ||
      sqlite3_prepare_v2(auth->db,
                         "UPDATE auth_tokens SET revoked_at = "
                         "strftime('%Y-%m-%dT%H:%M:%SZ', 'now'), rotated_at = "
                         "strftime('%Y-%m-%dT%H:%M:%SZ', 'now'), replaced_by_hash = ? "
                         "WHERE token_hash = ?",
                         -1, &statement, NULL) != SQLITE_OK) {
    goto refresh_database_failure;
  }
  sqlite3_bind_text(statement, 1, next_refresh_digest, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(statement, 2, token_digest, -1, SQLITE_TRANSIENT);
  if ((sqlite3_step(statement) != SQLITE_DONE) || !ExecuteSql(auth->db, "COMMIT")) {
    goto refresh_database_failure;
  }
  sqlite3_finalize(statement);
  UnlockDatabase(auth);
  LOG_INFO("account_auth event=refresh user_id=%u", user_id);
  return SHROOM_ACCOUNT_SUCCESS;

refresh_database_failure:
  sqlite3_finalize(statement);
  Rollback(auth->db);
  UnlockDatabase(auth);
  OPENSSL_cleanse(tokens, sizeof(*tokens));
  return SHROOM_ACCOUNT_DATABASE_ERROR;
}

ShroomAccountResult ShroomAccountLogout(ShroomAccountAuth* auth, const char* access_token) {
  char token_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];
  char family[ACCOUNT_FAMILY_BYTES * 2u + 1u] = {0};
  sqlite3_stmt* statement = NULL;

  if ((auth == NULL) || (auth->db == NULL) || !HashToken(access_token, token_digest)) {
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }
  LockDatabase(auth);
  if (!ExecuteSql(auth->db, "BEGIN IMMEDIATE") ||
      sqlite3_prepare_v2(auth->db,
                         "SELECT refresh_family_id FROM auth_tokens "
                         "WHERE token_hash = ? AND token_kind = 'access'",
                         -1, &statement, NULL) != SQLITE_OK) {
    goto logout_database_failure;
  }
  sqlite3_bind_text(statement, 1, token_digest, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    const char* database_family = (const char*)sqlite3_column_text(statement, 0);
    if (database_family != NULL) {
      snprintf(family, sizeof(family), "%s", database_family);
    }
  }
  sqlite3_finalize(statement);
  statement = NULL;
  if (family[0] == '\0') {
    Rollback(auth->db);
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }
  if (sqlite3_prepare_v2(auth->db,
                         "UPDATE auth_tokens SET revoked_at = COALESCE(revoked_at, "
                         "strftime('%Y-%m-%dT%H:%M:%SZ', 'now')) WHERE refresh_family_id = ?",
                         -1, &statement, NULL) != SQLITE_OK) {
    goto logout_database_failure;
  }
  sqlite3_bind_text(statement, 1, family, -1, SQLITE_TRANSIENT);
  if ((sqlite3_step(statement) != SQLITE_DONE) || !ExecuteSql(auth->db, "COMMIT")) {
    goto logout_database_failure;
  }
  sqlite3_finalize(statement);
  UnlockDatabase(auth);
  return SHROOM_ACCOUNT_SUCCESS;

logout_database_failure:
  sqlite3_finalize(statement);
  Rollback(auth->db);
  UnlockDatabase(auth);
  return SHROOM_ACCOUNT_DATABASE_ERROR;
}

ShroomAccountResult ShroomAccountIdentifyAccess(ShroomAccountAuth* auth, const char* access_token,
                                                uint32_t* user_id) {
  char token_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];
  sqlite3_stmt* statement = NULL;
  ShroomAccountResult result = SHROOM_ACCOUNT_INVALID_TOKEN;

  if ((auth == NULL) || (auth->db == NULL) || (user_id == NULL) ||
      !HashToken(access_token, token_digest)) {
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }
  LockDatabase(auth);
  if (sqlite3_prepare_v2(auth->db,
                         "SELECT user_id FROM auth_tokens "
                         "WHERE token_hash = ? AND token_kind = 'access'",
                         -1, &statement, NULL) != SQLITE_OK) {
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_DATABASE_ERROR;
  }
  sqlite3_bind_text(statement, 1, token_digest, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    *user_id = (uint32_t)sqlite3_column_int64(statement, 0);
    result = SHROOM_ACCOUNT_SUCCESS;
  }
  sqlite3_finalize(statement);
  UnlockDatabase(auth);
  return result;
}

ShroomAccountResult ShroomAccountGetMe(ShroomAccountAuth* auth, const char* access_token,
                                       ShroomAccount* account) {
  char token_digest[ACCOUNT_TOKEN_HASH_LENGTH + 1u];
  sqlite3_stmt* statement = NULL;
  ShroomAccountResult result = SHROOM_ACCOUNT_INVALID_TOKEN;

  if ((auth == NULL) || (auth->db == NULL) || (account == NULL) ||
      !HashToken(access_token, token_digest)) {
    return SHROOM_ACCOUNT_INVALID_TOKEN;
  }
  LockDatabase(auth);
  if (sqlite3_prepare_v2(
          auth->db,
          "SELECT u.id, p.player_uuid, u.username, u.email, u.created_at "
          "FROM auth_tokens t JOIN users u ON u.id = t.user_id "
          "JOIN players p ON p.id = u.player_id "
          "WHERE t.token_hash = ? AND t.token_kind = 'access' AND t.revoked_at IS NULL "
          "AND t.expires_at > strftime('%Y-%m-%dT%H:%M:%SZ', 'now')",
          -1, &statement, NULL) != SQLITE_OK) {
    UnlockDatabase(auth);
    return SHROOM_ACCOUNT_DATABASE_ERROR;
  }
  sqlite3_bind_text(statement, 1, token_digest, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(statement) == SQLITE_ROW) {
    account->user_id = (uint32_t)sqlite3_column_int64(statement, 0);
    snprintf(account->player_id, sizeof(account->player_id), "%s",
             sqlite3_column_text(statement, 1));
    snprintf(account->username, sizeof(account->username), "%s", sqlite3_column_text(statement, 2));
    snprintf(account->email, sizeof(account->email), "%s", sqlite3_column_text(statement, 3));
    snprintf(account->created_at, sizeof(account->created_at), "%s",
             sqlite3_column_text(statement, 4));
    result = SHROOM_ACCOUNT_SUCCESS;
  }
  sqlite3_finalize(statement);
  UnlockDatabase(auth);
  return result;
}
