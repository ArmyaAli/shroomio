#include "client_rest.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cjson/cJSON.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

#define ACCESS_REFRESH_SKEW_SECONDS 30u

static uint64_t DefaultNow(void* context) {
  const time_t now = time(NULL);

  (void)context;
  return now >= 0 ? (uint64_t)now : 0u;
}

static uint64_t AddLifetime(uint64_t now, uint64_t lifetime) {
  return lifetime <= UINT64_MAX - now ? now + lifetime : UINT64_MAX;
}

static void SetError(ShroomClientRest* rest, const char* code, const char* message) {
  snprintf(rest->error_code, sizeof(rest->error_code), "%s", code != NULL ? code : "");
  snprintf(rest->error_message, sizeof(rest->error_message), "%s", message != NULL ? message : "");
}

static void CleanseJsonString(cJSON* object, const char* name) {
  cJSON* value = object != NULL ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;

  if (cJSON_IsString(value) && (value->valuestring != NULL)) {
    OPENSSL_cleanse(value->valuestring, strlen(value->valuestring));
  }
}

static bool NormalizeBaseUrl(const char* source, bool development_mode, char* output,
                             size_t output_size) {
  const char* authority;
  size_t length;

  if ((source == NULL) || (output == NULL) || (output_size == 0u)) {
    return false;
  }
  if (strncmp(source, "https://", 8u) == 0) {
    authority = source + 8u;
  } else if (development_mode && (strncmp(source, "http://", 7u) == 0)) {
    authority = source + 7u;
  } else {
    return false;
  }
  if ((authority[0] == '\0') || (strchr(authority, '@') != NULL) || (authority[0] == '/') ||
      (strchr(authority, '?') != NULL) || (strchr(authority, '#') != NULL)) {
    return false;
  }
  for (const char* cursor = authority; *cursor != '\0'; ++cursor) {
    if (((unsigned char)*cursor <= 32u) || ((unsigned char)*cursor >= 127u) ||
        ((*cursor == '/') && (cursor[1] != '\0'))) {
      return false;
    }
  }
  length = strlen(source);
  if ((length > 0u) && (source[length - 1u] == '/')) {
    --length;
  }
  if ((length == 0u) || (length >= output_size)) {
    return false;
  }
  memcpy(output, source, length);
  output[length] = '\0';
  return true;
}

static void ClearSecrets(ShroomClientRest* rest) {
  OPENSSL_cleanse(rest->access_token, sizeof(rest->access_token));
  OPENSSL_cleanse(rest->refresh_token, sizeof(rest->refresh_token));
  rest->access_expires_at = 0u;
  rest->refresh_expires_at = 0u;
  rest->authenticated = false;
  memset(&rest->profile, 0, sizeof(rest->profile));
}

bool ShroomClientRestInit(ShroomClientRest* rest, const ShroomClientRestConfig* config) {
  char base_url[SHROOM_CLIENT_SESSION_BASE_URL_MAX];
  char session_path[SHROOM_CLIENT_SESSION_PATH_MAX];

  if ((rest == NULL) || (config == NULL) || (config->transport == NULL) ||
      !NormalizeBaseUrl(config->base_url, config->development_mode, base_url, sizeof(base_url))) {
    if (rest != NULL) {
      memset(rest, 0, sizeof(*rest));
    }
    return false;
  }
  if (config->session_path != NULL) {
    if (strlen(config->session_path) >= sizeof(session_path)) {
      memset(rest, 0, sizeof(*rest));
      return false;
    }
    snprintf(session_path, sizeof(session_path), "%s", config->session_path);
  } else if (!ShroomClientSessionDefaultPath(session_path, sizeof(session_path))) {
    memset(rest, 0, sizeof(*rest));
    return false;
  }
  memset(rest, 0, sizeof(*rest));
  snprintf(rest->base_url, sizeof(rest->base_url), "%s", base_url);
  snprintf(rest->session_path, sizeof(rest->session_path), "%s", session_path);
  rest->development_mode = config->development_mode;
  rest->transport = config->transport;
  rest->transport_context = config->transport_context;
  rest->now = config->now != NULL ? config->now : DefaultNow;
  rest->now_context = config->now_context;
  return true;
}

