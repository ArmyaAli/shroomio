#include "unity.h"

#include "client/client_rest.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct FakeTransport {
  int register_calls;
  int login_calls;
  int refresh_calls;
  int me_calls;
  int logout_calls;
  int checkout_calls;
  bool fail_transport;
  bool unauthorized;
  char last_authorization[160];
  char last_idempotency[80];
  char last_body[1024];
} FakeTransport;

static FakeTransport fake;
static char session_path[256];
static uint64_t now_seconds;

static uint64_t FakeNow(void* context) {
  (void)context;
  return now_seconds;
}

static void Respond(ShroomClientHttpResponse* response, long status, const char* body) {
  response->status = status;
  response->body_length = strlen(body);
  TEST_ASSERT_LESS_THAN(response->body_capacity, response->body_length);
  memcpy(response->body, body, response->body_length + 1u);
}

static bool FakePerform(void* context, const ShroomClientHttpRequest* request,
                        ShroomClientHttpResponse* response) {
  FakeTransport* transport = context;

  snprintf(transport->last_authorization, sizeof(transport->last_authorization), "%s",
           request->authorization != NULL ? request->authorization : "");
  snprintf(transport->last_idempotency, sizeof(transport->last_idempotency), "%s",
           request->idempotency_key != NULL ? request->idempotency_key : "");
  snprintf(transport->last_body, sizeof(transport->last_body), "%s",
           request->json_body != NULL ? request->json_body : "");
  if (transport->fail_transport) {
    snprintf(response->transport_error, sizeof(response->transport_error), "offline");
    return false;
  }
  if (transport->unauthorized) {
    Respond(response, 401, "{\"error\":{\"code\":\"unauthorized\",\"message\":\"Sign in again\"}}");
    return true;
  }
  if (strstr(request->url, "/v1/account/register") != NULL) {
    transport->register_calls++;
    Respond(response, 201,
            "{\"account\":{},\"session\":{\"access_token\":\"access-one\","
            "\"expires_in\":60,\"refresh_token\":\"refresh-one\","
            "\"refresh_expires_in\":3600}}");
  } else if (strstr(request->url, "/v1/account/login") != NULL) {
    transport->login_calls++;
    Respond(response, 200,
            "{\"access_token\":\"access-login\",\"expires_in\":60,"
            "\"refresh_token\":\"refresh-login\",\"refresh_expires_in\":3600}");
  } else if (strstr(request->url, "/v1/account/refresh") != NULL) {
    transport->refresh_calls++;
    Respond(response, 200,
            "{\"access_token\":\"access-refreshed\",\"expires_in\":60,"
            "\"refresh_token\":\"refresh-rotated\",\"refresh_expires_in\":3600}");
  } else if (strstr(request->url, "/v1/account/me") != NULL) {
    transport->me_calls++;
    Respond(response, 200,
            "{\"player_id\":\"player-1\",\"username\":\"fungi\","
            "\"email\":\"fungi@example.test\",\"created_at\":\"2026-07-14T00:00:00Z\"}");
  } else if (strstr(request->url, "/v1/account/logout") != NULL) {
    transport->logout_calls++;
    Respond(response, 204, "");
  } else if (strstr(request->url, "/v1/billing/checkout") != NULL) {
    transport->checkout_calls++;
    Respond(response, 201,
            "{\"checkout_id\":\"checkout-1\","
            "\"checkout_url\":\"https://pay.example.test/checkout-1\","
            "\"expires_at\":\"2026-07-14T01:00:00Z\"}");
  } else {
    Respond(response, 404, "{\"error\":{\"code\":\"not_found\",\"message\":\"Missing\"}}");
  }
  return true;
}

static bool InitRest(ShroomClientRest* rest, const char* url, bool development_mode) {
  ShroomClientRestConfig config = {.base_url = url,
                                   .session_path = session_path,
                                   .development_mode = development_mode,
                                   .transport = FakePerform,
                                   .transport_context = &fake,
                                   .now = FakeNow};
  return ShroomClientRestInit(rest, &config);
}

void setUp(void) {
  char side_path[300];

  memset(&fake, 0, sizeof(fake));
  now_seconds = 1000u;
  snprintf(session_path, sizeof(session_path), "/tmp/shroomio-rest-%ld/session.cfg",
           (long)getpid());
  (void)ShroomClientSessionDelete(session_path);
  snprintf(side_path, sizeof(side_path), "%s.tmp", session_path);
  (void)unlink(side_path);
  snprintf(side_path, sizeof(side_path), "%s.target", session_path);
  (void)unlink(side_path);
}

void tearDown(void) {
  char side_path[300];

  (void)ShroomClientSessionDelete(session_path);
  snprintf(side_path, sizeof(side_path), "%s.tmp", session_path);
  (void)unlink(side_path);
  snprintf(side_path, sizeof(side_path), "%s.target", session_path);
  (void)unlink(side_path);
}

