#include "unity.h"
#include "../src/shared/sim.h"

static ShroomWorldState world;

void setUp(void) {
  ShroomWorldInitWithSeed(&world, 13u);
  world.spore_count = 0;
  world.powerup_count = 0;
}

void tearDown(void) {}

static ShroomVec2 CenterPosition(void) {
  return (ShroomVec2){world.width * 0.5f, world.height * 0.5f};
}

static ShroomVec2 MidPosition(void) {
  return (ShroomVec2){(world.width * 0.5f) + SHROOM_ZONE_CENTER_RADIUS + 100.0f,
                      world.height * 0.5f};
}

static ShroomVec2 OuterPosition(void) { return (ShroomVec2){300.0f, 300.0f}; }

static ShroomPlayerState* SpawnDecayTestPlayer(ShroomVec2 position, float mass) {
  ShroomPlayerState* player;

  world.player_count = 0;
  world.spore_count = 0;
  world.next_entity_id = 1;
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->position = position;
  player->mass = mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->last_move_time_ms = 100000u;
  return player;
}

static float MassAfterZoneStep(ShroomVec2 position, float mass) {
  ShroomPlayerState* player = SpawnDecayTestPlayer(position, mass);

  ShroomWorldStep(&world, 10.0f);
  return player->mass;
}

void test_center_bleeds_at_two_and_half_default_mass(void) {
  const float mass = SHROOM_DEFAULT_PLAYER_MASS * 2.5f;
  const float after_mass = MassAfterZoneStep(CenterPosition(), mass);

  TEST_ASSERT_LESS_THAN_FLOAT(mass, after_mass);
}

void test_outer_does_not_bleed_at_two_and_half_default_mass(void) {
  const float mass = SHROOM_DEFAULT_PLAYER_MASS * 2.5f;
  const float after_mass = MassAfterZoneStep(OuterPosition(), mass);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, mass, after_mass);
}

void test_decay_pressure_orders_center_mid_outer_for_same_mass(void) {
  const float mass = SHROOM_DEFAULT_PLAYER_MASS * 7.0f;
  const float center_loss = mass - MassAfterZoneStep(CenterPosition(), mass);
  const float mid_loss = mass - MassAfterZoneStep(MidPosition(), mass);
  const float outer_loss = mass - MassAfterZoneStep(OuterPosition(), mass);

  TEST_ASSERT_GREATER_THAN_FLOAT(mid_loss, center_loss);
  TEST_ASSERT_GREATER_THAN_FLOAT(outer_loss, mid_loss);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, outer_loss);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_center_bleeds_at_two_and_half_default_mass);
  RUN_TEST(test_outer_does_not_bleed_at_two_and_half_default_mass);
  RUN_TEST(test_decay_pressure_orders_center_mid_outer_for_same_mass);
  return UNITY_END();
}
