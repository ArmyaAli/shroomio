#ifndef SHROOM_ACCOUNT_AUTH_H
#define SHROOM_ACCOUNT_AUTH_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sqlite3.h>

#define SHROOM_ACCOUNT_EMAIL_MAX_LENGTH 254u
#define SHROOM_ACCOUNT_PASSWORD_MIN_LENGTH 12u
#define SHROOM_ACCOUNT_PASSWORD_MAX_LENGTH 128u
#define SHROOM_ACCOUNT_PLAYER_ID_LENGTH 32u
#define SHROOM_ACCOUNT_TOKEN_LENGTH 43u
#define SHROOM_ACCOUNT_ACCESS_EXPIRES_SECONDS 900u
#define SHROOM_ACCOUNT_REFRESH_EXPIRES_SECONDS 2592000u

typedef enum ShroomAccountResult {
  SHROOM_ACCOUNT_SUCCESS = 0,
  SHROOM_ACCOUNT_INVALID_INPUT,
  SHROOM_ACCOUNT_INVALID_CREDENTIALS,
  SHROOM_ACCOUNT_INVALID_TOKEN,
  SHROOM_ACCOUNT_USERNAME_TAKEN,
  SHROOM_ACCOUNT_EMAIL_TAKEN,
  SHROOM_ACCOUNT_DATABASE_ERROR,
  SHROOM_ACCOUNT_CRYPTO_ERROR,
} ShroomAccountResult;

typedef struct ShroomAccount {
  uint32_t user_id;
  char player_id[SHROOM_ACCOUNT_PLAYER_ID_LENGTH + 1u];
  char username[33];
  char email[SHROOM_ACCOUNT_EMAIL_MAX_LENGTH + 1u];
  char created_at[32];
} ShroomAccount;

typedef struct ShroomAccountTokenPair {
  char access_token[SHROOM_ACCOUNT_TOKEN_LENGTH + 1u];
  char refresh_token[SHROOM_ACCOUNT_TOKEN_LENGTH + 1u];
  uint32_t access_expires_in;
  uint32_t refresh_expires_in;
} ShroomAccountTokenPair;

typedef struct ShroomAccountAuth {
  sqlite3* db;
  atomic_flag database_lock;
} ShroomAccountAuth;

void ShroomAccountAuthInit(ShroomAccountAuth* auth, sqlite3* db);
void ShroomAccountAuthShutdown(ShroomAccountAuth* auth);

bool ShroomAccountValidateUsername(const char* username);
bool ShroomAccountValidateEmail(const char* email);
bool ShroomAccountValidatePassword(const char* password);
bool ShroomAccountHashPassword(const char* password, char* encoded, size_t encoded_size);
bool ShroomAccountVerifyPassword(const char* encoded, const char* password);

ShroomAccountResult ShroomAccountRegister(ShroomAccountAuth* auth, const char* username,
                                          const char* email, const char* password,
                                          ShroomAccount* account, ShroomAccountTokenPair* tokens);
ShroomAccountResult ShroomAccountLogin(ShroomAccountAuth* auth, const char* identity,
                                       const char* password, ShroomAccountTokenPair* tokens);
ShroomAccountResult ShroomAccountRefresh(ShroomAccountAuth* auth, const char* refresh_token,
                                         ShroomAccountTokenPair* tokens);
ShroomAccountResult ShroomAccountLogout(ShroomAccountAuth* auth, const char* access_token);
ShroomAccountResult ShroomAccountIdentifyAccess(ShroomAccountAuth* auth, const char* access_token,
                                                uint32_t* user_id);
ShroomAccountResult ShroomAccountGetMe(ShroomAccountAuth* auth, const char* access_token,
                                       ShroomAccount* account);

#endif
