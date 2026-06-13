#include "unity.h"
#include "../src/shared/sim.h"

static ShroomWorldState world;

void setUp(void) { ShroomWorldInitWithSeed(&world, 7u); }

void tearDown(void) {}

static void ResetWorldForPlayers(void) {
  world.tick = 0;
  world.player_count = 0;
  world.spore_count = 0;
  world.next_entity_id = 1;
}

void test_world_init_sets_expected_defaults(void) {
  TEST_ASSERT_EQUAL_UINT64(0, world.tick);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_WORLD_WIDTH, world.width);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_WORLD_HEIGHT, world.height);
  TEST_ASSERT_EQUAL_UINT32(7u, world.random_seed);
  TEST_ASSERT_EQUAL_size_t(SHROOM_SPORE_TARGET_COUNT, world.spore_count);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(SHROOM_SPORE_TARGET_COUNT + 1), world.next_entity_id);
}

void test_world_init_with_seed_repeats_same_layout(void) {
  ShroomWorldState a;
  ShroomWorldState b;

  ShroomWorldInitWithSeed(&a, 1234u);
  ShroomWorldInitWithSeed(&b, 1234u);

  TEST_ASSERT_EQUAL_UINT32(a.random_seed, b.random_seed);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, a.spores[0].position.x, b.spores[0].position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, a.spores[0].position.y, b.spores[0].position.y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, a.spores[1].position.x, b.spores[1].position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, a.spores[1].position.y, b.spores[1].position.y);
}

void test_world_init_with_different_seed_changes_layout(void) {
  ShroomWorldState a;
  ShroomWorldState b;

  ShroomWorldInitWithSeed(&a, 1234u);
  ShroomWorldInitWithSeed(&b, 5678u);

  TEST_ASSERT_NOT_EQUAL_UINT32(a.random_seed, b.random_seed);
  TEST_ASSERT_TRUE((a.spores[0].position.x != b.spores[0].position.x) ||
                   (a.spores[0].position.y != b.spores[0].position.y));
}

void test_world_init_populates_active_spores_with_value(void) {
  for (size_t index = 0; index < world.spore_count; ++index) {
    TEST_ASSERT_TRUE(world.spores[index].active);
    TEST_ASSERT_EQUAL_UINT16(SHROOM_SPORE_VALUE, world.spores[index].value);
    TEST_ASSERT_NOT_EQUAL(0, world.spores[index].entity_id);
  }
}

void test_zone_classification_matches_center_mid_and_outer(void) {
  const ShroomVec2 center = {world.width * 0.5f, world.height * 0.5f};

  TEST_ASSERT_EQUAL(SHROOM_ZONE_CENTER, ShroomGetZoneAtPosition(&world, center));
  TEST_ASSERT_EQUAL(
      SHROOM_ZONE_MID,
      ShroomGetZoneAtPosition(&world, (ShroomVec2){center.x + SHROOM_ZONE_CENTER_RADIUS + 10.0f, center.y}));
  TEST_ASSERT_EQUAL(
      SHROOM_ZONE_OUTER,
      ShroomGetZoneAtPosition(&world, (ShroomVec2){center.x + SHROOM_ZONE_MID_RADIUS + 10.0f, center.y}));
}

void test_mass_helpers_respect_expected_scaling_and_bounds(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.0f, ShroomMassToRadius(100.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_MAX_PLAYER_SPEED, ShroomMassToSpeed(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_MIN_PLAYER_SPEED, ShroomMassToSpeed(100000.0f));
  TEST_ASSERT_TRUE(ShroomMassToSpeed(100.0f) > ShroomMassToSpeed(500.0f));
}

void test_spawn_player_prefers_safe_outer_spawn(void) {
  ShroomPlayerState* first;
  ShroomPlayerState* second;
  const float min_distance =
      ShroomMassToRadius(SHROOM_DEFAULT_PLAYER_MASS) + SHROOM_SPAWN_SAFE_DISTANCE;

  ResetWorldForPlayers();

  first = ShroomWorldSpawnPlayer(&world, 1, false);
  second = ShroomWorldSpawnPlayer(&world, 2, false);

  TEST_ASSERT_NOT_NULL(first);
  TEST_ASSERT_NOT_NULL(second);
  TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER, ShroomGetZoneAtPosition(&world, first->position));
  TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER, ShroomGetZoneAtPosition(&world, second->position));
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(
      min_distance * min_distance,
      ShroomDistanceSqr(first->position, second->position));
}

void test_world_step_moves_player_and_increments_tick(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){2000.0f, 2000.0f};
  ShroomPlayerSetInput(player, (ShroomVec2){1.0f, 0.0f});
  ShroomWorldStep(&world, 1.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f + ShroomMassToSpeed(SHROOM_DEFAULT_PLAYER_MASS),
                           player->position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 2000.0f, player->position.y);
  TEST_ASSERT_EQUAL_UINT64(1, world.tick);
}

void test_world_step_clamps_player_to_world_bounds(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){player->radius, player->radius};
  ShroomPlayerSetInput(player, (ShroomVec2){-1.0f, -1.0f});
  ShroomWorldStep(&world, 1.0f);

  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(player->radius, player->position.x);
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(player->radius, player->position.y);
}

void test_world_step_collects_spores_and_gains_mass(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  world.spore_count = 1;
  world.next_entity_id = player->entity_id + 1;
  world.spores[0] = (ShroomSporeState){
      .entity_id = world.next_entity_id++,
      .position = player->position,
      .value = SHROOM_SPORE_VALUE,
      .active = true,
  };

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS + SHROOM_SPORE_VALUE, player->mass);
  TEST_ASSERT_TRUE(world.spores[0].active);
  TEST_ASSERT_EQUAL_UINT16(SHROOM_SPORE_VALUE, world.spores[0].value);
}

