#include "unity.h"
#include "../src/shared/intermission.h"

static ShroomIntermissionState state;
static const uint32_t players[] = {10u, 20u, 30u, 40u};

void setUp(void) { ShroomIntermissionBegin(&state, players, 4u, 7u); }
void tearDown(void) {}

void test_begin_freezes_eligible_players_and_round(void) {
  TEST_ASSERT_EQUAL_UINT32(7u, state.round_id);
  TEST_ASSERT_EQUAL_UINT16(4u, state.eligible_count);
  TEST_ASSERT_FALSE(ShroomIntermissionCastVote(&state, 99u, SHROOM_REMATCH_VOTE_PLAY_AGAIN));
}

void test_vote_can_change_without_double_counting(void) {
  TEST_ASSERT_TRUE(ShroomIntermissionCastVote(&state, 10u, SHROOM_REMATCH_VOTE_PLAY_AGAIN));
  TEST_ASSERT_FALSE(ShroomIntermissionCastVote(&state, 10u, SHROOM_REMATCH_VOTE_PLAY_AGAIN));
  TEST_ASSERT_TRUE(ShroomIntermissionCastVote(&state, 10u, SHROOM_REMATCH_VOTE_SPECTATE));
  TEST_ASSERT_EQUAL_UINT16(0u, state.play_again_votes);
  TEST_ASSERT_EQUAL_UINT16(1u, state.spectate_votes);
}

void test_disconnect_removes_vote_and_eligibility(void) {
  ShroomIntermissionCastVote(&state, 20u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  TEST_ASSERT_TRUE(ShroomIntermissionRemoveVoter(&state, 20u));
  TEST_ASSERT_EQUAL_UINT16(3u, state.eligible_count);
  TEST_ASSERT_EQUAL_UINT16(0u, state.play_again_votes);
  TEST_ASSERT_FALSE(ShroomIntermissionCastVote(&state, 20u, SHROOM_REMATCH_VOTE_PLAY_AGAIN));
}

void test_all_voted_resolves_play_again_plurality(void) {
  ShroomIntermissionCastVote(&state, 10u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  ShroomIntermissionCastVote(&state, 20u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  ShroomIntermissionCastVote(&state, 30u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  ShroomIntermissionCastVote(&state, 40u, SHROOM_REMATCH_VOTE_SPECTATE);
  TEST_ASSERT_TRUE(ShroomIntermissionAllEligibleVoted(&state));
  TEST_ASSERT_EQUAL(SHROOM_REMATCH_VOTE_PLAY_AGAIN, ShroomIntermissionResolve(&state));
  TEST_ASSERT_FALSE(ShroomIntermissionCastVote(&state, 40u, SHROOM_REMATCH_VOTE_PLAY_AGAIN));
}

void test_non_voters_default_to_lobby_and_lobby_wins_ties(void) {
  ShroomIntermissionCastVote(&state, 10u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  ShroomIntermissionCastVote(&state, 20u, SHROOM_REMATCH_VOTE_PLAY_AGAIN);
  TEST_ASSERT_EQUAL(SHROOM_REMATCH_VOTE_RETURN_TO_LOBBY, ShroomIntermissionResolve(&state));
}

void test_spectate_requires_strict_plurality(void) {
  ShroomIntermissionCastVote(&state, 10u, SHROOM_REMATCH_VOTE_SPECTATE);
  ShroomIntermissionCastVote(&state, 20u, SHROOM_REMATCH_VOTE_SPECTATE);
  ShroomIntermissionCastVote(&state, 30u, SHROOM_REMATCH_VOTE_SPECTATE);
  TEST_ASSERT_EQUAL(SHROOM_REMATCH_VOTE_SPECTATE, ShroomIntermissionResolve(&state));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_begin_freezes_eligible_players_and_round);
  RUN_TEST(test_vote_can_change_without_double_counting);
  RUN_TEST(test_disconnect_removes_vote_and_eligibility);
  RUN_TEST(test_all_voted_resolves_play_again_plurality);
  RUN_TEST(test_non_voters_default_to_lobby_and_lobby_wins_ties);
  RUN_TEST(test_spectate_requires_strict_plurality);
  return UNITY_END();
}
