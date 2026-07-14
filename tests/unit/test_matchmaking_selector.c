#include "unity.h"

#include <float.h>
#include <math.h>

#include "client/matchmaking_selector.h"

static ShroomMatchmakingCandidate candidates[5];

void setUp(void) {
  candidates[0] = (ShroomMatchmakingCandidate){"slow-empty.test", 7777u, 100u, 1u, 10u, true};
  candidates[1] = (ShroomMatchmakingCandidate){"fast-busy.test", 7777u, 40u, 9u, 10u, true};
}

void tearDown(void) {}

void test_default_weights_choose_weighted_winner(void) {
  ShroomMatchmakingSelection selection;

  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(candidates, 2u, ShroomMatchmakingDefaultWeights(),
                                          &selection));
  TEST_ASSERT_EQUAL_size_t(1u, selection.candidate_index);
  TEST_ASSERT_FALSE(selection.high_latency_fallback);
}

void test_custom_weights_change_the_weighted_winner(void) {
  ShroomMatchmakingSelection selection;

  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(
      candidates, 2u, (ShroomMatchmakingWeights){.latency = 0.1f, .load = 0.9f}, &selection));
  TEST_ASSERT_EQUAL_size_t(0u, selection.candidate_index);
}

void test_full_and_unreachable_candidates_are_excluded(void) {
  ShroomMatchmakingSelection selection;

  candidates[0].reachable = false;
  candidates[1].player_count = candidates[1].capacity;
  TEST_ASSERT_FALSE(ShroomMatchmakingSelect(candidates, 2u, ShroomMatchmakingDefaultWeights(),
                                           &selection));
  TEST_ASSERT_EQUAL_size_t(SIZE_MAX, selection.candidate_index);
}

void test_two_hundred_ms_boundary_is_preferred_over_faster_fallback_pool_member(void) {
  ShroomMatchmakingSelection selection;

  candidates[0].latency_ms = SHROOM_MATCHMAKING_PREFERRED_LATENCY_MS;
  candidates[1].latency_ms = SHROOM_MATCHMAKING_PREFERRED_LATENCY_MS + 1u;
  candidates[1].player_count = 0u;
  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(candidates, 2u, ShroomMatchmakingDefaultWeights(),
                                          &selection));
  TEST_ASSERT_EQUAL_size_t(0u, selection.candidate_index);
  TEST_ASSERT_FALSE(selection.high_latency_fallback);
}

void test_all_high_latency_candidates_use_fallback_and_warn(void) {
  ShroomMatchmakingSelection selection;

  candidates[0].latency_ms = 260u;
  candidates[1].latency_ms = 240u;
  candidates[1].player_count = 0u;
  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(candidates, 2u, ShroomMatchmakingDefaultWeights(),
                                          &selection));
  TEST_ASSERT_TRUE(selection.high_latency_fallback);
}

void test_no_candidates_returns_false(void) {
  ShroomMatchmakingSelection selection;

  TEST_ASSERT_FALSE(
      ShroomMatchmakingSelect(NULL, 0u, ShroomMatchmakingDefaultWeights(), &selection));
}

void test_invalid_weight_configuration_is_rejected(void) {
  ShroomMatchmakingSelection selection;

  TEST_ASSERT_FALSE(ShroomMatchmakingWeightsAreValid((ShroomMatchmakingWeights){0.0f, 0.0f}));
  TEST_ASSERT_FALSE(ShroomMatchmakingWeightsAreValid((ShroomMatchmakingWeights){-0.1f, 1.1f}));
  TEST_ASSERT_FALSE(ShroomMatchmakingWeightsAreValid((ShroomMatchmakingWeights){NAN, 1.0f}));
  TEST_ASSERT_FALSE(
      ShroomMatchmakingWeightsAreValid((ShroomMatchmakingWeights){FLT_MAX, FLT_MAX}));
  TEST_ASSERT_FALSE(ShroomMatchmakingSelect(
      candidates, 2u, (ShroomMatchmakingWeights){0.0f, 0.0f}, &selection));
}

void test_equal_scores_tie_break_by_latency_load_host_and_port(void) {
  ShroomMatchmakingSelection selection;

  candidates[0] = (ShroomMatchmakingCandidate){"z.test", 8000u, 60u, 5u, 10u, true};
  candidates[1] = (ShroomMatchmakingCandidate){"a.test", 7000u, 40u, 5u, 10u, true};
  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(
      candidates, 2u, (ShroomMatchmakingWeights){.latency = 0.0f, .load = 1.0f}, &selection));
  TEST_ASSERT_EQUAL_size_t(1u, selection.candidate_index);

  candidates[0] = (ShroomMatchmakingCandidate){"z.test", 8000u, 50u, 6u, 10u, true};
  candidates[1] = (ShroomMatchmakingCandidate){"a.test", 7000u, 50u, 4u, 10u, true};
  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(
      candidates, 2u, (ShroomMatchmakingWeights){.latency = 1.0f, .load = 0.0f}, &selection));
  TEST_ASSERT_EQUAL_size_t(1u, selection.candidate_index);

  candidates[0] = (ShroomMatchmakingCandidate){"z.test", 8000u, 50u, 5u, 10u, true};
  candidates[1] = (ShroomMatchmakingCandidate){"b.test", 9000u, 50u, 5u, 10u, true};
  candidates[2] = (ShroomMatchmakingCandidate){"a.test", 9000u, 50u, 5u, 10u, true};
  candidates[3] = (ShroomMatchmakingCandidate){"a.test", 7000u, 50u, 5u, 10u, true};
  TEST_ASSERT_TRUE(ShroomMatchmakingSelect(candidates, 4u, ShroomMatchmakingDefaultWeights(),
                                          &selection));
  TEST_ASSERT_EQUAL_size_t(3u, selection.candidate_index);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_default_weights_choose_weighted_winner);
  RUN_TEST(test_custom_weights_change_the_weighted_winner);
  RUN_TEST(test_full_and_unreachable_candidates_are_excluded);
  RUN_TEST(test_two_hundred_ms_boundary_is_preferred_over_faster_fallback_pool_member);
  RUN_TEST(test_all_high_latency_candidates_use_fallback_and_warn);
  RUN_TEST(test_no_candidates_returns_false);
  RUN_TEST(test_invalid_weight_configuration_is_rejected);
  RUN_TEST(test_equal_scores_tie_break_by_latency_load_host_and_port);
  return UNITY_END();
}