void ShroomClientRestClear(ShroomClientRest* rest) {
  if (rest == NULL) {
    return;
  }
  ClearSecrets(rest);
  SetError(rest, "", "");
}

bool ShroomClientRestHasStoredSession(const ShroomClientRest* rest) {
  ShroomClientStoredSession session;
  bool found = false;

  if (rest == NULL) {
    return false;
  }
  if (ShroomClientSessionLoad(rest->session_path, &session)) {
    found = strcmp(session.base_url, rest->base_url) == 0;
    OPENSSL_cleanse(&session, sizeof(session));
  }
  if (!found) {
    (void)ShroomClientSessionDelete(rest->session_path);
  }
  return found;
}

static bool RequiredJsonString(const cJSON* object, const char* name, char* output,
                               size_t output_size) {
  const cJSON* value = cJSON_GetObjectItemCaseSensitive(object, name);
  size_t length;

  if (!cJSON_IsString(value) || (value->valuestring == NULL)) {
    return false;
  }
  length = strlen(value->valuestring);
  if ((length == 0u) || (length >= output_size)) {
    return false;
  }
  memcpy(output, value->valuestring, length + 1u);
  return true;
}

static ShroomClientRestResult ParseError(ShroomClientRest* rest, long status, const char* body) {
  cJSON* root = cJSON_Parse(body != NULL ? body : "");
  const cJSON* error = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "error") : NULL;
  const cJSON* code = error != NULL ? cJSON_GetObjectItemCaseSensitive(error, "code") : NULL;
  const cJSON* message = error != NULL ? cJSON_GetObjectItemCaseSensitive(error, "message") : NULL;

  SetError(rest, cJSON_IsString(code) ? code->valuestring : "protocol_error",
           cJSON_IsString(message) ? message->valuestring
                                   : "The server returned an invalid response");
  cJSON_Delete(root);
  if (status == 401) {
    return SHROOM_CLIENT_REST_UNAUTHORIZED;
  }
  return status >= 500 ? SHROOM_CLIENT_REST_SERVER_ERROR : SHROOM_CLIENT_REST_PROTOCOL_ERROR;
}

static ShroomClientRestResult Request(ShroomClientRest* rest, const char* method, const char* path,
                                      const char* authorization, const char* idempotency_key,
                                      const char* json_body, long expected_status,
                                      char* response_body, size_t response_size) {
  char url[SHROOM_CLIENT_SESSION_BASE_URL_MAX + 96u];
  ShroomClientHttpResponse response = {.body = response_body, .body_capacity = response_size};
  ShroomClientHttpRequest request;

  if ((rest == NULL) || (method == NULL) || (path == NULL) || (response_body == NULL) ||
      (response_size == 0u) ||
      (snprintf(url, sizeof(url), "%s%s", rest->base_url, path) >= (int)sizeof(url))) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  response_body[0] = '\0';
  request = (ShroomClientHttpRequest){.method = method,
                                      .url = url,
                                      .authorization = authorization,
                                      .idempotency_key = idempotency_key,
                                      .json_body = json_body};
  SetError(rest, "", "");
  if (!rest->transport(rest->transport_context, &request, &response)) {
    SetError(rest, "transport_error",
             response.transport_error[0] != '\0' ? response.transport_error
                                                 : "Unable to reach the account service");
    return SHROOM_CLIENT_REST_TRANSPORT_ERROR;
  }
  if (response.status != expected_status) {
    return ParseError(rest, response.status, response_body);
  }
  return SHROOM_CLIENT_REST_OK;
}

