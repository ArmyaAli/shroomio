#include "unity.h"

#include "client/spectator_target.h"

void setUp(void) {}
void tearDown(void) {}

static ShroomPlayerState Player(ShroomPlayerId player_id, ShroomEntityId entity_id,
                                uint8_t piece_index, bool alive) {
  return (ShroomPlayerState){
      .player_id = player_id,
      .entity_id = entity_id,
      .piece_index = piece_index,
      .alive = alive,
      .mass = 80.0f,
      .life_generation = 1u,
  };
}

static void test_next_target_cycles_stably_and_wraps(void) {
  ShroomPlayerState players[] = {
      Player(1u, 10u, 0u, true),
      Player(2u, 20u, 0u, true),
      Player(3u, 30u, 0u, true),
  };

  TEST_ASSERT_EQUAL_UINT32(30u, ShroomSpectatorSelectNextTarget(players, 3u, 20u, 0u, 0u, 1));
  TEST_ASSERT_EQUAL_UINT32(10u, ShroomSpectatorSelectNextTarget(players, 3u, 30u, 0u, 0u, 1));
  TEST_ASSERT_EQUAL_UINT32(30u, ShroomSpectatorSelectNextTarget(players, 3u, 10u, 0u, 0u, -1));
}

static void test_selection_excludes_invalid_targets_and_consumed_life(void) {
  ShroomPlayerState players[] = {
      Player(1u, 10u, 0u, true), Player(2u, 20u, 1u, true), Player(3u, 30u, 0u, false),
      Player(4u, 0u, 0u, true),  Player(5u, 50u, 0u, true),
  };

  TEST_ASSERT_EQUAL_UINT32(50u, ShroomSpectatorSelectNextTarget(players, 5u, 10u, 1u, 10u, 1));
  TEST_ASSERT_EQUAL_UINT32(0u, ShroomSpectatorSelectNextTarget(players, 5u, 50u, 1u, 50u, 1));
}

static void test_missing_target_reacquires_first_eligible_target(void) {
  ShroomPlayerState players[] = {
      Player(1u, 10u, 0u, true),
      Player(2u, 20u, 0u, true),
  };

  TEST_ASSERT_EQUAL_UINT32(20u, ShroomSpectatorSelectNextTarget(players, 2u, 0u, 1u, 0u, 1));
}

static void test_generation_change_ends_watched_life_at_any_position(void) {
  ShroomPlayerState previous = Player(2u, 20u, 0u, true);
  ShroomPlayerState current = previous;
  previous.position = (ShroomVec2){100.0f, 100.0f};
  current.position = (ShroomVec2){101.0f, 101.0f};
  current.life_generation = 2u;

  TEST_ASSERT_TRUE(ShroomSpectatorTargetLifeEnded(&previous, &current));
  current.life_generation = previous.life_generation;
  TEST_ASSERT_FALSE(ShroomSpectatorTargetLifeEnded(&previous, &current));
  previous.life_generation = UINT8_MAX;
  current.life_generation = 1u;
  TEST_ASSERT_TRUE(ShroomSpectatorTargetLifeEnded(&previous, &current));
  TEST_ASSERT_TRUE(ShroomSpectatorTargetLifeEnded(&previous, NULL));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_next_target_cycles_stably_and_wraps);
  RUN_TEST(test_selection_excludes_invalid_targets_and_consumed_life);
  RUN_TEST(test_missing_target_reacquires_first_eligible_target);
  RUN_TEST(test_generation_change_ends_watched_life_at_any_position);
  return UNITY_END();
}
