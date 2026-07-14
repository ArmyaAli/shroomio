#include "rest_account.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <civetweb.h>
#include <cjson/cJSON.h>

#include "account_auth.h"
#include "rest_rate_limit.h"

#define REST_BODY_MAX_BYTES 65536u

static const char* ReasonPhrase(int status) {
  switch (status) {
  case 200:
    return "OK";
  case 201:
    return "Created";
  case 204:
    return "No Content";
  case 400:
    return "Bad Request";
  case 401:
    return "Unauthorized";
  case 409:
    return "Conflict";
  case 413:
    return "Payload Too Large";
  case 429:
    return "Too Many Requests";
  default:
    return "Internal Server Error";
  }
}

static int SendResponse(struct mg_connection* connection, int status, const char* request_id,
                        const char* body, const ShroomRestRateLimitResult* rate) {
  const size_t body_length = body != NULL ? strlen(body) : 0u;

  mg_printf(connection,
            "HTTP/1.1 %d %s\r\n"
            "Content-Length: %llu\r\n"
            "Cache-Control: no-store\r\n"
            "X-Content-Type-Options: nosniff\r\n"
            "X-Request-ID: %s\r\n",
            status, ReasonPhrase(status), (unsigned long long)body_length, request_id);
  if (body_length > 0u) {
    mg_printf(connection, "Content-Type: application/json; charset=utf-8\r\n");
  }
  if (rate != NULL) {
    mg_printf(connection,
              "X-RateLimit-Limit: %u\r\n"
              "X-RateLimit-Remaining: %u\r\n"
              "X-RateLimit-Reset: %llu\r\n",
              rate->limit, rate->remaining, (unsigned long long)rate->reset_at);
    if (!rate->allowed) {
      mg_printf(connection, "Retry-After: %u\r\n", rate->retry_after);
    }
  }
  mg_printf(connection, "Connection: close\r\n\r\n");
  if (body_length > 0u) {
    mg_write(connection, body, body_length);
  }
  return status;
}

static int SendError(struct mg_connection* connection, int status, const char* code,
                     const char* message, const char* request_id,
                     const ShroomRestRateLimitResult* rate) {
  cJSON* root = cJSON_CreateObject();
  cJSON* error = cJSON_AddObjectToObject(root, "error");
  char* body;
  int result;

  cJSON_AddStringToObject(error, "code", code);
  cJSON_AddStringToObject(error, "message", message);
  cJSON_AddStringToObject(error, "request_id", request_id);
  body = cJSON_PrintUnformatted(root);
  result = body != NULL ? SendResponse(connection, status, request_id, body, rate)
                        : SendResponse(connection, 500, request_id, NULL, rate);
  cJSON_free(body);
  cJSON_Delete(root);
  return result;
}

static cJSON* ReadJsonBody(struct mg_connection* connection, int* error_status) {
  const struct mg_request_info* request = mg_get_request_info(connection);
  const char* content_type = mg_get_header(connection, "Content-Type");
  char* body;
  size_t received = 0u;

  *error_status = 400;
  if ((request == NULL) || (request->content_length < 0) ||
      ((unsigned long long)request->content_length > REST_BODY_MAX_BYTES)) {
    if ((request != NULL) && ((unsigned long long)request->content_length > REST_BODY_MAX_BYTES)) {
      *error_status = 413;
    }
    return NULL;
  }
  if ((content_type == NULL) || (strncmp(content_type, "application/json", 16u) != 0)) {
    return NULL;
  }
  body = malloc((size_t)request->content_length + 1u);
  if (body == NULL) {
    *error_status = 500;
    return NULL;
  }
  while (received < (size_t)request->content_length) {
    const int count =
        mg_read(connection, body + received, (size_t)request->content_length - received);
    if (count <= 0) {
      free(body);
      return NULL;
    }
    received += (size_t)count;
  }
  body[received] = '\0';
  cJSON* json = cJSON_ParseWithLength(body, received);
  free(body);
  return json;
}

static const char* RequiredString(const cJSON* json, const char* name) {
  const cJSON* value = cJSON_GetObjectItemCaseSensitive(json, name);
  return cJSON_IsString(value) && (value->valuestring != NULL) ? value->valuestring : NULL;
}

static const char* BearerToken(struct mg_connection* connection) {
  const char* authorization = mg_get_header(connection, "Authorization");
  const char* token;

  if ((authorization == NULL) || (strncmp(authorization, "Bearer ", 7u) != 0)) {
    return NULL;
  }
  token = authorization + 7u;
  return (strlen(token) == SHROOM_ACCOUNT_TOKEN_LENGTH) ? token : NULL;
}

static ShroomRestRateLimitResult CheckRate(struct mg_connection* connection,
                                           ShroomRestRateLimiter* limiter,
                                           ShroomRestRateRoute route, uint32_t user_id) {
  const struct mg_request_info* request = mg_get_request_info(connection);
  char key[64];

  if (user_id != 0u) {
    snprintf(key, sizeof(key), "user:%u", user_id);
  } else {
    snprintf(key, sizeof(key), "ip:%s",
             (request != NULL) && (request->remote_addr[0] != '\0') ? request->remote_addr
                                                                    : "unknown");
  }
  return ShroomRestRateLimitCheck(limiter, key, route, (uint64_t)time(NULL));
}