static bool ParseTokenPair(const cJSON* json, char* access_token, size_t access_size,
                           uint64_t* access_lifetime, char* refresh_token, size_t refresh_size,
                           uint64_t* refresh_lifetime) {
  const cJSON* access_expiry = cJSON_GetObjectItemCaseSensitive(json, "expires_in");
  const cJSON* refresh_expiry = cJSON_GetObjectItemCaseSensitive(json, "refresh_expires_in");

  const double max_lifetime = (double)(365u * 24u * 60u * 60u);

  return RequiredJsonString(json, "access_token", access_token, access_size) &&
         RequiredJsonString(json, "refresh_token", refresh_token, refresh_size) &&
         cJSON_IsNumber(access_expiry) && (access_expiry->valuedouble > 0.0) &&
         (access_expiry->valuedouble <= max_lifetime) &&
         (floor(access_expiry->valuedouble) == access_expiry->valuedouble) &&
         cJSON_IsNumber(refresh_expiry) && (refresh_expiry->valuedouble > 0.0) &&
         (refresh_expiry->valuedouble <= max_lifetime) &&
         (floor(refresh_expiry->valuedouble) == refresh_expiry->valuedouble) &&
         ((*access_lifetime = (uint64_t)access_expiry->valuedouble) > 0u) &&
         ((*refresh_lifetime = (uint64_t)refresh_expiry->valuedouble) > 0u);
}

static bool PersistTokens(ShroomClientRest* rest, const char* access, uint64_t access_lifetime,
                          const char* refresh, uint64_t refresh_lifetime) {
  const uint64_t now = rest->now(rest->now_context);
  ShroomClientStoredSession stored = {0};

  snprintf(stored.base_url, sizeof(stored.base_url), "%s", rest->base_url);
  snprintf(stored.refresh_token, sizeof(stored.refresh_token), "%s", refresh);
  stored.refresh_expires_at = AddLifetime(now, refresh_lifetime);
  if (!ShroomClientSessionSave(rest->session_path, &stored)) {
    ClearSecrets(rest);
    SetError(rest, "storage_error", "The session could not be stored securely");
    return false;
  }
  snprintf(rest->access_token, sizeof(rest->access_token), "%s", access);
  snprintf(rest->refresh_token, sizeof(rest->refresh_token), "%s", refresh);
  rest->access_expires_at = AddLifetime(now, access_lifetime);
  rest->refresh_expires_at = stored.refresh_expires_at;
  rest->authenticated = true;
  return true;
}

static cJSON* CreateCredentialsJson(const char* identity_name, const char* identity,
                                    const char* password) {
  cJSON* json = cJSON_CreateObject();

  if ((json == NULL) || !cJSON_AddStringToObject(json, identity_name, identity) ||
      !cJSON_AddStringToObject(json, "password", password)) {
    cJSON_Delete(json);
    return NULL;
  }
  return json;
}

static ShroomClientRestResult ParseAndPersistTokens(ShroomClientRest* rest, const char* body,
                                                    bool registration) {
  cJSON* root = cJSON_Parse(body);
  const cJSON* token_json =
      registration && (root != NULL) ? cJSON_GetObjectItemCaseSensitive(root, "session") : root;
  char access[SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX] = {0};
  char refresh[SHROOM_CLIENT_SESSION_REFRESH_TOKEN_MAX] = {0};
  uint64_t access_lifetime = 0u;
  uint64_t refresh_lifetime = 0u;
  ShroomClientRestResult result = SHROOM_CLIENT_REST_PROTOCOL_ERROR;

  if ((token_json != NULL) && ParseTokenPair(token_json, access, sizeof(access), &access_lifetime,
                                             refresh, sizeof(refresh), &refresh_lifetime)) {
    result = PersistTokens(rest, access, access_lifetime, refresh, refresh_lifetime)
                 ? SHROOM_CLIENT_REST_OK
                 : SHROOM_CLIENT_REST_STORAGE_ERROR;
  } else {
    SetError(rest, "protocol_error", "The server returned an invalid session");
  }
  CleanseJsonString((cJSON*)token_json, "access_token");
  CleanseJsonString((cJSON*)token_json, "refresh_token");
  OPENSSL_cleanse(access, sizeof(access));
  OPENSSL_cleanse(refresh, sizeof(refresh));
  cJSON_Delete(root);
  return result;
}

