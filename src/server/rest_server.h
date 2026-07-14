#ifndef SHROOM_REST_SERVER_H
#define SHROOM_REST_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#include "rest_rate_limit.h"

struct mg_context;
struct ShroomAccountAuth;

#define SHROOM_REST_DEFAULT_PORT 7443u
#define SHROOM_REST_BIND_MAX_LENGTH 63u
#define SHROOM_REST_CERT_PATH_MAX_LENGTH 255u

typedef struct ShroomRestConfig {
  const char* bind_host;
  uint16_t port;
  const char* certificate_path;
  struct ShroomAccountAuth* account_auth;
} ShroomRestConfig;

typedef struct ShroomRestServer {
  struct mg_context* context;
  struct ShroomAccountAuth* account_auth;
  ShroomRestRateLimiter rate_limiter;
  bool library_initialized;
} ShroomRestServer;

bool ShroomRestServerStart(ShroomRestServer* server, const ShroomRestConfig* config);
void ShroomRestServerStop(ShroomRestServer* server);

#endif
