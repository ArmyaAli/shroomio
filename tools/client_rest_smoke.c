#include "client/client_rest.h"
#include "client/client_rest_curl.h"

#include <stdio.h>
#include <string.h>

static int Fail(const ShroomClientRest* rest, const char* step) {
  fprintf(stderr, "client REST smoke failed at %s: %s\n", step,
          (rest != NULL) && (rest->error_message[0] != '\0') ? rest->error_message : "no detail");
  return 1;
}

int main(int argc, char** argv) {
  ShroomClientRestCurl curl_transport;
  ShroomClientRest rest;
  ShroomClientRestConfig config;

  if (argc != 4) {
    fprintf(stderr, "usage: %s <base-url> <ca-certificate> <session-path>\n", argv[0]);
    return 2;
  }
  if (!ShroomClientRestCurlGlobalInit() ||
      !ShroomClientRestCurlInit(&curl_transport, argv[2], NULL)) {
    return Fail(NULL, "curl initialization");
  }
  config = (ShroomClientRestConfig){.base_url = argv[1],
                                    .session_path = argv[3],
                                    .transport = ShroomClientRestCurlPerform,
                                    .transport_context = &curl_transport};
  if (!ShroomClientRestInit(&rest, &config)) {
    ShroomClientRestCurlGlobalCleanup();
    return Fail(NULL, "REST initialization");
  }
  if (ShroomClientRestRegister(&rest, "client_smoke", "client-smoke@example.test",
                               "correct horse battery") != SHROOM_CLIENT_REST_OK) {
    ShroomClientRestCurlGlobalCleanup();
    return Fail(&rest, "registration");
  }
  ShroomClientRestClear(&rest);
  if (!ShroomClientRestInit(&rest, &config) ||
      (ShroomClientRestRestore(&rest) != SHROOM_CLIENT_REST_OK) ||
      (ShroomClientRestGetMe(&rest) != SHROOM_CLIENT_REST_OK)) {
    ShroomClientRestCurlGlobalCleanup();
    return Fail(&rest, "restart and refresh");
  }
  if ((strcmp(rest.profile.username, "client_smoke") != 0) ||
      (strcmp(rest.profile.email, "client-smoke@example.test") != 0)) {
    ShroomClientRestCurlGlobalCleanup();
    return Fail(&rest, "profile validation");
  }
  if (ShroomClientRestLogout(&rest) != SHROOM_CLIENT_REST_OK) {
    ShroomClientRestCurlGlobalCleanup();
    return Fail(&rest, "logout");
  }
  if (ShroomClientRestHasStoredSession(&rest)) {
    ShroomClientRestCurlGlobalCleanup();
    return Fail(&rest, "session deletion");
  }
  ShroomClientRestCurlGlobalCleanup();
  puts("Client REST HTTPS smoke passed.");
  return 0;
}