ShroomClientRestResult ShroomClientRestRegister(ShroomClientRest* rest, const char* username,
                                                const char* email, const char* password) {
  cJSON* json;
  char* body;
  char response[SHROOM_CLIENT_REST_RESPONSE_MAX + 1u];
  ShroomClientRestResult result;

  if ((rest == NULL) || (username == NULL) || (email == NULL) || (password == NULL)) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  json = CreateCredentialsJson("username", username, password);
  if ((json == NULL) || !cJSON_AddStringToObject(json, "email", email) ||
      ((body = cJSON_PrintUnformatted(json)) == NULL)) {
    CleanseJsonString(json, "password");
    cJSON_Delete(json);
    SetError(rest, "internal_error", "The registration request could not be created");
    return SHROOM_CLIENT_REST_PROTOCOL_ERROR;
  }
  result = Request(rest, "POST", "/v1/account/register", NULL, NULL, body, 201, response,
                   sizeof(response));
  OPENSSL_cleanse(body, strlen(body));
  cJSON_free(body);
  CleanseJsonString(json, "password");
  cJSON_Delete(json);
  if (result == SHROOM_CLIENT_REST_OK) {
    result = ParseAndPersistTokens(rest, response, true);
  }
  OPENSSL_cleanse(response, sizeof(response));
  return result;
}

ShroomClientRestResult ShroomClientRestLogin(ShroomClientRest* rest, const char* identity,
                                             const char* password) {
  cJSON* json;
  char* body;
  char response[SHROOM_CLIENT_REST_RESPONSE_MAX + 1u];
  ShroomClientRestResult result;

  if ((rest == NULL) || (identity == NULL) || (password == NULL)) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  json = CreateCredentialsJson("identity", identity, password);
  if ((json == NULL) || ((body = cJSON_PrintUnformatted(json)) == NULL)) {
    CleanseJsonString(json, "password");
    cJSON_Delete(json);
    SetError(rest, "internal_error", "The login request could not be created");
    return SHROOM_CLIENT_REST_PROTOCOL_ERROR;
  }
  result =
      Request(rest, "POST", "/v1/account/login", NULL, NULL, body, 200, response, sizeof(response));
  OPENSSL_cleanse(body, strlen(body));
  cJSON_free(body);
  CleanseJsonString(json, "password");
  cJSON_Delete(json);
  if (result == SHROOM_CLIENT_REST_OK) {
    result = ParseAndPersistTokens(rest, response, false);
  }
  OPENSSL_cleanse(response, sizeof(response));
  return result;
}

ShroomClientRestResult ShroomClientRestRefresh(ShroomClientRest* rest) {
  cJSON* json;
  char* body;
  char response[SHROOM_CLIENT_REST_RESPONSE_MAX + 1u];
  ShroomClientRestResult result;

  if ((rest == NULL) || (rest->refresh_token[0] == '\0')) {
    return SHROOM_CLIENT_REST_UNAUTHORIZED;
  }
  json = cJSON_CreateObject();
  if ((json == NULL) || !cJSON_AddStringToObject(json, "refresh_token", rest->refresh_token) ||
      ((body = cJSON_PrintUnformatted(json)) == NULL)) {
    CleanseJsonString(json, "refresh_token");
    cJSON_Delete(json);
    return SHROOM_CLIENT_REST_PROTOCOL_ERROR;
  }
  result = Request(rest, "POST", "/v1/account/refresh", NULL, NULL, body, 200, response,
                   sizeof(response));
  OPENSSL_cleanse(body, strlen(body));
  cJSON_free(body);
  CleanseJsonString(json, "refresh_token");
  cJSON_Delete(json);
  if (result == SHROOM_CLIENT_REST_UNAUTHORIZED) {
    (void)ShroomClientSessionDelete(rest->session_path);
    ClearSecrets(rest);
    OPENSSL_cleanse(response, sizeof(response));
    return result;
  }
  if (result == SHROOM_CLIENT_REST_OK) {
    result = ParseAndPersistTokens(rest, response, false);
  }
  OPENSSL_cleanse(response, sizeof(response));
  return result;
}

