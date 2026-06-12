#ifndef SHROOM_AUTH_H
#define SHROOM_AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include <sqlite3.h>

#include "shared/protocol.h"

#define SHROOM_AUTH_PASSWORD_MIN_LENGTH 6
#define SHROOM_AUTH_USERNAME_MIN_LENGTH 3
#define SHROOM_AUTH_USERNAME_MAX_LENGTH 32

typedef struct ShroomAuthUser {
  uint32_t user_id;
  uint32_t player_id;
  char username[SHROOM_AUTH_USERNAME_MAX_LENGTH + 1];
  ShroomAuthMethod auth_method;
} ShroomAuthUser;

typedef struct ShroomAuthToken {
  char token[SHROOM_AUTH_TOKEN_LENGTH + 1];
  uint32_t user_id;
  uint64_t expires_at;
} ShroomAuthToken;

typedef struct ShroomAuthContext {
  sqlite3* db;
} ShroomAuthContext;

void ShroomAuthInit(ShroomAuthContext* ctx, sqlite3* db);
void ShroomAuthShutdown(ShroomAuthContext* ctx);

ShroomAuthResult ShroomAuthRegister(ShroomAuthContext* ctx, const char* username,
                                    const char* password, ShroomAuthUser* out_user);

ShroomAuthResult ShroomAuthLogin(ShroomAuthContext* ctx, const char* username, const char* password,
                                 ShroomAuthToken* out_token);

ShroomAuthResult ShroomAuthLoginAnonymous(ShroomAuthContext* ctx, const char* username,
                                          ShroomAuthToken* out_token);

ShroomAuthResult ShroomAuthValidateToken(ShroomAuthContext* ctx, const char* token,
                                         ShroomAuthUser* out_user);

ShroomAuthResult ShroomAuthRevokeToken(ShroomAuthContext* ctx, const char* token);

#endif