static int SendRateLimited(struct mg_connection* connection, const char* request_id,
                           const ShroomRestRateLimitResult* rate) {
  return SendError(connection, 429, "rate_limited", "Too many requests; try again later",
                   request_id, rate);
}

static int SendAccountResultError(struct mg_connection* connection, ShroomAccountResult result,
                                  const char* request_id, const ShroomRestRateLimitResult* rate) {
  switch (result) {
  case SHROOM_ACCOUNT_INVALID_INPUT:
    return SendError(connection, 400, "invalid_request", "Request fields are invalid", request_id,
                     rate);
  case SHROOM_ACCOUNT_INVALID_CREDENTIALS:
    return SendError(connection, 401, "invalid_credentials", "Identity or password is incorrect",
                     request_id, rate);
  case SHROOM_ACCOUNT_INVALID_TOKEN:
    return SendError(connection, 401, "invalid_token", "Session token is invalid", request_id,
                     rate);
  case SHROOM_ACCOUNT_USERNAME_TAKEN:
    return SendError(connection, 409, "username_taken", "Username is already in use", request_id,
                     rate);
  case SHROOM_ACCOUNT_EMAIL_TAKEN:
    return SendError(connection, 409, "email_taken", "Email is already in use", request_id, rate);
  case SHROOM_ACCOUNT_DATABASE_ERROR:
  case SHROOM_ACCOUNT_CRYPTO_ERROR:
  default:
    return SendError(connection, 500, "internal_error", "The request could not be completed",
                     request_id, rate);
  }
}

static cJSON* TokenPairJson(const ShroomAccountTokenPair* tokens) {
  cJSON* json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "access_token", tokens->access_token);
  cJSON_AddStringToObject(json, "token_type", "Bearer");
  cJSON_AddNumberToObject(json, "expires_in", tokens->access_expires_in);
  cJSON_AddStringToObject(json, "refresh_token", tokens->refresh_token);
  cJSON_AddNumberToObject(json, "refresh_expires_in", tokens->refresh_expires_in);
  return json;
}

static int SendJsonObject(struct mg_connection* connection, int status, const char* request_id,
                          cJSON* json, const ShroomRestRateLimitResult* rate) {
  char* body = cJSON_PrintUnformatted(json);
  const int result = body != NULL
                         ? SendResponse(connection, status, request_id, body, rate)
                         : SendError(connection, 500, "internal_error",
                                     "The response could not be created", request_id, rate);
  cJSON_free(body);
  cJSON_Delete(json);
  return result;
}

static int HandleRegister(struct mg_connection* connection, const char* request_id,
                          ShroomAccountAuth* auth, ShroomRestRateLimiter* limiter) {
  ShroomRestRateLimitResult rate = CheckRate(connection, limiter, SHROOM_REST_RATE_REGISTER, 0u);
  ShroomAccount account = {0};
  ShroomAccountTokenPair tokens = {0};
  int body_error;
  cJSON* request;
  ShroomAccountResult result;
  cJSON* response;
  cJSON* account_json;

  if (!rate.allowed) {
    return SendRateLimited(connection, request_id, &rate);
  }
  request = ReadJsonBody(connection, &body_error);
  if (request == NULL) {
    return SendError(connection, body_error,
                     body_error == 413 ? "payload_too_large" : "invalid_request",
                     body_error == 413 ? "Request body exceeds 64 KiB" : "A JSON body is required",
                     request_id, &rate);
  }
  result = ShroomAccountRegister(auth, RequiredString(request, "username"),
                                 RequiredString(request, "email"),
                                 RequiredString(request, "password"), &account, &tokens);
  cJSON_Delete(request);
  if (result != SHROOM_ACCOUNT_SUCCESS) {
    return SendAccountResultError(connection, result, request_id, &rate);
  }
  response = cJSON_CreateObject();
  account_json = cJSON_AddObjectToObject(response, "account");
  cJSON_AddStringToObject(account_json, "player_id", account.player_id);
  cJSON_AddStringToObject(account_json, "username", account.username);
  cJSON_AddStringToObject(account_json, "email", account.email);
  cJSON_AddStringToObject(account_json, "created_at", account.created_at);
  cJSON_AddItemToObject(response, "session", TokenPairJson(&tokens));
  return SendJsonObject(connection, 201, request_id, response, &rate);
}