ShroomClientRestResult ShroomClientRestRestore(ShroomClientRest* rest) {
  ShroomClientStoredSession stored = {0};
  const uint64_t now = rest != NULL ? rest->now(rest->now_context) : 0u;

  if ((rest == NULL) || !ShroomClientSessionLoad(rest->session_path, &stored) ||
      (strcmp(stored.base_url, rest->base_url) != 0) || (stored.refresh_expires_at <= now)) {
    if (rest != NULL) {
      (void)ShroomClientSessionDelete(rest->session_path);
      ClearSecrets(rest);
    }
    OPENSSL_cleanse(&stored, sizeof(stored));
    return SHROOM_CLIENT_REST_UNAUTHORIZED;
  }
  snprintf(rest->refresh_token, sizeof(rest->refresh_token), "%s", stored.refresh_token);
  rest->refresh_expires_at = stored.refresh_expires_at;
  OPENSSL_cleanse(&stored, sizeof(stored));
  return ShroomClientRestRefresh(rest);
}

static ShroomClientRestResult EnsureAccess(ShroomClientRest* rest) {
  const uint64_t now = rest->now(rest->now_context);

  if ((rest->access_token[0] != '\0') && (rest->access_expires_at > now) &&
      ((rest->access_expires_at - now) > ACCESS_REFRESH_SKEW_SECONDS)) {
    return SHROOM_CLIENT_REST_OK;
  }
  return ShroomClientRestRefresh(rest);
}

ShroomClientRestResult ShroomClientRestGetMe(ShroomClientRest* rest) {
  char authorization[SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX + 8u];
  char response[SHROOM_CLIENT_REST_RESPONSE_MAX + 1u];
  ShroomClientRestResult result;
  cJSON* root;
  ShroomClientAccountProfile profile = {0};

  if (rest == NULL) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  result = EnsureAccess(rest);
  if (result != SHROOM_CLIENT_REST_OK) {
    return result;
  }
  snprintf(authorization, sizeof(authorization), "Bearer %s", rest->access_token);
  result = Request(rest, "GET", "/v1/account/me", authorization, NULL, NULL, 200, response,
                   sizeof(response));
  OPENSSL_cleanse(authorization, sizeof(authorization));
  if (result != SHROOM_CLIENT_REST_OK) {
    OPENSSL_cleanse(response, sizeof(response));
    return result;
  }
  root = cJSON_Parse(response);
  if ((root == NULL) ||
      !RequiredJsonString(root, "player_id", profile.player_id, sizeof(profile.player_id)) ||
      !RequiredJsonString(root, "username", profile.username, sizeof(profile.username)) ||
      !RequiredJsonString(root, "email", profile.email, sizeof(profile.email)) ||
      !RequiredJsonString(root, "created_at", profile.created_at, sizeof(profile.created_at))) {
    cJSON_Delete(root);
    SetError(rest, "protocol_error", "The server returned an invalid account profile");
    OPENSSL_cleanse(response, sizeof(response));
    return SHROOM_CLIENT_REST_PROTOCOL_ERROR;
  }
  cJSON_Delete(root);
  OPENSSL_cleanse(response, sizeof(response));
  rest->profile = profile;
  rest->authenticated = true;
  return SHROOM_CLIENT_REST_OK;
}

ShroomClientRestResult ShroomClientRestLogout(ShroomClientRest* rest) {
  char authorization[SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX + 8u];
  char response[2];
  ShroomClientRestResult result;

  if (rest == NULL) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  result = EnsureAccess(rest);
  if (result != SHROOM_CLIENT_REST_OK) {
    (void)ShroomClientSessionDelete(rest->session_path);
    ClearSecrets(rest);
    return result;
  }
  if (rest->access_token[0] == '\0') {
    return SHROOM_CLIENT_REST_UNAUTHORIZED;
  }
  snprintf(authorization, sizeof(authorization), "Bearer %s", rest->access_token);
  result = Request(rest, "POST", "/v1/account/logout", authorization, NULL, NULL, 204, response,
                   sizeof(response));
  OPENSSL_cleanse(authorization, sizeof(authorization));
  OPENSSL_cleanse(response, sizeof(response));
  (void)ShroomClientSessionDelete(rest->session_path);
  ClearSecrets(rest);
  return result;
}

