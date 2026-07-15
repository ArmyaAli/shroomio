#ifndef SHROOM_CLIENT_REST_CURL_H
#define SHROOM_CLIENT_REST_CURL_H

#include <stdbool.h>

#include "client_rest.h"

typedef struct ShroomClientRestCurl {
  char ca_certificate[SHROOM_CLIENT_SESSION_PATH_MAX];
  char pinned_public_key[SHROOM_CLIENT_SESSION_PATH_MAX];
} ShroomClientRestCurl;

bool ShroomClientRestCurlGlobalInit(void);
void ShroomClientRestCurlGlobalCleanup(void);
bool ShroomClientRestCurlInit(ShroomClientRestCurl* transport, const char* ca_certificate,
                              const char* pinned_public_key);
bool ShroomClientRestCurlPerform(void* context, const ShroomClientHttpRequest* request,
                                 ShroomClientHttpResponse* response);

#endif
