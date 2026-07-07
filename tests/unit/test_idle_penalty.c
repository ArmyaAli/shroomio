#include "unity.h"
#include "../src/shared/sim.h"

static ShroomWorldState world;

void setUp(void) {
  ShroomWorldInitWithSeed(&world, 11u);
  world.spore_count = 0;
  world.powerup_count = 0;
}

void tearDown(void) {}

static ShroomPlayerState* SpawnIdleTestPlayer(float mass) {
  ShroomPlayerState* player;

  world.player_count = 0;
  world.next_entity_id = 1;
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->position = (ShroomVec2){300.0f, 300.0f};
  player->mass = mass;
  player->radius = ShroomMassToRadius(player->mass);
  return player;
}

void test_stationary_high_mass_player_bleeds_after_grace(void) {
  ShroomPlayerState* player = SpawnIdleTestPlayer(SHROOM_DEFAULT_PLAYER_MASS * 4.0f);
  const float before_mass = player->mass;

  ShroomWorldStep(&world, SHROOM_IDLE_PENALTY_GRACE_SECONDS + 1.0f);

  TEST_ASSERT_LESS_THAN_FLOAT(before_mass, player->mass);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, player->decay_spore_accumulator);
}

void test_moving_player_does_not_bleed_after_grace(void) {
  ShroomPlayerState* player = SpawnIdleTestPlayer(SHROOM_DEFAULT_PLAYER_MASS * 4.0f);
  const float before_mass = player->mass;

  ShroomPlayerSetInput(player, (ShroomVec2){2.0f, 0.0f});
  ShroomWorldStep(&world, SHROOM_IDLE_PENALTY_GRACE_SECONDS + 1.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, before_mass, player->mass);
}

void test_penalty_stops_when_motion_resumes(void) {
  ShroomPlayerState* player = SpawnIdleTestPlayer(SHROOM_DEFAULT_PLAYER_MASS * 4.0f);
  float after_idle_mass;

  ShroomWorldStep(&world, SHROOM_IDLE_PENALTY_GRACE_SECONDS + 1.0f);
  after_idle_mass = player->mass;
  ShroomPlayerSetInput(player, (ShroomVec2){2.0f, 0.0f});
  ShroomWorldStep(&world, 1.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, after_idle_mass, player->mass);
}

void test_idle_penalty_applies_below_decay_threshold(void) {
  ShroomPlayerState* player = SpawnIdleTestPlayer(SHROOM_DEFAULT_PLAYER_MASS);
  const float before_mass = player->mass;

  ShroomWorldStep(&world, SHROOM_IDLE_PENALTY_GRACE_SECONDS + 1.0f);

  TEST_ASSERT_LESS_THAN_FLOAT(before_mass, player->mass);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_stationary_high_mass_player_bleeds_after_grace);
  RUN_TEST(test_moving_player_does_not_bleed_after_grace);
  RUN_TEST(test_penalty_stops_when_motion_resumes);
  RUN_TEST(test_idle_penalty_applies_below_decay_threshold);
  return UNITY_END();
}