void test_https_is_required_unless_development_mode_is_explicit(void) {
  ShroomClientRest rest;

  TEST_ASSERT_FALSE(InitRest(&rest, "http://127.0.0.1:7443", false));
  TEST_ASSERT_TRUE(InitRest(&rest, "http://127.0.0.1:7443/", true));
  TEST_ASSERT_EQUAL_STRING("http://127.0.0.1:7443", rest.base_url);
  TEST_ASSERT_FALSE(InitRest(&rest, "https://user@example.test", false));
  TEST_ASSERT_FALSE(InitRest(&rest, "https://example.test/api", false));
  TEST_ASSERT_EQUAL_STRING("", rest.base_url);
  TEST_ASSERT_FALSE(rest.authenticated);
  TEST_ASSERT_FALSE(InitRest(&rest, "https:///", false));
}

void test_register_persists_only_refresh_session_with_private_permissions(void) {
  ShroomClientRest rest;
  ShroomClientStoredSession stored;
  struct stat metadata;

  TEST_ASSERT_TRUE(InitRest(&rest, "https://account.example.test", false));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK,
                        ShroomClientRestRegister(&rest, "fungi", "fungi@example.test", "secret"));
  TEST_ASSERT_TRUE(rest.authenticated);
  TEST_ASSERT_EQUAL_STRING("access-one", rest.access_token);
  TEST_ASSERT_TRUE(ShroomClientSessionLoad(session_path, &stored));
  TEST_ASSERT_EQUAL_STRING("refresh-one", stored.refresh_token);
  TEST_ASSERT_EQUAL_UINT64(4600u, stored.refresh_expires_at);
  TEST_ASSERT_EQUAL_INT(0, stat(session_path, &metadata));
  TEST_ASSERT_EQUAL_INT(0, metadata.st_mode & (S_IRWXG | S_IRWXO));
  TEST_ASSERT_NULL(strstr(fake.last_body, "access-one"));
}

void test_restart_restores_and_rotates_refresh_token(void) {
  ShroomClientRest first;
  ShroomClientRest restarted;
  ShroomClientStoredSession stored;

  TEST_ASSERT_TRUE(InitRest(&first, "https://account.example.test", false));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, ShroomClientRestLogin(&first, "fungi", "secret"));
  memset(&first, 0, sizeof(first));
  TEST_ASSERT_TRUE(InitRest(&restarted, "https://account.example.test", false));
  TEST_ASSERT_TRUE(ShroomClientRestHasStoredSession(&restarted));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, ShroomClientRestRestore(&restarted));
  TEST_ASSERT_EQUAL_INT(1, fake.refresh_calls);
  TEST_ASSERT_EQUAL_STRING("access-refreshed", restarted.access_token);
  TEST_ASSERT_TRUE(ShroomClientSessionLoad(session_path, &stored));
  TEST_ASSERT_EQUAL_STRING("refresh-rotated", stored.refresh_token);
}

void test_get_me_refreshes_expiring_access_and_parses_profile(void) {
  ShroomClientRest rest;

  TEST_ASSERT_TRUE(InitRest(&rest, "https://account.example.test", false));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, ShroomClientRestLogin(&rest, "fungi", "secret"));
  now_seconds = 1031u;
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, ShroomClientRestGetMe(&rest));
  TEST_ASSERT_EQUAL_INT(1, fake.refresh_calls);
  TEST_ASSERT_EQUAL_INT(1, fake.me_calls);
  TEST_ASSERT_EQUAL_STRING("Bearer access-refreshed", fake.last_authorization);
  TEST_ASSERT_EQUAL_STRING("player-1", rest.profile.player_id);
  TEST_ASSERT_EQUAL_STRING("fungi", rest.profile.username);
}

void test_unauthorized_refresh_clears_memory_and_disk_session(void) {
  ShroomClientRest rest;

  TEST_ASSERT_TRUE(InitRest(&rest, "https://account.example.test", false));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, ShroomClientRestLogin(&rest, "fungi", "secret"));
  fake.unauthorized = true;
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_UNAUTHORIZED, ShroomClientRestRefresh(&rest));
  TEST_ASSERT_FALSE(rest.authenticated);
  TEST_ASSERT_EQUAL_STRING("", rest.access_token);
  TEST_ASSERT_EQUAL_INT(-1, access(session_path, F_OK));
  TEST_ASSERT_EQUAL_STRING("Sign in again", rest.error_message);
}

void test_checkout_requires_auth_and_uses_https_result_and_idempotency_key(void) {
  ShroomClientRest rest;
  ShroomClientCheckout checkout;

  TEST_ASSERT_TRUE(InitRest(&rest, "https://account.example.test", false));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_UNAUTHORIZED,
                        ShroomClientRestBeginCheckout(&rest, "supporter", 1,
                                                      "https://game.test/success",
                                                      "https://game.test/cancel", &checkout));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, ShroomClientRestLogin(&rest, "fungi", "secret"));
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK,
                        ShroomClientRestBeginCheckout(&rest, "supporter", 1,
                                                      "https://game.test/success",
                                                      "https://game.test/cancel", &checkout));
  TEST_ASSERT_EQUAL_INT(1, fake.checkout_calls);
  TEST_ASSERT_EQUAL_UINT(36u, strlen(fake.last_idempotency));
  TEST_ASSERT_EQUAL_STRING("https://pay.example.test/checkout-1", checkout.checkout_url);
}

