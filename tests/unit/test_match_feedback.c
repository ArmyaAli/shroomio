#include "unity.h"

#include "client/match_feedback.h"

void setUp(void) {}

void tearDown(void) {}

static void test_results_to_running_resets_match_presentation(void) {
  TEST_ASSERT_TRUE(
      ShroomMatchPresentationNeedsReset(SHROOM_MATCH_PHASE_RESULTS, SHROOM_MATCH_PHASE_RUNNING));
}

static void test_same_round_running_snapshot_preserves_match_presentation(void) {
  TEST_ASSERT_FALSE(
      ShroomMatchPresentationNeedsReset(SHROOM_MATCH_PHASE_RUNNING, SHROOM_MATCH_PHASE_RUNNING));
}

static void test_non_reset_phase_edges_preserve_match_presentation(void) {
  TEST_ASSERT_FALSE(
      ShroomMatchPresentationNeedsReset(SHROOM_MATCH_PHASE_RUNNING, SHROOM_MATCH_PHASE_RESULTS));
  TEST_ASSERT_FALSE(
      ShroomMatchPresentationNeedsReset(SHROOM_MATCH_PHASE_RESULTS, SHROOM_MATCH_PHASE_RESULTS));
  TEST_ASSERT_FALSE(
      ShroomMatchPresentationNeedsReset(SHROOM_MATCH_PHASE_RESET, SHROOM_MATCH_PHASE_RUNNING));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_results_to_running_resets_match_presentation);
  RUN_TEST(test_same_round_running_snapshot_preserves_match_presentation);
  RUN_TEST(test_non_reset_phase_edges_preserve_match_presentation);
  return UNITY_END();
}
