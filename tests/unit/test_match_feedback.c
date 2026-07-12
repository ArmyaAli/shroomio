#include "unity.h"

#include "client/match_feedback.h"

void setUp(void) {}

void tearDown(void) {}

static void test_results_to_running_requires_feedback_rebaseline(void) {
  TEST_ASSERT_TRUE(
      ShroomMatchFeedbackNeedsRebaseline(SHROOM_MATCH_PHASE_RESULTS, SHROOM_MATCH_PHASE_RUNNING));
}

static void test_same_round_running_snapshot_preserves_feedback_inference(void) {
  TEST_ASSERT_FALSE(
      ShroomMatchFeedbackNeedsRebaseline(SHROOM_MATCH_PHASE_RUNNING, SHROOM_MATCH_PHASE_RUNNING));
}

static void test_non_reset_phase_edges_do_not_rebaseline(void) {
  TEST_ASSERT_FALSE(
      ShroomMatchFeedbackNeedsRebaseline(SHROOM_MATCH_PHASE_RUNNING, SHROOM_MATCH_PHASE_RESULTS));
  TEST_ASSERT_FALSE(
      ShroomMatchFeedbackNeedsRebaseline(SHROOM_MATCH_PHASE_RESULTS, SHROOM_MATCH_PHASE_RESULTS));
  TEST_ASSERT_FALSE(
      ShroomMatchFeedbackNeedsRebaseline(SHROOM_MATCH_PHASE_RESET, SHROOM_MATCH_PHASE_RUNNING));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_results_to_running_requires_feedback_rebaseline);
  RUN_TEST(test_same_round_running_snapshot_preserves_feedback_inference);
  RUN_TEST(test_non_reset_phase_edges_do_not_rebaseline);
  return UNITY_END();
}