static int HandleLogin(struct mg_connection* connection, const char* request_id,
                       ShroomAccountAuth* auth, ShroomRestRateLimiter* limiter) {
  ShroomRestRateLimitResult rate = CheckRate(connection, limiter, SHROOM_REST_RATE_LOGIN, 0u);
  ShroomAccountTokenPair tokens = {0};
  int body_error;
  cJSON* request;
  ShroomAccountResult result;

  if (!rate.allowed) {
    return SendRateLimited(connection, request_id, &rate);
  }
  request = ReadJsonBody(connection, &body_error);
  if (request == NULL) {
    return SendError(connection, body_error,
                     body_error == 413 ? "payload_too_large" : "invalid_request",
                     body_error == 413 ? "Request body exceeds 64 KiB" : "A JSON body is required",
                     request_id, &rate);
  }
  result = ShroomAccountLogin(auth, RequiredString(request, "identity"),
                              RequiredString(request, "password"), &tokens);
  cJSON_Delete(request);
  if (result != SHROOM_ACCOUNT_SUCCESS) {
    return SendAccountResultError(connection, result, request_id, &rate);
  }
  return SendJsonObject(connection, 200, request_id, TokenPairJson(&tokens), &rate);
}

static int HandleRefresh(struct mg_connection* connection, const char* request_id,
                         ShroomAccountAuth* auth, ShroomRestRateLimiter* limiter) {
  ShroomRestRateLimitResult rate = CheckRate(connection, limiter, SHROOM_REST_RATE_REFRESH, 0u);
  ShroomAccountTokenPair tokens = {0};
  int body_error;
  cJSON* request;
  ShroomAccountResult result;

  if (!rate.allowed) {
    return SendRateLimited(connection, request_id, &rate);
  }
  request = ReadJsonBody(connection, &body_error);
  if (request == NULL) {
    return SendError(connection, body_error,
                     body_error == 413 ? "payload_too_large" : "invalid_request",
                     body_error == 413 ? "Request body exceeds 64 KiB" : "A JSON body is required",
                     request_id, &rate);
  }
  result = ShroomAccountRefresh(auth, RequiredString(request, "refresh_token"), &tokens);
  cJSON_Delete(request);
  if (result != SHROOM_ACCOUNT_SUCCESS) {
    return SendAccountResultError(connection, result, request_id, &rate);
  }
  return SendJsonObject(connection, 200, request_id, TokenPairJson(&tokens), &rate);
}

static int HandleAuthenticated(struct mg_connection* connection, ShroomRestRoute route,
                               const char* request_id, ShroomAccountAuth* auth,
                               ShroomRestRateLimiter* limiter) {
  const char* token = BearerToken(connection);
  ShroomRestRateLimitResult rate =
      CheckRate(connection, limiter, SHROOM_REST_RATE_AUTHENTICATED, 0u);
  ShroomAccount account = {0};
  ShroomAccountResult result;
  uint32_t user_id = 0u;

  if (!rate.allowed) {
    return SendRateLimited(connection, request_id, &rate);
  }
  if (route == SHROOM_REST_ROUTE_ACCOUNT_LOGOUT) {
    result = ShroomAccountIdentifyAccess(auth, token, &user_id);
  } else {
    result = ShroomAccountGetMe(auth, token, &account);
    user_id = account.user_id;
  }
  if (result != SHROOM_ACCOUNT_SUCCESS) {
    return SendAccountResultError(connection, result, request_id, &rate);
  }
  rate = CheckRate(connection, limiter, SHROOM_REST_RATE_AUTHENTICATED, user_id);
  if (!rate.allowed) {
    return SendRateLimited(connection, request_id, &rate);
  }
  if (route == SHROOM_REST_ROUTE_ACCOUNT_LOGOUT) {
    result = ShroomAccountLogout(auth, token);
    return result == SHROOM_ACCOUNT_SUCCESS
               ? SendResponse(connection, 204, request_id, NULL, &rate)
               : SendAccountResultError(connection, result, request_id, &rate);
  }

  cJSON* response = cJSON_CreateObject();
  cJSON_AddStringToObject(response, "player_id", account.player_id);
  cJSON_AddStringToObject(response, "username", account.username);
  cJSON_AddStringToObject(response, "email", account.email);
  cJSON_AddStringToObject(response, "created_at", account.created_at);
  cJSON_AddItemToObject(response, "unlocked_species", cJSON_CreateArray());
  return SendJsonObject(connection, 200, request_id, response, &rate);
}

int ShroomRestHandleAccount(struct mg_connection* connection, ShroomRestRoute route,
                            const char* request_id, ShroomAccountAuth* auth,
                            ShroomRestRateLimiter* limiter) {
  if ((auth == NULL) || (limiter == NULL)) {
    return SendError(connection, 500, "internal_error", "Account service is unavailable",
                     request_id, NULL);
  }
  switch (route) {
  case SHROOM_REST_ROUTE_ACCOUNT_REGISTER:
    return HandleRegister(connection, request_id, auth, limiter);
  case SHROOM_REST_ROUTE_ACCOUNT_LOGIN:
    return HandleLogin(connection, request_id, auth, limiter);
  case SHROOM_REST_ROUTE_ACCOUNT_REFRESH:
    return HandleRefresh(connection, request_id, auth, limiter);
  case SHROOM_REST_ROUTE_ACCOUNT_LOGOUT:
  case SHROOM_REST_ROUTE_ACCOUNT_ME:
    return HandleAuthenticated(connection, route, request_id, auth, limiter);
  default:
    return SendError(connection, 404, "not_found", "Route not found", request_id, NULL);
  }
}
