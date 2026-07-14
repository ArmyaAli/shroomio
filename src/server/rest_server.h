#ifndef SHROOM_REST_SERVER_H
#define SHROOM_REST_SERVER_H

#include <stdbool.h>
#include <stdint.h>

struct mg_context;

#define SHROOM_REST_DEFAULT_PORT 7443u
#define SHROOM_REST_BIND_MAX_LENGTH 63u
#define SHROOM_REST_CERT_PATH_MAX_LENGTH 255u

typedef struct ShroomRestConfig {
  const char* bind_host;
  uint16_t port;
  const char* certificate_path;
} ShroomRestConfig;

typedef struct ShroomRestServer {
  struct mg_context* context;
  bool library_initialized;
} ShroomRestServer;

bool ShroomRestServerStart(ShroomRestServer* server, const ShroomRestConfig* config);
void ShroomRestServerStop(ShroomRestServer* server);

#endif