void test_world_step_consumes_player_when_mass_advantage_and_overlap_match(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;
  const float expected_gain = 100.0f * SHROOM_CONSUME_MASS_GAIN_FACTOR;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  attacker->mass = 140.0f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){3000.0f, 3000.0f};
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = attacker->position;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 140.0f + expected_gain, attacker->mass);
  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
  TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER, ShroomGetZoneAtPosition(&world, victim->position));
  TEST_ASSERT_GREATER_THAN_FLOAT(
      (victim->radius + SHROOM_SPAWN_SAFE_DISTANCE) * (victim->radius + SHROOM_SPAWN_SAFE_DISTANCE),
      ShroomDistanceSqr(attacker->position, victim->position));
}

void test_world_step_does_not_consume_without_required_mass_advantage(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  attacker->mass = 114.0f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){3000.0f, 3000.0f};
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = attacker->position;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 114.0f, attacker->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, victim->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3000.0f, victim->position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3000.0f, victim->position.y);
}

void test_small_bot_prefers_safer_spore_over_slightly_closer_center_spore(void) {
  ShroomPlayerState* bot;

  ResetWorldForPlayers();

  bot = ShroomWorldSpawnPlayer(&world, 1, true);
  TEST_ASSERT_NOT_NULL(bot);

  bot->mass = SHROOM_DEFAULT_PLAYER_MASS;
  bot->radius = ShroomMassToRadius(bot->mass);
  bot->position = (ShroomVec2){3950.0f, 3000.0f};
  world.spore_count = 2;
  world.spores[0] =
      (ShroomSporeState){.entity_id = 1, .position = {3880.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE,
                         .active = true};
  world.spores[1] =
      (ShroomSporeState){.entity_id = 2, .position = {4035.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE,
                         .active = true};

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_TRUE(bot->input_direction.x > 0.0f);
}

void test_large_bot_prefers_center_pressure_spore_choice(void) {
  ShroomPlayerState* bot;

  ResetWorldForPlayers();

  bot = ShroomWorldSpawnPlayer(&world, 1, true);
  TEST_ASSERT_NOT_NULL(bot);

  bot->mass = SHROOM_DEFAULT_PLAYER_MASS * 2.2f;
  bot->radius = ShroomMassToRadius(bot->mass);
  bot->position = (ShroomVec2){3950.0f, 3000.0f};
  world.spore_count = 2;
  world.spores[0] =
      (ShroomSporeState){.entity_id = 1, .position = {3880.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE,
                         .active = true};
  world.spores[1] =
      (ShroomSporeState){.entity_id = 2, .position = {4035.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE,
                         .active = true};

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_TRUE(bot->input_direction.x < 0.0f);
}

void test_bot_flees_nearby_threat_even_with_available_prey(void) {
  ShroomPlayerState* bot;
  ShroomPlayerState* threat;
  ShroomPlayerState* prey;

  ResetWorldForPlayers();

  bot = ShroomWorldSpawnPlayer(&world, 1, true);
  threat = ShroomWorldSpawnPlayer(&world, 2, false);
  prey = ShroomWorldSpawnPlayer(&world, 3, false);
  TEST_ASSERT_NOT_NULL(bot);
  TEST_ASSERT_NOT_NULL(threat);
  TEST_ASSERT_NOT_NULL(prey);

  bot->mass = SHROOM_DEFAULT_PLAYER_MASS;
  bot->radius = ShroomMassToRadius(bot->mass);
  bot->position = (ShroomVec2){3000.0f, 3000.0f};
  threat->mass = SHROOM_DEFAULT_PLAYER_MASS * 2.0f;
  threat->radius = ShroomMassToRadius(threat->mass);
  threat->position = (ShroomVec2){3200.0f, 3000.0f};
  prey->mass = SHROOM_DEFAULT_PLAYER_MASS * 0.5f;
  prey->radius = ShroomMassToRadius(prey->mass);
  prey->position = (ShroomVec2){2850.0f, 3000.0f};
  world.spore_count = 0;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_TRUE(bot->input_direction.x < 0.0f);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_world_init_sets_expected_defaults);
  RUN_TEST(test_world_init_with_seed_repeats_same_layout);
  RUN_TEST(test_world_init_with_different_seed_changes_layout);
  RUN_TEST(test_world_init_populates_active_spores_with_value);
  RUN_TEST(test_zone_classification_matches_center_mid_and_outer);
  RUN_TEST(test_mass_helpers_respect_expected_scaling_and_bounds);
  RUN_TEST(test_spawn_player_prefers_safe_outer_spawn);
  RUN_TEST(test_world_step_moves_player_and_increments_tick);
  RUN_TEST(test_world_step_clamps_player_to_world_bounds);
  RUN_TEST(test_world_step_collects_spores_and_gains_mass);
  RUN_TEST(test_world_step_consumes_player_when_mass_advantage_and_overlap_match);
  RUN_TEST(test_world_step_does_not_consume_without_required_mass_advantage);
  RUN_TEST(test_small_bot_prefers_safer_spore_over_slightly_closer_center_spore);
  RUN_TEST(test_large_bot_prefers_center_pressure_spore_choice);
  RUN_TEST(test_bot_flees_nearby_threat_even_with_available_prey);
  return UNITY_END();
}
