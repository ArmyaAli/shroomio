#include "unity.h"
#include "../src/server/auth.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>

static sqlite3* test_db = NULL;
static ShroomAuthContext auth_ctx;

void setUp(void) {
  // Create in-memory database for testing
  TEST_ASSERT_EQUAL(SQLITE_OK, sqlite3_open(":memory:", &test_db));
  ShroomAuthInit(&auth_ctx, test_db);

  // Create schema
  const char* schema_sql =
      "CREATE TABLE users ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "player_id INTEGER NOT NULL UNIQUE,"
      "username TEXT NOT NULL UNIQUE,"
      "password_hash TEXT,"
      "auth_method TEXT NOT NULL,"
      "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
      "last_login_at TEXT DEFAULT CURRENT_TIMESTAMP"
      ");"
      "CREATE TABLE auth_tokens ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "token TEXT NOT NULL UNIQUE,"
      "expires_at TEXT NOT NULL,"
      "created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
      "revoked_at TEXT,"
      "FOREIGN KEY (user_id) REFERENCES users(id)"
      ");"
      "CREATE TABLE players ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "player_uuid TEXT NOT NULL UNIQUE,"
      "display_name TEXT NOT NULL,"
      "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
      ");"
      "CREATE TABLE player_stats ("
      "player_id INTEGER PRIMARY KEY,"
      "total_games INTEGER DEFAULT 0,"
      "total_kills INTEGER DEFAULT 0,"
      "total_deaths INTEGER DEFAULT 0,"
      "highest_mass REAL DEFAULT 0,"
      "longest_survival REAL DEFAULT 0,"
      "FOREIGN KEY (player_id) REFERENCES players(id)"
      ");";

  char* err_msg = NULL;
  TEST_ASSERT_EQUAL(SQLITE_OK, sqlite3_exec(test_db, schema_sql, NULL, NULL, &err_msg));
}

void tearDown(void) {
  ShroomAuthShutdown(&auth_ctx);
  if (test_db != NULL) {
    sqlite3_close(test_db);
    test_db = NULL;
  }
}

void test_auth_register_success(void) {
  ShroomAuthUser user;
  ShroomAuthResult result = ShroomAuthRegister(&auth_ctx, "testuser", "password123", &user);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, result);
  TEST_ASSERT_NOT_EQUAL(0, user.user_id);
  TEST_ASSERT_NOT_EQUAL(0, user.player_id);
  TEST_ASSERT_EQUAL_STRING("testuser", user.username);
  TEST_ASSERT_EQUAL(SHROOM_AUTH_PASSWORD, user.auth_method);
}

void test_auth_register_duplicate_username(void) {
  ShroomAuthUser user1, user2;
  ShroomAuthResult result1 = ShroomAuthRegister(&auth_ctx, "testuser", "password123", &user1);
  ShroomAuthResult result2 = ShroomAuthRegister(&auth_ctx, "testuser", "password456", &user2);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, result1);
  TEST_ASSERT_EQUAL(SHROOM_AUTH_USERNAME_TAKEN, result2);
}

void test_auth_register_invalid_username(void) {
  ShroomAuthUser user;
  ShroomAuthResult result = ShroomAuthRegister(&auth_ctx, "ab", "password123", &user);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_INPUT, result);
}

void test_auth_register_invalid_password(void) {
  ShroomAuthUser user;
  ShroomAuthResult result = ShroomAuthRegister(&auth_ctx, "testuser", "123", &user);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_INPUT, result);
}

void test_auth_login_success(void) {
  ShroomAuthUser user;
  ShroomAuthToken token;

  ShroomAuthRegister(&auth_ctx, "testuser", "password123", &user);
  ShroomAuthResult result = ShroomAuthLogin(&auth_ctx, "testuser", "password123", &token);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, result);
  TEST_ASSERT_NOT_EQUAL(0, token.user_id);
  TEST_ASSERT_NOT_EQUAL(0, strlen(token.token));
}

void test_auth_login_wrong_password(void) {
  ShroomAuthUser user;
  ShroomAuthToken token;

  ShroomAuthRegister(&auth_ctx, "testuser", "password123", &user);
  ShroomAuthResult result = ShroomAuthLogin(&auth_ctx, "testuser", "wrongpassword", &token);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_CREDENTIALS, result);
}

void test_auth_login_nonexistent_user(void) {
  ShroomAuthToken token;
  ShroomAuthResult result = ShroomAuthLogin(&auth_ctx, "nonexistent", "password123", &token);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_CREDENTIALS, result);
}

void test_auth_anonymous_login_success(void) {
  ShroomAuthToken token;
  ShroomAuthResult result = ShroomAuthLoginAnonymous(&auth_ctx, "guest123", &token);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, result);
  TEST_ASSERT_NOT_EQUAL(0, token.user_id);
  TEST_ASSERT_NOT_EQUAL(0, strlen(token.token));
}

void test_auth_anonymous_login_reuse_username(void) {
  ShroomAuthToken token1, token2;

  ShroomAuthLoginAnonymous(&auth_ctx, "guest123", &token1);
  ShroomAuthResult result = ShroomAuthLoginAnonymous(&auth_ctx, "guest123", &token2);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, result);
  TEST_ASSERT_EQUAL(token1.user_id, token2.user_id);
}

