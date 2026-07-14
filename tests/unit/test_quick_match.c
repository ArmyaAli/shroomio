#include "unity.h"

#include <stdio.h>

#include "client/quick_match.h"

static ShroomQuickMatchState state;
static ShroomQuickMatchCandidate candidates[3];

static ShroomQuickMatchCandidate Candidate(const char* name, const char* host, uint16_t latency,
                                            uint16_t players, uint16_t capacity, bool reachable) {
  ShroomQuickMatchCandidate candidate = {.port = 7777u,
                                         .latency_ms = latency,
                                         .player_count = players,
                                         .capacity = capacity,
                                         .reachable = reachable};
  snprintf(candidate.name, sizeof(candidate.name), "%s", name);
  snprintf(candidate.host, sizeof(candidate.host), "%s", host);
  return candidate;
}

void setUp(void) {
  ShroomQuickMatchInit(&state);
  candidates[0] = Candidate("Busy Fast", "fast.test", 40u, 9u, 10u, true);
  candidates[1] = Candidate("Open Slow", "slow.test", 100u, 1u, 10u, true);
  candidates[2] = Candidate("Backup", "backup.test", 120u, 2u, 10u, true);
}

void tearDown(void) {}

void test_begin_find_preview_and_connect_transitions(void) {
  ShroomQuickMatchBegin(&state);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FINDING, state.phase);
  TEST_ASSERT_TRUE(ShroomQuickMatchSetCandidates(&state, candidates, 3u, 1000ull));
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_PREVIEW, state.phase);
  TEST_ASSERT_EQUAL_STRING("fast.test", ShroomQuickMatchSelected(&state)->host);
  ShroomQuickMatchUpdate(&state, 1000ull + SHROOM_QUICK_MATCH_PREVIEW_MS - 1ull);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_PREVIEW, state.phase);
  ShroomQuickMatchUpdate(&state, 1000ull + SHROOM_QUICK_MATCH_PREVIEW_MS);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_CONNECTING, state.phase);
  TEST_ASSERT_EQUAL_size_t(1u, state.attempt_count);
}

void test_failed_first_candidate_retries_next_ranked_candidate(void) {
  ShroomQuickMatchBegin(&state);
  TEST_ASSERT_TRUE(ShroomQuickMatchSetCandidates(&state, candidates, 3u, 1000ull));
  ShroomQuickMatchUpdate(&state, 1750ull);
  TEST_ASSERT_EQUAL_STRING("fast.test", ShroomQuickMatchSelected(&state)->host);
  ShroomQuickMatchConnectionFailed(&state, 2000ull);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_PREVIEW, state.phase);
  TEST_ASSERT_EQUAL_STRING("slow.test", ShroomQuickMatchSelected(&state)->host);
  TEST_ASSERT_EQUAL_size_t(1u, state.attempt_count);
}

void test_candidate_exhaustion_is_terminal_and_actionable(void) {
  ShroomQuickMatchBegin(&state);
  TEST_ASSERT_TRUE(ShroomQuickMatchSetCandidates(&state, candidates, 1u, 1000ull));
  ShroomQuickMatchUpdate(&state, 1750ull);
  ShroomQuickMatchConnectionFailed(&state, 2000ull);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILED, state.phase);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILURE_EXHAUSTED, state.failure);
  TEST_ASSERT_EQUAL_STRING("Every selected server failed to connect. Retry or browse manually.",
                           ShroomQuickMatchStatusText(&state));
}

void test_cancel_is_terminal_before_connection(void) {
  ShroomQuickMatchBegin(&state);
  ShroomQuickMatchCancel(&state);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_CANCELLED, state.phase);
  ShroomQuickMatchUpdate(&state, 9000ull);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_CANCELLED, state.phase);
}

void test_empty_full_and_directory_failures_are_distinct(void) {
  ShroomQuickMatchBegin(&state);
  TEST_ASSERT_FALSE(ShroomQuickMatchSetCandidates(&state, NULL, 0u, 1000ull));
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILURE_NO_SERVERS, state.failure);

  ShroomQuickMatchBegin(&state);
  candidates[0].player_count = candidates[0].capacity;
  TEST_ASSERT_FALSE(ShroomQuickMatchSetCandidates(&state, candidates, 1u, 1000ull));
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILURE_SERVERS_FULL, state.failure);

  ShroomQuickMatchBegin(&state);
  ShroomQuickMatchDirectoryFailed(&state);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILURE_DIRECTORY, state.failure);
}

void test_high_latency_fallback_is_exposed_to_ui(void) {
  ShroomQuickMatchBegin(&state);
  candidates[0].latency_ms = 240u;
  candidates[1].latency_ms = 280u;
  TEST_ASSERT_TRUE(ShroomQuickMatchSetCandidates(&state, candidates, 2u, 1000ull));
  TEST_ASSERT_TRUE(state.high_latency_fallback);
}

void test_directory_full_summary_sets_full_terminal_state(void) {
  ShroomQuickMatchBegin(&state);
  ShroomQuickMatchServersFull(&state);

  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILED, state.phase);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_FAILURE_SERVERS_FULL, state.failure);
}

void test_connection_success_completes_attempt(void) {
  ShroomQuickMatchBegin(&state);
  TEST_ASSERT_TRUE(ShroomQuickMatchSetCandidates(&state, candidates, 1u, 1000ull));
  ShroomQuickMatchUpdate(&state, 1750ull);
  ShroomQuickMatchConnectionSucceeded(&state);
  TEST_ASSERT_EQUAL(SHROOM_QUICK_MATCH_SUCCEEDED, state.phase);
  TEST_ASSERT_FALSE(ShroomQuickMatchIsActive(&state));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_find_preview_and_connect_transitions);
  RUN_TEST(test_failed_first_candidate_retries_next_ranked_candidate);
  RUN_TEST(test_candidate_exhaustion_is_terminal_and_actionable);
  RUN_TEST(test_cancel_is_terminal_before_connection);
  RUN_TEST(test_empty_full_and_directory_failures_are_distinct);
  RUN_TEST(test_directory_full_summary_sets_full_terminal_state);
  RUN_TEST(test_high_latency_fallback_is_exposed_to_ui);
  RUN_TEST(test_connection_success_completes_attempt);
  return UNITY_END();
}
