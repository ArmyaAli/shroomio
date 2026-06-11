#include "unity.h"
#include "../src/shared/lifecycle.h"

static ShroomLifecycle lifecycle;

void setUp(void) {
  ShroomLifecycleInit(&lifecycle);
}

void tearDown(void) {
}

void test_lifecycle_init(void) {
  ShroomLifecycle lc;
  ShroomLifecycleInit(&lc);
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_UNINITIALIZED, lc.state);
  TEST_ASSERT_FALSE(lc.shutdown_requested);
  TEST_ASSERT_EQUAL(0, lc.error_code);
  TEST_ASSERT_NULL(lc.error_message);
}

void test_lifecycle_init_null(void) {
  ShroomLifecycleInit(NULL);
}

void test_lifecycle_can_transition_from_uninitialized(void) {
  TEST_ASSERT_TRUE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_INIT));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_START));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_PAUSE));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_RESUME));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_STOP));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_ERROR));
}

void test_lifecycle_transition_init(void) {
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_INIT));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_INITIALIZING, lifecycle.state);
}

void test_lifecycle_transition_start_from_initializing(void) {
  lifecycle.state = SHROOM_LIFECYCLE_INITIALIZING;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_START));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_RUNNING, lifecycle.state);
}

void test_lifecycle_transition_start_from_initialized(void) {
  lifecycle.state = SHROOM_LIFECYCLE_INITIALIZED;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_START));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_RUNNING, lifecycle.state);
}

void test_lifecycle_transition_pause(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_PAUSE));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_PAUSED, lifecycle.state);
}

void test_lifecycle_transition_resume(void) {
  lifecycle.state = SHROOM_LIFECYCLE_PAUSED;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_RESUME));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_RUNNING, lifecycle.state);
}

void test_lifecycle_transition_stop_from_running(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_STOP));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_SHUTTING_DOWN, lifecycle.state);
}

void test_lifecycle_transition_stop_from_paused(void) {
  lifecycle.state = SHROOM_LIFECYCLE_PAUSED;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_STOP));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_SHUTTING_DOWN, lifecycle.state);
}

void test_lifecycle_transition_shutdown_from_shutting_down(void) {
  lifecycle.state = SHROOM_LIFECYCLE_SHUTTING_DOWN;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_SHUTDOWN, lifecycle.state);
}

void test_lifecycle_transition_error_from_running(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_ERROR));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_ERROR, lifecycle.state);
}

void test_lifecycle_transition_error_from_initializing(void) {
  lifecycle.state = SHROOM_LIFECYCLE_INITIALIZING;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_ERROR));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_ERROR, lifecycle.state);
}

void test_lifecycle_transition_shutdown_from_error(void) {
  lifecycle.state = SHROOM_LIFECYCLE_ERROR;
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_SHUTDOWN, lifecycle.state);
}

void test_lifecycle_invalid_transition_from_shutdown(void) {
  lifecycle.state = SHROOM_LIFECYCLE_SHUTDOWN;
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_INIT));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_START));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_STOP));
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_ERROR));
}

void test_lifecycle_invalid_transition_start_from_running(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_START));
}

void test_lifecycle_invalid_transition_pause_from_paused(void) {
  lifecycle.state = SHROOM_LIFECYCLE_PAUSED;
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_PAUSE));
}

void test_lifecycle_invalid_transition_resume_from_running(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_RESUME));
}

void test_lifecycle_request_shutdown(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_FALSE(lifecycle.shutdown_requested);
  ShroomLifecycleRequestShutdown(&lifecycle);
  TEST_ASSERT_TRUE(lifecycle.shutdown_requested);
}

void test_lifecycle_request_shutdown_null(void) {
  ShroomLifecycleRequestShutdown(NULL);
}

void test_lifecycle_is_running(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  TEST_ASSERT_TRUE(ShroomLifecycleIsRunning(&lifecycle));
  
  lifecycle.state = SHROOM_LIFECYCLE_PAUSED;
  TEST_ASSERT_TRUE(ShroomLifecycleIsRunning(&lifecycle));
  
  lifecycle.state = SHROOM_LIFECYCLE_INITIALIZING;
  TEST_ASSERT_FALSE(ShroomLifecycleIsRunning(&lifecycle));
  
  lifecycle.state = SHROOM_LIFECYCLE_SHUTDOWN;
  TEST_ASSERT_FALSE(ShroomLifecycleIsRunning(&lifecycle));
}

void test_lifecycle_is_running_null(void) {
  TEST_ASSERT_FALSE(ShroomLifecycleIsRunning(NULL));
}

void test_lifecycle_is_shutdown_requested(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  lifecycle.shutdown_requested = false;
  TEST_ASSERT_FALSE(ShroomLifecycleIsShutdownRequested(&lifecycle));
  
  lifecycle.shutdown_requested = true;
  TEST_ASSERT_TRUE(ShroomLifecycleIsShutdownRequested(&lifecycle));
  
  lifecycle.shutdown_requested = false;
  lifecycle.state = SHROOM_LIFECYCLE_SHUTTING_DOWN;
  TEST_ASSERT_TRUE(ShroomLifecycleIsShutdownRequested(&lifecycle));
  
  lifecycle.state = SHROOM_LIFECYCLE_SHUTDOWN;
  TEST_ASSERT_TRUE(ShroomLifecycleIsShutdownRequested(&lifecycle));
}

void test_lifecycle_is_shutdown_requested_null(void) {
  TEST_ASSERT_TRUE(ShroomLifecycleIsShutdownRequested(NULL));
}

