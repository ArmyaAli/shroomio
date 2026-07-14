#include "unity.h"

#include "server/lobby_capacity.h"
#include "shared/config.h"

void setUp(void) {}
void tearDown(void) {}

static void test_all_256_real_player_joins_are_admitted(void) {
  for (uint16_t count = 0u; count < SHROOM_MAX_PLAYABLE_PARTICIPANTS; ++count) {
    TEST_ASSERT_TRUE(ShroomLobbyPlanAdmission(count, 0u).accepted);
  }
  TEST_ASSERT_FALSE(ShroomLobbyPlanAdmission(SHROOM_MAX_PLAYABLE_PARTICIPANTS, 0u).accepted);
}

static void test_join_plan_removes_bots_before_rejecting_real_players(void) {
  ShroomLobbyAdmissionPlan plan = ShroomLobbyPlanAdmission(255u, 1u);

  TEST_ASSERT_TRUE(plan.accepted);
  TEST_ASSERT_EQUAL_UINT16(1u, plan.bots_to_remove);
  plan = ShroomLobbyPlanAdmission(250u, 6u);
  TEST_ASSERT_TRUE(plan.accepted);
  TEST_ASSERT_EQUAL_UINT16(1u, plan.bots_to_remove);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_all_256_real_player_joins_are_admitted);
  RUN_TEST(test_join_plan_removes_bots_before_rejecting_real_players);
  return UNITY_END();
}
