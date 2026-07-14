#include "unity.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#include "server/account_auth.h"
#include "server/database.h"

static sqlite3* database;
static ShroomAccountAuth auth;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(":memory:", &database));
  TEST_ASSERT_TRUE(ShroomDatabaseInitializeSchema(database));
  ShroomAccountAuthInit(&auth, database);
}

void tearDown(void) {
  ShroomAccountAuthShutdown(&auth);
  sqlite3_close(database);
}

static void test_argon2id_hash_records_parameters_and_verifies(void) {
  char encoded[256];

  TEST_ASSERT_TRUE(ShroomAccountHashPassword("correct horse battery", encoded, sizeof(encoded)));
  if (getenv("SHROOM_VALGRIND") == NULL) {
    TEST_ASSERT_NOT_NULL(strstr(encoded, "$argon2id$v=19$m=65536,t=3,p=1$"));
  } else {
    TEST_ASSERT_NOT_NULL(strstr(encoded, "$argon2id$v=19$m=1024,t=1,p=1$"));
  }
  TEST_ASSERT_TRUE(ShroomAccountVerifyPassword(encoded, "correct horse battery"));
  TEST_ASSERT_FALSE(ShroomAccountVerifyPassword(encoded, "incorrect horse test"));
}

static void test_register_normalizes_email_and_stores_only_hashes(void) {
  ShroomAccount account = {0};
  ShroomAccountTokenPair tokens = {0};
  sqlite3_stmt* statement = NULL;

  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountRegister(&auth, "forest_cap", "Player@Example.COM",
                                              "correct horse battery", &account, &tokens));
  TEST_ASSERT_EQUAL_STRING("player@example.com", account.email);
  TEST_ASSERT_EQUAL_UINT32(SHROOM_ACCOUNT_TOKEN_LENGTH, strlen(tokens.access_token));
  TEST_ASSERT_EQUAL_UINT32(SHROOM_ACCOUNT_TOKEN_LENGTH, strlen(tokens.refresh_token));
  TEST_ASSERT_EQUAL_UINT32(SHROOM_ACCOUNT_ACCESS_EXPIRES_SECONDS, tokens.access_expires_in);

  TEST_ASSERT_EQUAL_INT(
      SQLITE_OK,
      sqlite3_prepare_v2(database,
                         "SELECT password_hash LIKE '$argon2id$%', "
                         "(SELECT count(*) FROM auth_tokens WHERE token IS NULL "
                         "AND length(token_hash) = 64) FROM users WHERE id = ?",
                         -1, &statement, NULL));
  sqlite3_bind_int64(statement, 1, account.user_id);
  TEST_ASSERT_EQUAL_INT(SQLITE_ROW, sqlite3_step(statement));
  TEST_ASSERT_EQUAL_INT(1, sqlite3_column_int(statement, 0));
  TEST_ASSERT_EQUAL_INT(2, sqlite3_column_int(statement, 1));
  sqlite3_finalize(statement);
}

static void test_duplicate_username_and_normalized_email_are_distinct_conflicts(void) {
  ShroomAccount account = {0};
  ShroomAccountTokenPair tokens = {0};

  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountRegister(&auth, "forest_cap", "one@example.com",
                                              "correct horse battery", &account, &tokens));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_USERNAME_TAKEN,
                        ShroomAccountRegister(&auth, "forest_cap", "two@example.com",
                                              "different secure phrase", &account, &tokens));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_EMAIL_TAKEN,
                        ShroomAccountRegister(&auth, "other_cap", "ONE@EXAMPLE.COM",
                                              "different secure phrase", &account, &tokens));
}

static void test_login_accepts_email_and_rejects_unknown_like_wrong_password(void) {
  ShroomAccount account = {0};
  ShroomAccountTokenPair registered = {0};
  ShroomAccountTokenPair logged_in = {0};

  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountRegister(&auth, "forest_cap", "player@example.com",
                                              "correct horse battery", &account, &registered));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountLogin(&auth, "PLAYER@EXAMPLE.COM", "correct horse battery",
                                           &logged_in));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_INVALID_CREDENTIALS,
                        ShroomAccountLogin(&auth, "PLAYER@EXAMPLE.COM", "wrong password phrase",
                                           &logged_in));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_INVALID_CREDENTIALS,
                        ShroomAccountLogin(&auth, "missing@example.com", "wrong password phrase",
                                           &logged_in));
}

static void test_refresh_rotates_once_and_reuse_revokes_family(void) {
  ShroomAccount account = {0};
  ShroomAccount profile = {0};
  ShroomAccountTokenPair first = {0};
  ShroomAccountTokenPair second = {0};

  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountRegister(&auth, "forest_cap", "player@example.com",
                                              "correct horse battery", &account, &first));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountRefresh(&auth, first.refresh_token, &second));
  TEST_ASSERT_NOT_EQUAL(0, strcmp(first.refresh_token, second.refresh_token));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountGetMe(&auth, second.access_token, &profile));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_INVALID_TOKEN,
                        ShroomAccountRefresh(&auth, first.refresh_token, &first));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_INVALID_TOKEN,
                        ShroomAccountGetMe(&auth, second.access_token, &profile));
}

static void test_logout_is_idempotent_and_revokes_access_family(void) {
  ShroomAccount account = {0};
  ShroomAccount profile = {0};
  ShroomAccountTokenPair tokens = {0};

  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS,
                        ShroomAccountRegister(&auth, "forest_cap", "player@example.com",
                                              "correct horse battery", &account, &tokens));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS, ShroomAccountLogout(&auth, tokens.access_token));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_SUCCESS, ShroomAccountLogout(&auth, tokens.access_token));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_INVALID_TOKEN,
                        ShroomAccountGetMe(&auth, tokens.access_token, &profile));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_INVALID_TOKEN,
                        ShroomAccountRefresh(&auth, tokens.refresh_token, &tokens));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_argon2id_hash_records_parameters_and_verifies);
  RUN_TEST(test_register_normalizes_email_and_stores_only_hashes);
  RUN_TEST(test_duplicate_username_and_normalized_email_are_distinct_conflicts);
  RUN_TEST(test_login_accepts_email_and_rejects_unknown_like_wrong_password);
  RUN_TEST(test_refresh_rotates_once_and_reuse_revokes_family);
  RUN_TEST(test_logout_is_idempotent_and_revokes_access_family);
  return UNITY_END();
}
