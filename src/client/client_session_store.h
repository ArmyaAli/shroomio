#ifndef SHROOM_CLIENT_SESSION_STORE_H
#define SHROOM_CLIENT_SESSION_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHROOM_CLIENT_SESSION_PATH_MAX 512u
#define SHROOM_CLIENT_SESSION_BASE_URL_MAX 256u
#define SHROOM_CLIENT_SESSION_REFRESH_TOKEN_MAX 128u

typedef struct ShroomClientStoredSession {
  char base_url[SHROOM_CLIENT_SESSION_BASE_URL_MAX];
  char refresh_token[SHROOM_CLIENT_SESSION_REFRESH_TOKEN_MAX];
  uint64_t refresh_expires_at;
} ShroomClientStoredSession;

bool ShroomClientSessionDefaultPath(char* path, size_t path_size);
bool ShroomClientSessionSave(const char* path, const ShroomClientStoredSession* session);
bool ShroomClientSessionLoad(const char* path, ShroomClientStoredSession* session);
bool ShroomClientSessionDelete(const char* path);

#endif