void test_transport_failure_returns_safe_error(void) {
  ShroomClientRest rest;

  TEST_ASSERT_TRUE(InitRest(&rest, "https://account.example.test", false));
  fake.fail_transport = true;
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_TRANSPORT_ERROR,
                        ShroomClientRestLogin(&rest, "fungi", "secret"));
  TEST_ASSERT_EQUAL_STRING("transport_error", rest.error_code);
  TEST_ASSERT_EQUAL_STRING("offline", rest.error_message);
}

void test_session_store_rejects_loose_permissions_and_duplicate_records(void) {
  ShroomClientRest rest;
  ShroomClientStoredSession stored = {.base_url = "https://account.example.test",
                                      .refresh_token = "seed",
                                      .refresh_expires_at = 9999u};
  FILE* file;

  TEST_ASSERT_TRUE(ShroomClientSessionSave(session_path, &stored));
  file = fopen(session_path, "wb");
  TEST_ASSERT_NOT_NULL(file);
  fputs("version=1\nbase_url=https://account.example.test\nrefresh_token=one\n"
        "refresh_token=two\nrefresh_expires_at=9999\n",
        file);
  TEST_ASSERT_EQUAL_INT(0, fclose(file));
  TEST_ASSERT_EQUAL_INT(0, chmod(session_path, 0600));
  TEST_ASSERT_FALSE(ShroomClientSessionLoad(session_path, &stored));

  file = fopen(session_path, "wb");
  TEST_ASSERT_NOT_NULL(file);
  fputs("version=1\nbase_url=https://account.example.test\nrefresh_token=one\n"
        "refresh_expires_at=9999\n",
        file);
  TEST_ASSERT_EQUAL_INT(0, fclose(file));
  TEST_ASSERT_EQUAL_INT(0, chmod(session_path, 0644));
  TEST_ASSERT_FALSE(ShroomClientSessionLoad(session_path, &stored));

  file = fopen(session_path, "wb");
  TEST_ASSERT_NOT_NULL(file);
  fputs("version=1\nbase_url=https://account.example.test\nrefresh_token=", file);
  for (int index = 0; index < 200; ++index) {
    fputc('a', file);
  }
  fputs("\nrefresh_expires_at=9999\n", file);
  TEST_ASSERT_EQUAL_INT(0, fclose(file));
  TEST_ASSERT_EQUAL_INT(0, chmod(session_path, 0600));
  TEST_ASSERT_FALSE(ShroomClientSessionLoad(session_path, &stored));
  TEST_ASSERT_TRUE(InitRest(&rest, "https://account.example.test", false));
  TEST_ASSERT_FALSE(ShroomClientRestHasStoredSession(&rest));
  TEST_ASSERT_EQUAL_INT(-1, access(session_path, F_OK));
}

void test_session_save_replaces_temp_symlink_without_overwriting_target(void) {
  ShroomClientStoredSession stored = {.base_url = "https://account.example.test",
                                      .refresh_token = "refresh-secret",
                                      .refresh_expires_at = 9999u};
  char temporary_path[300];
  char target_path[300];
  char contents[32];
  FILE* target;

  snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", session_path);
  snprintf(target_path, sizeof(target_path), "%s.target", session_path);
  target = fopen(target_path, "wb");
  TEST_ASSERT_NOT_NULL(target);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fputs("guard\n", target));
  TEST_ASSERT_EQUAL_INT(0, fclose(target));
  TEST_ASSERT_EQUAL_INT(0, symlink(target_path, temporary_path));

  TEST_ASSERT_TRUE(ShroomClientSessionSave(session_path, &stored));
  target = fopen(target_path, "rb");
  TEST_ASSERT_NOT_NULL(target);
  TEST_ASSERT_NOT_NULL(fgets(contents, sizeof(contents), target));
  TEST_ASSERT_EQUAL_INT(0, fclose(target));
  TEST_ASSERT_EQUAL_STRING("guard\n", contents);
  TEST_ASSERT_EQUAL_INT(0, unlink(target_path));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_https_is_required_unless_development_mode_is_explicit);
  RUN_TEST(test_register_persists_only_refresh_session_with_private_permissions);
  RUN_TEST(test_restart_restores_and_rotates_refresh_token);
  RUN_TEST(test_get_me_refreshes_expiring_access_and_parses_profile);
  RUN_TEST(test_unauthorized_refresh_clears_memory_and_disk_session);
  RUN_TEST(test_checkout_requires_auth_and_uses_https_result_and_idempotency_key);
  RUN_TEST(test_transport_failure_returns_safe_error);
  RUN_TEST(test_session_store_rejects_loose_permissions_and_duplicate_records);
  RUN_TEST(test_session_save_replaces_temp_symlink_without_overwriting_target);
  return UNITY_END();
}
