#include "unity.h"

#include "client/results_transition.h"

void setUp(void) {}

void tearDown(void) {}

static ShroomIntermissionStatusPacket ResolvedStatus(uint32_t round_id,
                                                     ShroomRematchVote decision) {
  return (ShroomIntermissionStatusPacket){
      .round_id = round_id,
      .resolved = 1u,
      .decision = (uint8_t)decision,
  };
}

void test_unresolved_status_waits_through_every_match_phase(void) {
  ShroomIntermissionStatusPacket status = {.round_id = 4u};

  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESULTS, true, &status, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESET, true, &status, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RUNNING, true, &status, false, 0u));
}

void test_each_resolved_decision_has_a_deterministic_route(void) {
  ShroomIntermissionStatusPacket play = ResolvedStatus(5u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  ShroomIntermissionStatusPacket lobby =
      ResolvedStatus(5u, SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY);
  ShroomIntermissionStatusPacket spectate = ResolvedStatus(5u, SHROOM_REMATCH_VOTE_SPECTATE);

  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESULTS, true, &play, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESET, true, &play, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_GAME,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RUNNING, true, &play, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_LOBBY,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESULTS, true, &lobby, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_SPECTATE,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESULTS, true, &spectate, false, 0u));
}

void test_missing_status_only_resumes_after_running_snapshot(void) {
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESULTS, false, NULL, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_WAIT,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESET, false, NULL, false, 0u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_GAME,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RUNNING, false, NULL, false, 0u));
}

void test_stale_and_duplicate_statuses_are_ignored_after_consumption(void) {
  ShroomIntermissionStatusPacket stale =
      ResolvedStatus(8u, SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY);
  ShroomIntermissionStatusPacket duplicate =
      ResolvedStatus(9u, SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY);
  ShroomIntermissionStatusPacket current = ResolvedStatus(10u, SHROOM_REMATCH_VOTE_SPECTATE);

  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_GAME,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RUNNING, true, &stale, true, 9u));
  TEST_ASSERT_EQUAL(
      SHROOM_RESULTS_ROUTE_GAME,
      ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RUNNING, true, &duplicate, true, 9u));
  TEST_ASSERT_EQUAL(SHROOM_RESULTS_ROUTE_SPECTATE,
                    ShroomResultsResolveRoute(SHROOM_MATCH_PHASE_RESULTS, true, &current, true, 9u));
}

void test_round_comparison_handles_wraparound(void) {
  TEST_ASSERT_TRUE(ShroomIntermissionRoundIsNewer(1u, UINT32_MAX));
  TEST_ASSERT_FALSE(ShroomIntermissionRoundIsNewer(UINT32_MAX, 1u));
  TEST_ASSERT_FALSE(ShroomIntermissionRoundIsNewer(7u, 7u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_unresolved_status_waits_through_every_match_phase);
  RUN_TEST(test_each_resolved_decision_has_a_deterministic_route);
  RUN_TEST(test_missing_status_only_resumes_after_running_snapshot);
  RUN_TEST(test_stale_and_duplicate_statuses_are_ignored_after_consumption);
  RUN_TEST(test_round_comparison_handles_wraparound);
  return UNITY_END();
}