void test_lifecycle_set_error(void) {
  lifecycle.state = SHROOM_LIFECYCLE_RUNNING;
  ShroomLifecycleSetError(&lifecycle, 42, "Test error");
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_ERROR, lifecycle.state);
  TEST_ASSERT_EQUAL(42, lifecycle.error_code);
  TEST_ASSERT_EQUAL_STRING("Test error", lifecycle.error_message);
}

void test_lifecycle_set_error_null(void) {
  ShroomLifecycleSetError(NULL, 42, "Test error");
}

void test_lifecycle_state_to_string(void) {
  TEST_ASSERT_EQUAL_STRING("UNINITIALIZED", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_UNINITIALIZED));
  TEST_ASSERT_EQUAL_STRING("INITIALIZING", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_INITIALIZING));
  TEST_ASSERT_EQUAL_STRING("INITIALIZED", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_INITIALIZED));
  TEST_ASSERT_EQUAL_STRING("RUNNING", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_RUNNING));
  TEST_ASSERT_EQUAL_STRING("PAUSED", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_PAUSED));
  TEST_ASSERT_EQUAL_STRING("SHUTTING_DOWN", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_SHUTTING_DOWN));
  TEST_ASSERT_EQUAL_STRING("SHUTDOWN", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_SHUTDOWN));
  TEST_ASSERT_EQUAL_STRING("ERROR", ShroomLifecycleStateToString(SHROOM_LIFECYCLE_ERROR));
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", ShroomLifecycleStateToString(999));
}

void test_lifecycle_event_to_string(void) {
  TEST_ASSERT_EQUAL_STRING("INIT", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_INIT));
  TEST_ASSERT_EQUAL_STRING("START", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_START));
  TEST_ASSERT_EQUAL_STRING("PAUSE", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_PAUSE));
  TEST_ASSERT_EQUAL_STRING("RESUME", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_RESUME));
  TEST_ASSERT_EQUAL_STRING("STOP", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_STOP));
  TEST_ASSERT_EQUAL_STRING("SHUTDOWN", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_SHUTDOWN));
  TEST_ASSERT_EQUAL_STRING("ERROR", ShroomLifecycleEventToString(SHROOM_LIFECYCLE_EVENT_ERROR));
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", ShroomLifecycleEventToString(999));
}

void test_lifecycle_transition_null(void) {
  TEST_ASSERT_FALSE(ShroomLifecycleTransition(NULL, SHROOM_LIFECYCLE_EVENT_INIT));
}

void test_lifecycle_can_transition_null(void) {
  TEST_ASSERT_FALSE(ShroomLifecycleCanTransition(NULL, SHROOM_LIFECYCLE_EVENT_INIT));
}

void test_lifecycle_full_lifecycle(void) {
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_UNINITIALIZED, lifecycle.state);
  
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_INIT));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_INITIALIZING, lifecycle.state);
  
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_START));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_RUNNING, lifecycle.state);
  TEST_ASSERT_TRUE(ShroomLifecycleIsRunning(&lifecycle));
  
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_PAUSE));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_PAUSED, lifecycle.state);
  TEST_ASSERT_TRUE(ShroomLifecycleIsRunning(&lifecycle));
  
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_RESUME));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_RUNNING, lifecycle.state);
  
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_STOP));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_SHUTTING_DOWN, lifecycle.state);
  TEST_ASSERT_FALSE(ShroomLifecycleIsRunning(&lifecycle));
  
  TEST_ASSERT_TRUE(ShroomLifecycleTransition(&lifecycle, SHROOM_LIFECYCLE_EVENT_SHUTDOWN));
  TEST_ASSERT_EQUAL(SHROOM_LIFECYCLE_SHUTDOWN, lifecycle.state);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_lifecycle_init);
  RUN_TEST(test_lifecycle_init_null);
  RUN_TEST(test_lifecycle_can_transition_from_uninitialized);
  RUN_TEST(test_lifecycle_transition_init);
  RUN_TEST(test_lifecycle_transition_start_from_initializing);
  RUN_TEST(test_lifecycle_transition_start_from_initialized);
  RUN_TEST(test_lifecycle_transition_pause);
  RUN_TEST(test_lifecycle_transition_resume);
  RUN_TEST(test_lifecycle_transition_stop_from_running);
  RUN_TEST(test_lifecycle_transition_stop_from_paused);
  RUN_TEST(test_lifecycle_transition_shutdown_from_shutting_down);
  RUN_TEST(test_lifecycle_transition_error_from_running);
  RUN_TEST(test_lifecycle_transition_error_from_initializing);
  RUN_TEST(test_lifecycle_transition_shutdown_from_error);
  RUN_TEST(test_lifecycle_invalid_transition_from_shutdown);
  RUN_TEST(test_lifecycle_invalid_transition_start_from_running);
  RUN_TEST(test_lifecycle_invalid_transition_pause_from_paused);
  RUN_TEST(test_lifecycle_invalid_transition_resume_from_running);
  RUN_TEST(test_lifecycle_request_shutdown);
  RUN_TEST(test_lifecycle_request_shutdown_null);
  RUN_TEST(test_lifecycle_is_running);
  RUN_TEST(test_lifecycle_is_running_null);
  RUN_TEST(test_lifecycle_is_shutdown_requested);
  RUN_TEST(test_lifecycle_is_shutdown_requested_null);
  RUN_TEST(test_lifecycle_set_error);
  RUN_TEST(test_lifecycle_set_error_null);
  RUN_TEST(test_lifecycle_state_to_string);
  RUN_TEST(test_lifecycle_event_to_string);
  RUN_TEST(test_lifecycle_transition_null);
  RUN_TEST(test_lifecycle_can_transition_null);
  RUN_TEST(test_lifecycle_full_lifecycle);
  return UNITY_END();
}
