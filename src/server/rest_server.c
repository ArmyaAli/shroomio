#include "rest_server.h"

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include <civetweb.h>

#include "logger.h"
#include "rest_router.h"

#define SHROOM_REST_REQUEST_ID_LENGTH 64u

static atomic_uint_fast64_t g_rest_request_counter = ATOMIC_VAR_INIT(1u);

static void SanitizeLogValue(const char* value, char* output, size_t output_size) {
  size_t index = 0u;

  if ((output == NULL) || (output_size == 0u)) {
    return;
  }
  if (value != NULL) {
    while ((value[index] != '\0') && ((index + 1u) < output_size)) {
      const unsigned char character = (unsigned char)value[index];
      output[index] = (isalnum(character) || (character == '.') || (character == ':') ||
                       (character == '/') || (character == '_') || (character == '-'))
                          ? (char)character
                          : '_';
      ++index;
    }
  }
  output[index] = '\0';
}

static bool IsValidRequestId(const char* value) {
  size_t length = 0u;

  if ((value == NULL) || (value[0] == '\0')) {
    return false;
  }
  while (value[length] != '\0') {
    const unsigned char character = (unsigned char)value[length];
    if ((length >= SHROOM_REST_REQUEST_ID_LENGTH) || !(isalnum(character) || (character == '-'))) {
      return false;
    }
    ++length;
  }
  return true;
}

static void GetRequestId(const struct mg_connection* connection, char* request_id,
                         size_t request_id_size) {
  const char* supplied = mg_get_header(connection, "X-Request-ID");

  if (IsValidRequestId(supplied)) {
    snprintf(request_id, request_id_size, "%s", supplied);
    return;
  }
  snprintf(request_id, request_id_size, "rest-%llu",
           (unsigned long long)atomic_fetch_add(&g_rest_request_counter, 1u));
}

static int SendJson(struct mg_connection* connection, int status, const char* reason,
                    const char* request_id, const char* body) {
  const size_t body_length = strlen(body);

  mg_printf(connection,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "Content-Length: %llu\r\n"
            "Cache-Control: no-store\r\n"
            "X-Content-Type-Options: nosniff\r\n"
            "X-Request-ID: %s\r\n"
            "Connection: close\r\n\r\n%s",
            status, reason, (unsigned long long)body_length, request_id, body);
  return status;
}

static int HandleRequest(struct mg_connection* connection) {
  const struct mg_request_info* request = mg_get_request_info(connection);
  const char* method = request != NULL ? request->request_method : NULL;
  const char* path = request != NULL ? request->local_uri : NULL;
  char request_id[SHROOM_REST_REQUEST_ID_LENGTH + 1u];
  char body[256];

  GetRequestId(connection, request_id, sizeof(request_id));
  switch (ShroomRestClassifyRoute(method, path)) {
  case SHROOM_REST_ROUTE_HEALTH:
    return SendJson(connection, 200, "OK", request_id,
                    "{\"status\":\"ok\",\"service\":\"shroomio-server\"}");
  case SHROOM_REST_ROUTE_NOT_FOUND:
  default:
    snprintf(body, sizeof(body),
             "{\"error\":{\"code\":\"not_found\",\"message\":\"Route not found\","
             "\"request_id\":\"%s\"}}",
             request_id);
    return SendJson(connection, 404, "Not Found", request_id, body);
  }
}

static void LogCompletedRequest(const struct mg_connection* connection, int status) {
  const struct mg_request_info* request = mg_get_request_info(connection);
  char method[24];
  char path[160];
  char remote[64];

  SanitizeLogValue(request != NULL ? request->request_method : NULL, method, sizeof(method));
  SanitizeLogValue(request != NULL ? request->local_uri : NULL, path, sizeof(path));
  SanitizeLogValue(request != NULL ? request->remote_addr : NULL, remote, sizeof(remote));
  LOG_INFO("rest_access method=%s path=%s status=%d remote=%s body=redacted", method, path, status,
           remote);
}

static int LogCivetWebMessage(const struct mg_connection* connection, const char* message) {
  char sanitized_message[256];

  (void)connection;
  SanitizeLogValue(message != NULL ? message : "unknown", sanitized_message,
                   sizeof(sanitized_message));
  LOG_WARN("rest_transport message=%s", sanitized_message);
  return 1;
}

static int SuppressCivetWebAccessLog(const struct mg_connection* connection, const char* message) {
  (void)connection;
  (void)message;
  return 1;
}

bool ShroomRestServerStart(ShroomRestServer* server, const ShroomRestConfig* config) {
  struct mg_callbacks callbacks = {0};
  char listening_port[SHROOM_REST_BIND_MAX_LENGTH + 16u];
  const char* options[17];

  if ((server == NULL) || (config == NULL) || (config->bind_host == NULL) ||
      (config->bind_host[0] == '\0') || (config->port == 0u)) {
    return false;
  }
  if ((config->certificate_path == NULL) || (config->certificate_path[0] == '\0')) {
    LOG_ERROR("REST TLS certificate is required; set --rest-cert or SHROOM_SERVER_REST_CERT");
    return false;
  }
  if (server->context != NULL) {
    LOG_ERROR("REST server is already running");
    return false;
  }
  if (snprintf(listening_port, sizeof(listening_port), "%s:%us", config->bind_host,
               (unsigned)config->port) >= (int)sizeof(listening_port)) {
    LOG_ERROR("REST bind address is too long");
    return false;
  }

  if ((mg_init_library(MG_FEATURES_TLS) & MG_FEATURES_TLS) == 0u) {
    LOG_ERROR("failed to initialize REST TLS library");
    return false;
  }
  server->library_initialized = true;
  callbacks.begin_request = HandleRequest;
  callbacks.end_request = LogCompletedRequest;
  callbacks.log_message = LogCivetWebMessage;
  callbacks.log_access = SuppressCivetWebAccessLog;

  options[0] = "listening_ports";
  options[1] = listening_port;
  options[2] = "ssl_certificate";
  options[3] = config->certificate_path;
  options[4] = "ssl_protocol_version";
  options[5] = "4";
  options[6] = "num_threads";
  options[7] = "2";
  options[8] = "enable_directory_listing";
  options[9] = "no";
  options[10] = "enable_keep_alive";
  options[11] = "yes";
  options[12] = "keep_alive_timeout_ms";
  options[13] = "500";
  options[14] = "request_timeout_ms";
  options[15] = "5000";
  options[16] = NULL;

  server->context = mg_start(&callbacks, server, options);
  if (server->context == NULL) {
    LOG_ERROR("failed to start REST HTTPS listener on %s using certificate %s", listening_port,
              config->certificate_path);
    mg_exit_library();
    server->library_initialized = false;
    return false;
  }
  LOG_INFO("REST HTTPS listener started on %s", listening_port);
  return true;
}

void ShroomRestServerStop(ShroomRestServer* server) {
  if (server == NULL) {
    return;
  }
  if (server->context != NULL) {
    mg_stop(server->context);
    server->context = NULL;
    LOG_INFO("REST HTTPS listener stopped");
  }
  if (server->library_initialized) {
    mg_exit_library();
    server->library_initialized = false;
  }
}
