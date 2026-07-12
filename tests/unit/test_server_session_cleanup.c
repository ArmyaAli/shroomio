#include "unity.h"
#include "server/session_cleanup.h"

static ShroomWorldState world;

void setUp(void) {
  ShroomWorldInitWithSeed(&world, 438u);
  world.player_count = 0u;
  world.spore_count = 0u;
  world.powerup_count = 0u;
  world.next_entity_id = 1u;
}

void tearDown(void) {}

void test_disconnect_removes_maximum_split_colony_and_clears_session_focus(void) {
  ShroomPlayerState* primary = ShroomWorldSpawnPlayer(&world, 71u, false);
  ShroomPlayerState* other = ShroomWorldSpawnPlayer(&world, 72u, true);
  uint32_t focused_entity_id;
  uint32_t other_entity_id;

  TEST_ASSERT_NOT_NULL(primary);
  TEST_ASSERT_NOT_NULL(other);
  other_entity_id = other->entity_id;
  primary->mass = SHROOM_SPLIT_MIN_MASS * 16.0f;
  primary->radius = ShroomMassToRadius(primary->mass);
  for (size_t index = 1u; index < SHROOM_MAX_SPLIT_PIECES; ++index) {
    TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, primary));
    primary->has_split = false;
  }
  focused_entity_id = world.players[world.player_count - 1u].entity_id;

  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_SPLIT_PIECES,
                           ShroomServerCleanupPlayer(&world, 71u, &primary, &focused_entity_id));
  TEST_ASSERT_NULL(primary);
  TEST_ASSERT_EQUAL_UINT32(0u, focused_entity_id);

  for (size_t index = 0u; index < world.player_count; ++index) {
    TEST_ASSERT_FALSE(world.players[index].alive && (world.players[index].player_id == 71u));
  }
  TEST_ASSERT_TRUE(other->alive);
  TEST_ASSERT_EQUAL_UINT32(72u, other->player_id);
  TEST_ASSERT_EQUAL_UINT32(other_entity_id, other->entity_id);
  TEST_ASSERT_NOT_NULL(ShroomWorldSpawnPlayer(&world, 73u, false));
}

void test_cleanup_clears_stale_references_when_world_is_unavailable(void) {
  ShroomPlayerState placeholder = {0};
  ShroomPlayerState* primary = &placeholder;
  uint32_t focused_entity_id = 99u;

  TEST_ASSERT_EQUAL_size_t(0u, ShroomServerCleanupPlayer(NULL, 71u, &primary, &focused_entity_id));
  TEST_ASSERT_NULL(primary);
  TEST_ASSERT_EQUAL_UINT32(0u, focused_entity_id);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_disconnect_removes_maximum_split_colony_and_clears_session_focus);
  RUN_TEST(test_cleanup_clears_stale_references_when_world_is_unavailable);
  return UNITY_END();
}
