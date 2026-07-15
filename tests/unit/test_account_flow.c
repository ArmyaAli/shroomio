#include "unity.h"

#include "client/account_flow.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct FlowTransport {
  atomic_bool release_login;
  bool fail;
  int login_calls;
  int me_calls;
} FlowTransport;

static FlowTransport transport;
static ShroomClientRest rest;
static ShroomAccountFlow flow;
static char session_path[256];

static void Respond(ShroomClientHttpResponse* response, long status, const char* body) {
  response->status = status;
  response->body_length = strlen(body);
  memcpy(response->body, body, response->body_length + 1u);
}

static bool Perform(void* context, const ShroomClientHttpRequest* request,
                    ShroomClientHttpResponse* response) {
  FlowTransport* state = context;

  if (strstr(request->url, "/login") != NULL) {
    struct timespec pause = {.tv_nsec = 1000000L};
    state->login_calls++;
    while (!atomic_load_explicit(&state->release_login, memory_order_acquire)) {
      nanosleep(&pause, NULL);
    }
    if (state->fail) {
      Respond(response, 401,
              "{\"error\":{\"code\":\"invalid_credentials\","
              "\"message\":\"Email or password is incorrect\"}}");
    } else {
      Respond(response, 200,
              "{\"access_token\":\"access\",\"expires_in\":60,"
              "\"refresh_token\":\"refresh\",\"refresh_expires_in\":3600}");
    }
  } else if (strstr(request->url, "/me") != NULL) {
    state->me_calls++;
    Respond(response, 200,
            "{\"player_id\":\"player-1\",\"username\":\"fungi\","
            "\"email\":\"fungi@example.test\",\"created_at\":\"2026-07-14T00:00:00Z\"}");
  } else {
    return false;
  }
  return true;
}

static ShroomAccountFlowState WaitForCompletion(void) {
  struct timespec pause = {.tv_nsec = 1000000L};
  ShroomAccountFlowState state;

  for (int attempt = 0; attempt < 2000; ++attempt) {
    state = ShroomAccountFlowPoll(&flow);
    if (state != SHROOM_ACCOUNT_FLOW_WORKING) {
      return state;
    }
    nanosleep(&pause, NULL);
  }
  return SHROOM_ACCOUNT_FLOW_WORKING;
}

void setUp(void) {
  ShroomClientRestConfig config;

  memset(&transport, 0, sizeof(transport));
  atomic_init(&transport.release_login, false);
  snprintf(session_path, sizeof(session_path), "/tmp/shroomio-flow-%ld/session.cfg",
           (long)getpid());
  (void)ShroomClientSessionDelete(session_path);
  config = (ShroomClientRestConfig){.base_url = "https://account.example.test",
                                    .session_path = session_path,
                                    .transport = Perform,
                                    .transport_context = &transport};
  TEST_ASSERT_TRUE(ShroomClientRestInit(&rest, &config));
  ShroomAccountFlowInit(&flow, &rest);
}

void tearDown(void) {
  atomic_store_explicit(&transport.release_login, true, memory_order_release);
  ShroomAccountFlowShutdown(&flow);
  (void)ShroomClientSessionDelete(session_path);
}

void test_login_runs_off_thread_and_loads_profile(void) {
  TEST_ASSERT_TRUE(ShroomAccountFlowStartLogin(&flow, "fungi", "private-password"));
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_FLOW_WORKING, ShroomAccountFlowPoll(&flow));
  atomic_store_explicit(&transport.release_login, true, memory_order_release);
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_FLOW_SUCCEEDED, WaitForCompletion());
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_OK, flow.result);
  TEST_ASSERT_EQUAL_INT(1, transport.login_calls);
  TEST_ASSERT_EQUAL_INT(1, transport.me_calls);
  TEST_ASSERT_EQUAL_STRING("fungi", rest.profile.username);
  TEST_ASSERT_EQUAL_STRING("", flow.password);
  ShroomAccountFlowAcknowledge(&flow);
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_FLOW_IDLE, ShroomAccountFlowPoll(&flow));
}

void test_second_request_cannot_replace_in_flight_credentials(void) {
  TEST_ASSERT_TRUE(ShroomAccountFlowStartLogin(&flow, "first", "first-password"));
  TEST_ASSERT_FALSE(ShroomAccountFlowStartLogin(&flow, "second", "second-password"));
  TEST_ASSERT_EQUAL_STRING("first", flow.identity);
  TEST_ASSERT_EQUAL_STRING("first-password", flow.password);
  atomic_store_explicit(&transport.release_login, true, memory_order_release);
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_FLOW_SUCCEEDED, WaitForCompletion());
}

void test_failure_is_reported_without_retaining_password(void) {
  transport.fail = true;
  TEST_ASSERT_TRUE(ShroomAccountFlowStartLogin(&flow, "fungi", "private-password"));
  atomic_store_explicit(&transport.release_login, true, memory_order_release);
  TEST_ASSERT_EQUAL_INT(SHROOM_ACCOUNT_FLOW_FAILED, WaitForCompletion());
  TEST_ASSERT_EQUAL_INT(SHROOM_CLIENT_REST_UNAUTHORIZED, flow.result);
  TEST_ASSERT_EQUAL_STRING("Email or password is incorrect", rest.error_message);
  TEST_ASSERT_EQUAL_STRING("", flow.password);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_login_runs_off_thread_and_loads_profile);
  RUN_TEST(test_second_request_cannot_replace_in_flight_credentials);
  RUN_TEST(test_failure_is_reported_without_retaining_password);
  return UNITY_END();
}
