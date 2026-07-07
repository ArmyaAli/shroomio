#include "unity.h"
#include "../src/shared/sim.h"

void setUp(void) {}

void tearDown(void) {}

void test_powerup_respawn_type_uses_world_seed_not_slot_parity(void) {
  ShroomWorldState world_a;
  ShroomWorldState world_b;
  bool differs = false;

  ShroomWorldInitWithSeed(&world_a, 1u);
  ShroomWorldInitWithSeed(&world_b, 2u);

  for (size_t index = 0; index < SHROOM_MAX_POWERUPS; ++index) {
    TEST_ASSERT_TRUE(world_a.powerups[index].type >= SHROOM_POWERUP_SPEED);
    TEST_ASSERT_TRUE(world_a.powerups[index].type <= SHROOM_POWERUP_DECAY_IMMUNE);
    if (world_a.powerups[index].type != world_b.powerups[index].type) {
      differs = true;
    }
  }
  TEST_ASSERT_TRUE(differs);
}

void test_magnet_powerup_pulls_nearby_spores_toward_player(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;
  float before;
  float after;

  ShroomWorldInitWithSeed(&world, 3u);
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->position = (ShroomVec2){1000.0f, 1000.0f};
  player->magnet_powerup_timer = SHROOM_POWERUP_MAGNET_SECONDS;
  world.spores[0].position = (ShroomVec2){1000.0f + SHROOM_POWERUP_MAGNET_RADIUS - 20.0f, 1000.0f};
  world.spores[0].active = true;

  before = ShroomDistanceSqr(player->position, world.spores[0].position);
  ShroomWorldStep(&world, 0.1f);
  after = ShroomDistanceSqr(player->position, world.spores[0].position);

  TEST_ASSERT_TRUE(after < before);
}

void test_decay_immune_powerup_suppresses_decay_and_idle_bleed(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;
  float start_mass;

  ShroomWorldInitWithSeed(&world, 4u);
  world.spore_count = 0u;
  world.powerup_count = 0u;
  world.tick = (uint64_t)(SHROOM_SERVER_TICK_RATE * (SHROOM_IDLE_PENALTY_GRACE_SECONDS + 1.0f));
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->position = (ShroomVec2){world.width * 0.5f, world.height * 0.5f};
  player->mass = SHROOM_DEFAULT_PLAYER_MASS * 10.0f;
  player->radius = ShroomMassToRadius(player->mass);
  player->last_move_time_ms = 0u;
  player->decay_immune_powerup_timer = SHROOM_POWERUP_DECAY_IMMUNE_SECONDS;
  start_mass = player->mass;

  ShroomWorldStep(&world, 1.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, start_mass, player->mass);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_powerup_respawn_type_uses_world_seed_not_slot_parity);
  RUN_TEST(test_magnet_powerup_pulls_nearby_spores_toward_player);
  RUN_TEST(test_decay_immune_powerup_suppresses_decay_and_idle_bleed);
  return UNITY_END();
}
