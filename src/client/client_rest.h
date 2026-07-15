#ifndef SHROOM_CLIENT_REST_H
#define SHROOM_CLIENT_REST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "client_session_store.h"

#define SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX 128u
#define SHROOM_CLIENT_REST_ERROR_CODE_MAX 40u
#define SHROOM_CLIENT_REST_ERROR_MESSAGE_MAX 192u
#define SHROOM_CLIENT_REST_PLAYER_ID_MAX 64u
#define SHROOM_CLIENT_REST_USERNAME_MAX 32u
#define SHROOM_CLIENT_REST_EMAIL_MAX 254u
#define SHROOM_CLIENT_REST_TIMESTAMP_MAX 32u
#define SHROOM_CLIENT_REST_CHECKOUT_ID_MAX 96u
#define SHROOM_CLIENT_REST_CHECKOUT_URL_MAX 1024u
#define SHROOM_CLIENT_REST_RESPONSE_MAX 65536u

typedef enum ShroomClientRestResult {
  SHROOM_CLIENT_REST_OK = 0,
  SHROOM_CLIENT_REST_INVALID_ARGUMENT,
  SHROOM_CLIENT_REST_INSECURE_URL,
  SHROOM_CLIENT_REST_TRANSPORT_ERROR,
  SHROOM_CLIENT_REST_PROTOCOL_ERROR,
  SHROOM_CLIENT_REST_STORAGE_ERROR,
  SHROOM_CLIENT_REST_UNAUTHORIZED,
  SHROOM_CLIENT_REST_SERVER_ERROR,
} ShroomClientRestResult;

typedef struct ShroomClientHttpRequest {
  const char* method;
  const char* url;
  const char* authorization;
  const char* idempotency_key;
  const char* json_body;
} ShroomClientHttpRequest;

typedef struct ShroomClientHttpResponse {
  long status;
  char* body;
  size_t body_capacity;
  size_t body_length;
  char transport_error[SHROOM_CLIENT_REST_ERROR_MESSAGE_MAX];
} ShroomClientHttpResponse;

typedef bool (*ShroomClientRestTransport)(void* context, const ShroomClientHttpRequest* request,
                                          ShroomClientHttpResponse* response);
typedef uint64_t (*ShroomClientRestNowFn)(void* context);

typedef struct ShroomClientAccountProfile {
  char player_id[SHROOM_CLIENT_REST_PLAYER_ID_MAX];
  char username[SHROOM_CLIENT_REST_USERNAME_MAX + 1u];
  char email[SHROOM_CLIENT_REST_EMAIL_MAX + 1u];
  char created_at[SHROOM_CLIENT_REST_TIMESTAMP_MAX];
} ShroomClientAccountProfile;

typedef struct ShroomClientCheckout {
  char checkout_id[SHROOM_CLIENT_REST_CHECKOUT_ID_MAX];
  char checkout_url[SHROOM_CLIENT_REST_CHECKOUT_URL_MAX];
  char expires_at[SHROOM_CLIENT_REST_TIMESTAMP_MAX];
} ShroomClientCheckout;

typedef struct ShroomClientRestConfig {
  const char* base_url;
  const char* session_path;
  bool development_mode;
  ShroomClientRestTransport transport;
  void* transport_context;
  ShroomClientRestNowFn now;
  void* now_context;
} ShroomClientRestConfig;

typedef struct ShroomClientRest {
  char base_url[SHROOM_CLIENT_SESSION_BASE_URL_MAX];
  char session_path[SHROOM_CLIENT_SESSION_PATH_MAX];
  char access_token[SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX];
  char refresh_token[SHROOM_CLIENT_SESSION_REFRESH_TOKEN_MAX];
  uint64_t access_expires_at;
  uint64_t refresh_expires_at;
  ShroomClientAccountProfile profile;
  char error_code[SHROOM_CLIENT_REST_ERROR_CODE_MAX];
  char error_message[SHROOM_CLIENT_REST_ERROR_MESSAGE_MAX];
  bool development_mode;
  bool authenticated;
  ShroomClientRestTransport transport;
  void* transport_context;
  ShroomClientRestNowFn now;
  void* now_context;
} ShroomClientRest;

bool ShroomClientRestInit(ShroomClientRest* rest, const ShroomClientRestConfig* config);
void ShroomClientRestClear(ShroomClientRest* rest);
bool ShroomClientRestHasStoredSession(const ShroomClientRest* rest);

ShroomClientRestResult ShroomClientRestRegister(ShroomClientRest* rest, const char* username,
                                                const char* email, const char* password);
ShroomClientRestResult ShroomClientRestLogin(ShroomClientRest* rest, const char* identity,
                                             const char* password);
ShroomClientRestResult ShroomClientRestRestore(ShroomClientRest* rest);
ShroomClientRestResult ShroomClientRestRefresh(ShroomClientRest* rest);
ShroomClientRestResult ShroomClientRestLogout(ShroomClientRest* rest);
ShroomClientRestResult ShroomClientRestGetMe(ShroomClientRest* rest);
ShroomClientRestResult ShroomClientRestBeginCheckout(ShroomClientRest* rest, const char* sku,
                                                     int quantity, const char* success_url,
                                                     const char* cancel_url,
                                                     ShroomClientCheckout* checkout);

#endif