static bool GenerateUuid(char output[37]) {
  unsigned char bytes[16] = {0};

  if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
    OPENSSL_cleanse(bytes, sizeof(bytes));
    return false;
  }
  bytes[6] = (unsigned char)((bytes[6] & 0x0fu) | 0x40u);
  bytes[8] = (unsigned char)((bytes[8] & 0x3fu) | 0x80u);
  snprintf(output, 37u, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8],
           bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
  OPENSSL_cleanse(bytes, sizeof(bytes));
  return true;
}

ShroomClientRestResult ShroomClientRestBeginCheckout(ShroomClientRest* rest, const char* sku,
                                                     int quantity, const char* success_url,
                                                     const char* cancel_url,
                                                     ShroomClientCheckout* checkout) {
  char authorization[SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX + 8u];
  char idempotency_key[37];
  char response[SHROOM_CLIENT_REST_RESPONSE_MAX + 1u];
  cJSON* json;
  char* body;
  cJSON* root;
  ShroomClientCheckout parsed = {0};
  ShroomClientRestResult result;

  if ((rest == NULL) || (sku == NULL) || (success_url == NULL) || (cancel_url == NULL) ||
      (checkout == NULL) || (quantity < 1) || (quantity > 10)) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  if (!GenerateUuid(idempotency_key)) {
    return SHROOM_CLIENT_REST_INVALID_ARGUMENT;
  }
  result = EnsureAccess(rest);
  if (result != SHROOM_CLIENT_REST_OK) {
    OPENSSL_cleanse(idempotency_key, sizeof(idempotency_key));
    return result;
  }
  json = cJSON_CreateObject();
  if ((json == NULL) || !cJSON_AddStringToObject(json, "sku", sku) ||
      !cJSON_AddNumberToObject(json, "quantity", quantity) ||
      !cJSON_AddStringToObject(json, "success_url", success_url) ||
      !cJSON_AddStringToObject(json, "cancel_url", cancel_url) ||
      ((body = cJSON_PrintUnformatted(json)) == NULL)) {
    OPENSSL_cleanse(idempotency_key, sizeof(idempotency_key));
    cJSON_Delete(json);
    return SHROOM_CLIENT_REST_PROTOCOL_ERROR;
  }
  snprintf(authorization, sizeof(authorization), "Bearer %s", rest->access_token);
  result = Request(rest, "POST", "/v1/billing/checkout", authorization, idempotency_key, body, 201,
                   response, sizeof(response));
  OPENSSL_cleanse(authorization, sizeof(authorization));
  OPENSSL_cleanse(idempotency_key, sizeof(idempotency_key));
  cJSON_free(body);
  cJSON_Delete(json);
  if (result != SHROOM_CLIENT_REST_OK) {
    OPENSSL_cleanse(response, sizeof(response));
    return result;
  }
  root = cJSON_Parse(response);
  if ((root == NULL) ||
      !RequiredJsonString(root, "checkout_id", parsed.checkout_id, sizeof(parsed.checkout_id)) ||
      !RequiredJsonString(root, "checkout_url", parsed.checkout_url, sizeof(parsed.checkout_url)) ||
      !RequiredJsonString(root, "expires_at", parsed.expires_at, sizeof(parsed.expires_at)) ||
      (strncmp(parsed.checkout_url, "https://", 8u) != 0)) {
    cJSON_Delete(root);
    SetError(rest, "protocol_error", "The server returned an invalid checkout URL");
    OPENSSL_cleanse(response, sizeof(response));
    return SHROOM_CLIENT_REST_PROTOCOL_ERROR;
  }
  cJSON_Delete(root);
  OPENSSL_cleanse(response, sizeof(response));
  *checkout = parsed;
  return SHROOM_CLIENT_REST_OK;
}
