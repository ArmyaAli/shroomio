#include "unity.h"

#include "client/server_browser_model.h"

static ShroomServerBrowserModel model;

void setUp(void) { ShroomServerBrowserModelInit(&model); }

void tearDown(void) {}

void test_refresh_states_distinguish_empty_ready_and_failure(void) {
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_EMPTY, model.discovery_state);
  ShroomServerBrowserBeginRefresh(&model);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_LOADING, model.discovery_state);
  ShroomServerBrowserFinishRefresh(&model, true, 0u);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_EMPTY, model.discovery_state);
  ShroomServerBrowserFinishRefresh(&model, true, 2u);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_READY, model.discovery_state);
  ShroomServerBrowserFinishRefresh(&model, false, 0u);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_FAILED, model.discovery_state);
}

void test_only_ready_results_become_stale(void) {
  ShroomServerBrowserMarkStale(&model);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_EMPTY, model.discovery_state);
  ShroomServerBrowserFinishRefresh(&model, true, 1u);
  ShroomServerBrowserMarkStale(&model);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_DISCOVERY_STALE, model.discovery_state);
}

void test_sort_selection_resets_direction_and_repeated_key_toggles(void) {
  TEST_ASSERT_EQUAL(SHROOM_SERVER_SORT_PING, model.sort_key);
  TEST_ASSERT_FALSE(model.sort_descending);
  ShroomServerBrowserSetSort(&model, SHROOM_SERVER_SORT_PING);
  TEST_ASSERT_TRUE(model.sort_descending);
  ShroomServerBrowserSetSort(&model, SHROOM_SERVER_SORT_NAME);
  TEST_ASSERT_EQUAL(SHROOM_SERVER_SORT_NAME, model.sort_key);
  TEST_ASSERT_FALSE(model.sort_descending);
  TEST_ASSERT_EQUAL_STRING("Name", ShroomServerBrowserSortLabel(model.sort_key));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_refresh_states_distinguish_empty_ready_and_failure);
  RUN_TEST(test_only_ready_results_become_stale);
  RUN_TEST(test_sort_selection_resets_direction_and_repeated_key_toggles);
  return UNITY_END();
}
