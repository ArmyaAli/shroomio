#include "unity.h"
#include "../src/shared/sim.h"

void setUp(void) {}

void tearDown(void) {}

static ShroomSporeState* newest_spore(ShroomWorldState* world) {
  TEST_ASSERT_TRUE(world->spore_count > 0u);
  return &world->spores[world->spore_count - 1u];
}

void test_eject_reduces_mass_and_spawns_spore_in_aim_direction(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;
  ShroomSporeState* spore;
  const float start_mass = SHROOM_DEFAULT_PLAYER_MASS * 3.0f;
  const size_t start_spores = SHROOM_SPORE_TARGET_COUNT;

  ShroomWorldInitWithSeed(&world, 123u);
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->position = (ShroomVec2){1000.0f, 1000.0f};
  player->mass = start_mass;
  player->radius = ShroomMassToRadius(player->mass);

  TEST_ASSERT_TRUE(ShroomWorldEjectMass(&world, player, (ShroomVec2){1.0f, 0.0f}));

  TEST_ASSERT_FLOAT_WITHIN(0.001f,
                           start_mass - (SHROOM_EJECT_MASS_VALUE *
                                         (1.0f + SHROOM_EJECT_COST_FRACTION)),
                           player->mass);
  TEST_ASSERT_EQUAL(start_spores + 1u, world.spore_count);
  spore = newest_spore(&world);
  TEST_ASSERT_TRUE(spore->active);
  TEST_ASSERT_EQUAL((uint16_t)SHROOM_EJECT_MASS_VALUE, spore->value);
  TEST_ASSERT_TRUE(spore->position.x > player->position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, player->position.y, spore->position.y);
}

void test_eject_cooldown_blocks_rapid_reuse_then_recovers(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;

  ShroomWorldInitWithSeed(&world, 456u);
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = SHROOM_DEFAULT_PLAYER_MASS * 4.0f;
  player->radius = ShroomMassToRadius(player->mass);

  TEST_ASSERT_TRUE(ShroomWorldEjectMass(&world, player, (ShroomVec2){1.0f, 0.0f}));
  TEST_ASSERT_FALSE(ShroomWorldEjectMass(&world, player, (ShroomVec2){1.0f, 0.0f}));

  ShroomWorldStep(&world, SHROOM_EJECT_COOLDOWN_SECONDS + 0.1f);
  TEST_ASSERT_TRUE(ShroomWorldEjectMass(&world, player, (ShroomVec2){1.0f, 0.0f}));
}

void test_eject_min_mass_floor_refuses_small_players(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;

  ShroomWorldInitWithSeed(&world, 789u);
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = SHROOM_EJECT_MIN_MASS - 1.0f;
  player->radius = ShroomMassToRadius(player->mass);

  TEST_ASSERT_FALSE(ShroomWorldEjectMass(&world, player, (ShroomVec2){1.0f, 0.0f}));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_eject_reduces_mass_and_spawns_spore_in_aim_direction);
  RUN_TEST(test_eject_cooldown_blocks_rapid_reuse_then_recovers);
  RUN_TEST(test_eject_min_mass_floor_refuses_small_players);
  return UNITY_END();
}