void test_auth_anonymous_login_rejects_registered_username(void) {
  ShroomAuthUser registered_user;
  ShroomAuthToken token = {.user_id = UINT32_MAX};
  sqlite3_stmt* statement = NULL;

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS,
                    ShroomAuthRegister(&auth_ctx, "registered", "password123", &registered_user));

  TEST_ASSERT_EQUAL(SHROOM_AUTH_USERNAME_TAKEN,
                    ShroomAuthLoginAnonymous(&auth_ctx, "registered", &token));
  TEST_ASSERT_EQUAL(UINT32_MAX, token.user_id);
  TEST_ASSERT_EQUAL(SQLITE_OK,
                    sqlite3_prepare_v2(test_db,
                                       "SELECT auth_method, COUNT(*) FROM users WHERE username = ?",
                                       -1, &statement, NULL));
  sqlite3_bind_text(statement, 1, "registered", -1, SQLITE_STATIC);
  TEST_ASSERT_EQUAL(SQLITE_ROW, sqlite3_step(statement));
  TEST_ASSERT_EQUAL_STRING("password", sqlite3_column_text(statement, 0));
  TEST_ASSERT_EQUAL_INT(1, sqlite3_column_int(statement, 1));
  sqlite3_finalize(statement);
  statement = NULL;

  TEST_ASSERT_EQUAL(SQLITE_OK,
                    sqlite3_prepare_v2(test_db,
                                       "SELECT COUNT(*) FROM auth_tokens WHERE user_id = ?", -1,
                                       &statement, NULL));
  sqlite3_bind_int64(statement, 1, registered_user.user_id);
  TEST_ASSERT_EQUAL(SQLITE_ROW, sqlite3_step(statement));
  TEST_ASSERT_EQUAL_INT(0, sqlite3_column_int(statement, 0));
  sqlite3_finalize(statement);
}

void test_auth_validate_token_success(void) {
  ShroomAuthToken token;
  ShroomAuthUser user;

  ShroomAuthLoginAnonymous(&auth_ctx, "guest123", &token);
  ShroomAuthResult result = ShroomAuthValidateToken(&auth_ctx, token.token, &user);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, result);
  TEST_ASSERT_EQUAL(token.user_id, user.user_id);
  TEST_ASSERT_EQUAL_STRING("guest123", user.username);
  TEST_ASSERT_EQUAL(SHROOM_AUTH_ANONYMOUS, user.auth_method);
}

void test_auth_validate_token_invalid(void) {
  ShroomAuthUser user;
  ShroomAuthResult result = ShroomAuthValidateToken(&auth_ctx, "invalid_token_12345", &user);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_TOKEN, result);
}

void test_auth_revoke_token_success(void) {
  ShroomAuthToken token;
  ShroomAuthUser user;

  ShroomAuthLoginAnonymous(&auth_ctx, "guest123", &token);
  ShroomAuthResult revoke_result = ShroomAuthRevokeToken(&auth_ctx, token.token);
  ShroomAuthResult validate_result = ShroomAuthValidateToken(&auth_ctx, token.token, &user);

  TEST_ASSERT_EQUAL(SHROOM_AUTH_SUCCESS, revoke_result);
  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_TOKEN, validate_result);
}

void test_auth_revoke_token_invalid(void) {
  ShroomAuthResult result = ShroomAuthRevokeToken(&auth_ctx, "invalid_token_12345");

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_TOKEN, result);
}

void test_auth_null_context(void) {
  ShroomAuthUser user;
  ShroomAuthToken token;

  TEST_ASSERT_EQUAL(SHROOM_AUTH_DATABASE_ERROR,
                    ShroomAuthRegister(NULL, "testuser", "password123", &user));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_DATABASE_ERROR,
                    ShroomAuthLogin(NULL, "testuser", "password123", &token));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_DATABASE_ERROR,
                    ShroomAuthLoginAnonymous(NULL, "guest123", &token));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_TOKEN,
                    ShroomAuthValidateToken(NULL, "token", &user));
}

void test_auth_null_parameters(void) {
  ShroomAuthUser user;
  ShroomAuthToken token;

  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_INPUT,
                    ShroomAuthRegister(&auth_ctx, NULL, "password123", &user));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_INPUT,
                    ShroomAuthRegister(&auth_ctx, "testuser", NULL, &user));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_INPUT,
                    ShroomAuthLogin(&auth_ctx, NULL, "password123", &token));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_INPUT,
                    ShroomAuthLoginAnonymous(&auth_ctx, NULL, &token));
  TEST_ASSERT_EQUAL(SHROOM_AUTH_INVALID_TOKEN,
                    ShroomAuthValidateToken(&auth_ctx, NULL, &user));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_auth_register_success);
  RUN_TEST(test_auth_register_duplicate_username);
  RUN_TEST(test_auth_register_invalid_username);
  RUN_TEST(test_auth_register_invalid_password);
  RUN_TEST(test_auth_login_success);
  RUN_TEST(test_auth_login_wrong_password);
  RUN_TEST(test_auth_login_nonexistent_user);
  RUN_TEST(test_auth_anonymous_login_success);
  RUN_TEST(test_auth_anonymous_login_reuse_username);
  RUN_TEST(test_auth_anonymous_login_rejects_registered_username);
  RUN_TEST(test_auth_validate_token_success);
  RUN_TEST(test_auth_validate_token_invalid);
  RUN_TEST(test_auth_revoke_token_success);
  RUN_TEST(test_auth_revoke_token_invalid);
  RUN_TEST(test_auth_null_context);
  RUN_TEST(test_auth_null_parameters);
  return UNITY_END();
}
