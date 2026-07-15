#include "client_rest_curl.h"

#include <stdio.h>
#include <string.h>

#include <curl/curl.h>
#include <openssl/crypto.h>

typedef struct ShroomCurlWriteContext {
  ShroomClientHttpResponse* response;
  bool overflow;
} ShroomCurlWriteContext;

// cppcheck-suppress constParameterCallback -- libcurl requires this exact callback signature.
static size_t WriteResponse(char* data, size_t size, size_t count, void* user_data) {
  ShroomCurlWriteContext* context = user_data;
  size_t bytes;
  ShroomClientHttpResponse* response = context->response;

  if ((size != 0u) && (count > SIZE_MAX / size)) {
    context->overflow = true;
    return 0u;
  }
  bytes = size * count;
  if ((bytes > response->body_capacity - 1u) ||
      (response->body_length > response->body_capacity - 1u - bytes)) {
    context->overflow = true;
    return 0u;
  }
  memcpy(response->body + response->body_length, data, bytes);
  response->body_length += bytes;
  response->body[response->body_length] = '\0';
  return bytes;
}

static bool AppendHeader(struct curl_slist** headers, const char* value) {
  struct curl_slist* appended = curl_slist_append(*headers, value);

  if (appended == NULL) {
    return false;
  }
  *headers = appended;
  return true;
}

bool ShroomClientRestCurlGlobalInit(void) {
  return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

void ShroomClientRestCurlGlobalCleanup(void) { curl_global_cleanup(); }

bool ShroomClientRestCurlInit(ShroomClientRestCurl* transport, const char* ca_certificate,
                              const char* pinned_public_key) {
  if (transport == NULL) {
    return false;
  }
  memset(transport, 0, sizeof(*transport));
  if (((ca_certificate != NULL) &&
       (snprintf(transport->ca_certificate, sizeof(transport->ca_certificate), "%s",
                 ca_certificate) >= (int)sizeof(transport->ca_certificate))) ||
      ((pinned_public_key != NULL) &&
       (snprintf(transport->pinned_public_key, sizeof(transport->pinned_public_key), "%s",
                 pinned_public_key) >= (int)sizeof(transport->pinned_public_key)))) {
    memset(transport, 0, sizeof(*transport));
    return false;
  }
  return true;
}

bool ShroomClientRestCurlPerform(void* user_context, const ShroomClientHttpRequest* request,
                                 ShroomClientHttpResponse* response) {
  ShroomClientRestCurl* context = user_context;
  CURL* curl;
  CURLcode result;
  struct curl_slist* headers = NULL;
  char authorization[SHROOM_CLIENT_REST_ACCESS_TOKEN_MAX + 24u] = {0};
  char idempotency[80] = {0};
  char curl_error[CURL_ERROR_SIZE] = {0};
  ShroomCurlWriteContext write_context = {.response = response};

  if ((context == NULL) || (request == NULL) || (request->method == NULL) ||
      (request->url == NULL) || (response == NULL) || (response->body == NULL) ||
      (response->body_capacity == 0u)) {
    return false;
  }
  response->status = 0;
  response->body_length = 0u;
  response->body[0] = '\0';
  response->transport_error[0] = '\0';
  curl = curl_easy_init();
  if (curl == NULL) {
    snprintf(response->transport_error, sizeof(response->transport_error),
             "Unable to initialize HTTPS");
    return false;
  }

  if (!AppendHeader(&headers, "Accept: application/json") ||
      ((request->json_body != NULL) && !AppendHeader(&headers, "Content-Type: application/json"))) {
    snprintf(response->transport_error, sizeof(response->transport_error),
             "Unable to prepare HTTPS headers");
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return false;
  }
  if (request->authorization != NULL) {
    snprintf(authorization, sizeof(authorization), "Authorization: %s", request->authorization);
    if (!AppendHeader(&headers, authorization)) {
      snprintf(response->transport_error, sizeof(response->transport_error),
               "Unable to prepare HTTPS headers");
      OPENSSL_cleanse(authorization, sizeof(authorization));
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return false;
    }
  }
  if (request->idempotency_key != NULL) {
    snprintf(idempotency, sizeof(idempotency), "Idempotency-Key: %s", request->idempotency_key);
    if (!AppendHeader(&headers, idempotency)) {
      snprintf(response->transport_error, sizeof(response->transport_error),
               "Unable to prepare HTTPS headers");
      OPENSSL_cleanse(authorization, sizeof(authorization));
      OPENSSL_cleanse(idempotency, sizeof(idempotency));
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return false;
    }
  }

  curl_easy_setopt(curl, CURLOPT_URL, request->url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteResponse);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_context);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  if (context->ca_certificate[0] != '\0') {
    curl_easy_setopt(curl, CURLOPT_CAINFO, context->ca_certificate);
  }
  if (context->pinned_public_key[0] != '\0') {
    curl_easy_setopt(curl, CURLOPT_PINNEDPUBLICKEY, context->pinned_public_key);
  }
  if (strcmp(request->method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                     request->json_body != NULL ? request->json_body : "");
  } else if (strcmp(request->method, "GET") != 0) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->method);
  }

  result = curl_easy_perform(curl);
  if (result == CURLE_OK) {
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);
  } else if (write_context.overflow) {
    snprintf(response->transport_error, sizeof(response->transport_error),
             "The account service response was too large");
  } else {
    snprintf(response->transport_error, sizeof(response->transport_error), "%.*s",
             (int)sizeof(response->transport_error) - 1,
             curl_error[0] != '\0' ? curl_error : "The HTTPS request failed");
  }
  OPENSSL_cleanse(authorization, sizeof(authorization));
  OPENSSL_cleanse(idempotency, sizeof(idempotency));
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return result == CURLE_OK;
}
