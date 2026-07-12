#include "unity.h"

#include "client/game_mode_availability.h"

void setUp(void) {}

void tearDown(void) {}

void test_capabilities_cover_every_game_mode_once(void) {
  bool seen[SHROOM_GAME_MODE_COUNT] = {false};
  size_t count;
  const ShroomGameModeCapability* capabilities = ShroomGameModeCapabilities(&count);

  TEST_ASSERT_EQUAL_INT(SHROOM_GAME_MODE_COUNT, count);
  for (size_t index = 0; index < count; ++index) {
    TEST_ASSERT_GREATER_OR_EQUAL_INT(SHROOM_GAME_MODE_FFA, capabilities[index].mode);
    TEST_ASSERT_LESS_THAN_INT(SHROOM_GAME_MODE_COUNT, capabilities[index].mode);
    TEST_ASSERT_FALSE(seen[capabilities[index].mode]);
    TEST_ASSERT_NOT_NULL(capabilities[index].label);
    TEST_ASSERT_NOT_NULL(capabilities[index].summary);
    seen[capabilities[index].mode] = true;
  }
}

void test_supported_modes_match_server_implementations(void) {
  TEST_ASSERT_TRUE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_FFA));
  TEST_ASSERT_TRUE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_KING_OF_HILL));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_TEAMS_2V2));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_TEAMS_3V3));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_TEAMS_4V4));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_BATTLE_ROYALE));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_MASS_RACE));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable((ShroomGameMode)-1));
  TEST_ASSERT_FALSE(ShroomGameModeIsAvailable(SHROOM_GAME_MODE_COUNT));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_capabilities_cover_every_game_mode_once);
  RUN_TEST(test_supported_modes_match_server_implementations);
  return UNITY_END();
}
