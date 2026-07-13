#include "unity.h"

#include "client/results_summary.h"

void setUp(void) {}

void tearDown(void) {}

void test_elapsed_seconds_truncates_and_clamps_to_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(65u, ShroomResultsElapsedSeconds(10.0f, 75.9f));
  TEST_ASSERT_EQUAL_UINT32(0u, ShroomResultsElapsedSeconds(10.0f, 9.0f));
  TEST_ASSERT_EQUAL_UINT32(0u, ShroomResultsElapsedSeconds(10.0f, 10.0f));
}

void test_duration_format_uses_minutes_and_zero_padded_seconds(void) {
  char duration[32];

  ShroomResultsFormatDuration(0u, duration, sizeof(duration));
  TEST_ASSERT_EQUAL_STRING("0:00", duration);

  ShroomResultsFormatDuration(65u, duration, sizeof(duration));
  TEST_ASSERT_EQUAL_STRING("1:05", duration);

  ShroomResultsFormatDuration(3661u, duration, sizeof(duration));
  TEST_ASSERT_EQUAL_STRING("61:01", duration);
}

void test_vote_labels_cover_each_server_vote_state(void) {
  TEST_ASSERT_EQUAL_STRING("Not cast", ShroomResultsVoteLabel(SHROOM_REMATCH_VOTE_NONE));
  TEST_ASSERT_EQUAL_STRING("Play Again", ShroomResultsVoteLabel(SHROOM_REMATCH_VOTE_PLAY_AGAIN));
  TEST_ASSERT_EQUAL_STRING("Return to Lobby",
                           ShroomResultsVoteLabel(SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY));
  TEST_ASSERT_EQUAL_STRING("Spectate", ShroomResultsVoteLabel(SHROOM_REMATCH_VOTE_SPECTATE));
}

void test_vote_formatting_includes_participation_local_state_and_decision(void) {
  char text[128];

  ShroomResultsFormatVoteParticipation(2u, 1u, 0u, 5u, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Participation: 3 / 5 eligible", text);

  ShroomResultsFormatLocalVote(SHROOM_REMATCH_VOTE_NONE, true, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Your vote: Not cast", text);
  ShroomResultsFormatLocalVote(SHROOM_REMATCH_VOTE_PLAY_AGAIN, false, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Your vote: Not eligible", text);

  ShroomResultsFormatDecision(SHROOM_REMATCH_VOTE_SPECTATE, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("Decision: Spectate", text);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_elapsed_seconds_truncates_and_clamps_to_zero);
  RUN_TEST(test_duration_format_uses_minutes_and_zero_padded_seconds);
  RUN_TEST(test_vote_labels_cover_each_server_vote_state);
  RUN_TEST(test_vote_formatting_includes_participation_local_state_and_decision);
  return UNITY_END();
}
