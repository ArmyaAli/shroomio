#include "unity.h"
#include "../src/shared/sim.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

static ShroomWorldState world;

void setUp(void) { ShroomWorldInitWithSeed(&world, 7u); }

void tearDown(void) {}

static void ResetWorldForPlayers(void) {
  world.tick = 0;
  world.player_count = 0;
  world.spore_count = 0;
  world.powerup_count = 0;
  world.next_entity_id = 1;
}

static size_t CountAlivePieces(ShroomPlayerId player_id) {
  size_t count = 0;

  for (size_t index = 0; index < world.player_count; ++index) {
    const ShroomPlayerState* player = &world.players[index];

    if (player->alive && (player->player_id == player_id)) {
      count += 1;
    }
  }

  return count;
}

static ShroomPlayerState* FindSplitPiece(ShroomPlayerId player_id) {
  for (size_t index = 0; index < world.player_count; ++index) {
    ShroomPlayerState* player = &world.players[index];

    if (player->alive && (player->player_id == player_id) && (player->piece_index > 0)) {
      return player;
    }
  }

  return NULL;
}

static float SplitRequiredPreCostMass(void) { return SHROOM_SPLIT_MIN_MASS; }

void test_world_init_sets_expected_defaults(void) {
  TEST_ASSERT_EQUAL_UINT64(0, world.tick);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_WORLD_WIDTH, world.width);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_WORLD_HEIGHT, world.height);
  TEST_ASSERT_EQUAL_UINT32(7u, world.random_seed);
  TEST_ASSERT_EQUAL_size_t(SHROOM_SPORE_TARGET_COUNT, world.spore_count);
  TEST_ASSERT_EQUAL_UINT32((uint32_t)(SHROOM_SPORE_TARGET_COUNT + SHROOM_MAX_POWERUPS + 1),
                           world.next_entity_id);
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
  size_t outer_count = 0;
  size_t center_count = 0;

  for (size_t index = 0; index < world.spore_count; ++index) {
    TEST_ASSERT_TRUE(world.spores[index].active);
    TEST_ASSERT_EQUAL_UINT16(SHROOM_SPORE_VALUE, world.spores[index].value);
    TEST_ASSERT_NOT_EQUAL(0, world.spores[index].entity_id);
    if (ShroomGetZoneAtPosition(&world, world.spores[index].position) == SHROOM_ZONE_CENTER) {
      center_count += 1;
    }
    if (ShroomGetZoneAtPosition(&world, world.spores[index].position) == SHROOM_ZONE_OUTER) {
      outer_count += 1;
    }
  }

  TEST_ASSERT_TRUE(center_count > outer_count);
}

void test_world_init_populates_powerups_across_supported_types(void) {
  size_t speed_count = 0;
  size_t shield_count = 0;

  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_POWERUPS, world.powerup_count);

  for (size_t index = 0; index < world.powerup_count; ++index) {
    TEST_ASSERT_TRUE(world.powerups[index].active);
    TEST_ASSERT_NOT_EQUAL(0, world.powerups[index].entity_id);
    if (world.powerups[index].type == SHROOM_POWERUP_SPEED) {
      speed_count += 1;
    }
    if (world.powerups[index].type == SHROOM_POWERUP_SHIELD) {
      shield_count += 1;
    }
  }

  TEST_ASSERT_GREATER_THAN_size_t(0, speed_count);
  TEST_ASSERT_GREATER_THAN_size_t(0, shield_count);
}

void test_zone_classification_matches_center_mid_and_outer(void) {
  const ShroomVec2 center = {world.width * 0.5f, world.height * 0.5f};

  TEST_ASSERT_EQUAL(SHROOM_ZONE_CENTER, ShroomGetZoneAtPosition(&world, center));
  TEST_ASSERT_EQUAL(
      SHROOM_ZONE_MID,
      ShroomGetZoneAtPosition(
          &world, (ShroomVec2){center.x + SHROOM_ZONE_CENTER_RADIUS + 10.0f, center.y}));
  TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER,
                    ShroomGetZoneAtPosition(
                        &world, (ShroomVec2){center.x + SHROOM_ZONE_MID_RADIUS + 10.0f, center.y}));
}

void test_mass_helpers_respect_expected_scaling_and_bounds(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.0f, ShroomMassToRadius(100.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_MAX_PLAYER_SPEED, ShroomMassToSpeed(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_MIN_PLAYER_SPEED * SHROOM_SPEED_FLOOR_FACTOR,
                           ShroomMassToSpeed(100000.0f));
  TEST_ASSERT_TRUE(ShroomMassToSpeed(100.0f) > ShroomMassToSpeed(500.0f));
  TEST_ASSERT_TRUE(ShroomMassToSpeed(SHROOM_DECAY_MASS_THRESHOLD * 1.5f) < SHROOM_MIN_PLAYER_SPEED);
}

void test_human_spawn_protection_blocks_consumption_then_expires(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;
  const float attacker_mass = SHROOM_DEFAULT_PLAYER_MASS * 2.0f;
  const float expected_gain = SHROOM_DEFAULT_PLAYER_MASS * SHROOM_CONSUME_MASS_GAIN_FACTOR;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  attacker->spawn_protection_timer = 0.0f;
  attacker->mass = attacker_mass;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){800.0f, 800.0f};
  victim->position = attacker->position;

  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS,
                           victim->spawn_protection_timer);
  ShroomWorldStep(&world, SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS - 0.01f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, attacker_mass, attacker->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, victim->spawn_protection_timer);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.0f, victim->position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 800.0f, victim->position.y);

  ShroomWorldStep(&world, 0.01f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, attacker_mass + expected_gain, attacker->mass);
  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS,
                           victim->spawn_protection_timer);
  TEST_ASSERT_TRUE((victim->position.x != 800.0f) || (victim->position.y != 800.0f));
}

void test_bot_spawn_keeps_existing_unprotected_behavior(void) {
  ShroomPlayerState* bot;

  ResetWorldForPlayers();

  bot = ShroomWorldSpawnPlayer(&world, 1, true);
  TEST_ASSERT_NOT_NULL(bot);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bot->spawn_protection_timer);
  TEST_ASSERT_FALSE(ShroomPlayerHasConsumeProtection(bot));
}

void test_consume_predicate_respects_mass_zone_and_protection_boundaries(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);
  victim->spawn_protection_timer = 0.0f;

  attacker->position = (ShroomVec2){300.0f, 300.0f};
  victim->position = attacker->position;
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);

  attacker->mass = (victim->mass * SHROOM_CONSUME_MASS_ADVANTAGE) - 0.1f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  TEST_ASSERT_FALSE(ShroomPlayerCanConsume(&world, attacker, victim));

  victim->position = (ShroomVec2){world.width * 0.5f, world.height * 0.5f};
  attacker->position = victim->position;
  attacker->mass = victim->mass * SHROOM_CENTER_CONSUME_ADVANTAGE;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  TEST_ASSERT_TRUE(ShroomPlayerCanConsume(&world, attacker, victim));

  victim->shield_powerup_timer = 1.0f;
  TEST_ASSERT_TRUE(ShroomPlayerHasConsumeProtection(victim));
  TEST_ASSERT_FALSE(ShroomPlayerCanConsume(&world, attacker, victim));

  victim->shield_powerup_timer = 0.0f;
  victim->spawn_protection_timer = 1.0f;
  TEST_ASSERT_TRUE(ShroomPlayerHasConsumeProtection(victim));
  TEST_ASSERT_FALSE(ShroomPlayerCanConsume(&world, attacker, victim));
}

void test_decay_predicate_respects_zone_threshold_boundaries(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){300.0f, 300.0f};
  player->mass = SHROOM_DECAY_MASS_THRESHOLD + 100.0f;
  TEST_ASSERT_FALSE(ShroomPlayerCanDecay(&world, player));

  player->position = (ShroomVec2){world.width * 0.5f, world.height * 0.5f};
  player->mass = (SHROOM_DEFAULT_PLAYER_MASS * 2.0f) + 0.1f;
  TEST_ASSERT_TRUE(ShroomPlayerCanDecay(&world, player));

  player->position =
      (ShroomVec2){(world.width * 0.5f) + SHROOM_ZONE_CENTER_RADIUS + 20.0f, world.height * 0.5f};
  player->mass = SHROOM_DECAY_MASS_THRESHOLD;
  TEST_ASSERT_FALSE(ShroomPlayerCanDecay(&world, player));
  player->mass = SHROOM_DECAY_MASS_THRESHOLD + 0.1f;
  TEST_ASSERT_TRUE(ShroomPlayerCanDecay(&world, player));
}

void test_split_predicate_respects_mass_life_and_piece_boundaries(void) {
  ShroomPlayerState* player;
  int split_count;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->mass = SplitRequiredPreCostMass() - 0.1f;
  TEST_ASSERT_FALSE(ShroomPlayerCanSplit(&world, player));

  player->mass = SplitRequiredPreCostMass();
  TEST_ASSERT_TRUE(ShroomPlayerCanSplit(&world, player));

  player->has_split = true;
  TEST_ASSERT_FALSE(ShroomPlayerCanSplit(&world, player));

  player->is_bot = true;
  TEST_ASSERT_TRUE(ShroomPlayerCanSplit(&world, player));

  player->has_split = false;
  player->is_bot = false;
  player->mass = SHROOM_SPLIT_MIN_MASS * 8.0f;
  player->radius = ShroomMassToRadius(player->mass);
  for (split_count = 1; split_count < SHROOM_MAX_SPLIT_PIECES; ++split_count) {
    TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
    player->has_split = false;
  }
  TEST_ASSERT_FALSE(ShroomPlayerCanSplit(&world, player));
}

void test_merge_predicate_respects_cooldown_proximity_and_identity(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = SplitRequiredPreCostMass();
  player->radius = ShroomMassToRadius(player->mass);
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);

  player->position = (ShroomVec2){1000.0f, 1000.0f};
  piece->position = player->position;
  TEST_ASSERT_FALSE(ShroomPlayersCanMerge(player, piece));

  player->merge_timer = 0.0f;
  piece->merge_timer = 0.0f;
  piece->position = (ShroomVec2){3000.0f, 3000.0f};
  TEST_ASSERT_FALSE(ShroomPlayersCanMerge(player, piece));

  piece->position = player->position;
  TEST_ASSERT_TRUE(ShroomPlayersCanMerge(player, piece));

  piece->player_id = 2;
  TEST_ASSERT_FALSE(ShroomPlayersCanMerge(player, piece));
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
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(min_distance * min_distance,
                                     ShroomDistanceSqr(first->position, second->position));
}

static float DistanceFromWorldCenter(const ShroomWorldState* test_world, ShroomVec2 position) {
  const ShroomVec2 center = {test_world->width * 0.5f, test_world->height * 0.5f};

  return sqrtf(ShroomDistanceSqr(center, position));
}

void test_spawn_density_tightens_with_large_lobbies(void) {
  ShroomPlayerState* player;
  float largest_late_spawn_radius = 0.0f;

  ResetWorldForPlayers();

  for (size_t index = 0; index < 200u; ++index) {
    player = ShroomWorldSpawnPlayer(&world, (ShroomPlayerId)(index + 1u), false);
    TEST_ASSERT_NOT_NULL(player);
    TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER, ShroomGetZoneAtPosition(&world, player->position));

    if (index >= SHROOM_SPAWN_MEDIUM_LOBBY_PLAYERS) {
      largest_late_spawn_radius =
          fmaxf(largest_late_spawn_radius, DistanceFromWorldCenter(&world, player->position));
    }
  }

  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(2500.0f, largest_late_spawn_radius);
}

void test_spawn_distribution_avoids_player_overlap_in_crowded_lobbies(void) {
  ResetWorldForPlayers();

  for (size_t index = 0; index < SHROOM_SPAWN_MEDIUM_LOBBY_PLAYERS; ++index) {
    TEST_ASSERT_NOT_NULL(ShroomWorldSpawnPlayer(&world, (ShroomPlayerId)(index + 1u), false));
  }

  for (size_t i = 0; i < world.player_count; ++i) {
    for (size_t j = i + 1u; j < world.player_count; ++j) {
      const ShroomPlayerState* a = &world.players[i];
      const ShroomPlayerState* b = &world.players[j];
      const float min_distance = a->radius + b->radius;

      TEST_ASSERT_GREATER_THAN_FLOAT(min_distance * min_distance,
                                     ShroomDistanceSqr(a->position, b->position));
    }
  }
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

void test_world_step_collects_speed_powerup_and_expires_effect(void) {
  ShroomPlayerState* player;
  const float start_x = 2000.0f;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){start_x, 2000.0f};
  world.powerup_count = 1;
  world.powerups[0] = (ShroomPowerupState){
      .entity_id = 10,
      .position = player->position,
      .type = SHROOM_POWERUP_SPEED,
      .active = true,
  };

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FALSE(world.powerups[0].active);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_POWERUP_SPEED_SECONDS, player->speed_powerup_timer);

  ShroomPlayerSetInput(player, (ShroomVec2){1.0f, 0.0f});
  ShroomWorldStep(&world, 1.0f);

  TEST_ASSERT_FLOAT_WITHIN(
      0.01f, start_x + (ShroomMassToSpeed(player->mass) * SHROOM_POWERUP_SPEED_MULTIPLIER),
      player->position.x);

  ShroomWorldStep(&world, SHROOM_POWERUP_SPEED_SECONDS);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, player->speed_powerup_timer);
}

void test_mass_shield_powerup_blocks_consumption_until_expired(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  attacker->mass = 160.0f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){3000.0f, 3000.0f};
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = attacker->position;
  victim->spawn_protection_timer = 0.0f;
  victim->shield_powerup_timer = SHROOM_POWERUP_SHIELD_SECONDS;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, victim->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3000.0f, victim->position.x);

  victim->shield_powerup_timer = 0.0f;
  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
  TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER, ShroomGetZoneAtPosition(&world, victim->position));
}

void test_powerup_respawns_after_timer(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  world.powerup_count = 1;
  world.powerups[0] = (ShroomPowerupState){
      .entity_id = 10,
      .position = player->position,
      .type = SHROOM_POWERUP_SHIELD,
      .active = true,
  };

  ShroomWorldStep(&world, 0.0f);
  TEST_ASSERT_FALSE(world.powerups[0].active);

  ShroomWorldStep(&world, SHROOM_POWERUP_RESPAWN_SECONDS - 0.1f);
  TEST_ASSERT_FALSE(world.powerups[0].active);

  ShroomWorldStep(&world, 0.2f);
  TEST_ASSERT_TRUE(world.powerups[0].active);
  TEST_ASSERT_EQUAL_UINT32(10u, world.powerups[0].entity_id);
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
  victim->spawn_protection_timer = 0.0f;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 140.0f + expected_gain, attacker->mass);
  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
  TEST_ASSERT_EQUAL(SHROOM_ZONE_OUTER, ShroomGetZoneAtPosition(&world, victim->position));
  TEST_ASSERT_GREATER_THAN_FLOAT((victim->radius + SHROOM_SPAWN_SAFE_DISTANCE) *
                                     (victim->radius + SHROOM_SPAWN_SAFE_DISTANCE),
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
  attacker->position = (ShroomVec2){300.0f, 300.0f};
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = attacker->position;
  victim->spawn_protection_timer = 0.0f;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 114.0f, attacker->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, victim->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 300.0f, victim->position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 300.0f, victim->position.y);
}

void test_center_zone_uses_lower_consume_advantage(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  attacker->mass = 108.5f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){world.width * 0.5f, world.height * 0.5f};
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = attacker->position;
  victim->spawn_protection_timer = 0.0f;

  TEST_ASSERT_TRUE(attacker->mass < (victim->mass * SHROOM_CONSUME_MASS_ADVANTAGE));
  TEST_ASSERT_TRUE(attacker->mass >= (victim->mass * SHROOM_CENTER_CONSUME_ADVANTAGE));

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
}

void test_spawn_player_reuses_inactive_slot(void) {
  ShroomPlayerState* first;
  ShroomPlayerState* second;

  ResetWorldForPlayers();

  first = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(first);
  first->alive = false;
  first->mass = 0.0f;
  first->radius = 0.0f;

  second = ShroomWorldSpawnPlayer(&world, 2, true);

  TEST_ASSERT_NOT_NULL(second);
  TEST_ASSERT_EQUAL_PTR(first, second);
  TEST_ASSERT_EQUAL_size_t(1, world.player_count);
  TEST_ASSERT_TRUE(second->alive);
  TEST_ASSERT_TRUE(second->is_bot);
  TEST_ASSERT_EQUAL_UINT32(2u, second->player_id);
}

void test_world_step_caps_mass_gain_at_configured_maximum(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  /* All players (including bots) are now force-split at SHROOM_SPLIT_MASS_THRESHOLD.
   * Collect a spore that pushes a bot to the mass cap, then verify it was
   * capped at SHROOM_MAX_PLAYER_MASS and immediately halved by forced split. */
  player = ShroomWorldSpawnPlayer(&world, 1, true);
  TEST_ASSERT_NOT_NULL(player);

  player->mass = SHROOM_MAX_PLAYER_MASS - 1.0f;
  player->radius = ShroomMassToRadius(player->mass);
  world.spore_count = 1;
  world.spores[0] = (ShroomSporeState){
      .entity_id = 1,
      .position = player->position,
      .value = SHROOM_SPORE_VALUE,
      .active = true,
  };

  ShroomWorldStep(&world, 0.0f);

  /* Mass was capped at SHROOM_MAX_PLAYER_MASS, then halved by forced split,
   * with SHROOM_SPLIT_MASS_LOSS_FRACTION deducted as the cost. */
  TEST_ASSERT_FLOAT_WITHIN(0.001f,
                           SHROOM_MAX_PLAYER_MASS * (1.0f - SHROOM_SPLIT_MASS_LOSS_FRACTION) / 2.0f,
                           player->mass);
  TEST_ASSERT_EQUAL(2, world.player_count);
}

void test_player_can_voluntarily_split_at_large_colony_threshold(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;
  const float starting_mass = SplitRequiredPreCostMass();

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->mass = starting_mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));

  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);
  TEST_ASSERT_EQUAL_size_t(2, CountAlivePieces(player->player_id));
  TEST_ASSERT_NOT_EQUAL_UINT32(player->entity_id, piece->entity_id);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, starting_mass * (1.0f - SHROOM_SPLIT_MASS_LOSS_FRACTION),
                           player->mass + piece->mass);
  TEST_ASSERT_FALSE(piece->ai_controlled);
}

void test_remove_player_clears_maximum_colony_and_reuses_capacity(void) {
  ShroomPlayerState* departing;
  ShroomPlayerState* survivor;
  ShroomPlayerState* replacement;
  uint32_t survivor_entity_id;
  size_t split_count;

  ResetWorldForPlayers();
  departing = ShroomWorldSpawnPlayer(&world, 41u, false);
  survivor = ShroomWorldSpawnPlayer(&world, 42u, true);
  TEST_ASSERT_NOT_NULL(departing);
  TEST_ASSERT_NOT_NULL(survivor);
  survivor_entity_id = survivor->entity_id;

  departing->mass = SHROOM_SPLIT_MIN_MASS * 16.0f;
  departing->radius = ShroomMassToRadius(departing->mass);
  for (split_count = 1u; split_count < SHROOM_MAX_SPLIT_PIECES; ++split_count) {
    TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, departing));
    departing->has_split = false;
  }
  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_SPLIT_PIECES, CountAlivePieces(41u));

  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_SPLIT_PIECES, ShroomWorldRemovePlayer(&world, 41u));
  TEST_ASSERT_EQUAL_size_t(0u, CountAlivePieces(41u));
  TEST_ASSERT_TRUE(survivor->alive);
  TEST_ASSERT_EQUAL_UINT32(42u, survivor->player_id);
  TEST_ASSERT_EQUAL_UINT32(survivor_entity_id, survivor->entity_id);

  replacement = ShroomWorldSpawnPlayer(&world, 43u, false);
  TEST_ASSERT_NOT_NULL(replacement);
  TEST_ASSERT_TRUE(replacement->alive);
  TEST_ASSERT_EQUAL_UINT32(43u, replacement->player_id);
}

void test_remove_player_rejects_invalid_owner_without_mutating_world(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();
  player = ShroomWorldSpawnPlayer(&world, 7u, false);
  TEST_ASSERT_NOT_NULL(player);

  TEST_ASSERT_EQUAL_size_t(0u, ShroomWorldRemovePlayer(NULL, 7u));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomWorldRemovePlayer(&world, 0u));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomWorldRemovePlayer(&world, 99u));
  TEST_ASSERT_TRUE(player->alive);
  TEST_ASSERT_EQUAL_UINT32(7u, player->player_id);
}

void test_player_split_uses_requested_aim_direction(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->mass = SplitRequiredPreCostMass();
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayerToward(&world, player, (ShroomVec2){0.0f, -3.0f}));

  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, piece->split_velocity.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -SHROOM_SPLIT_IMPULSE_SPEED, piece->split_velocity.y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, piece->input_direction.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, piece->input_direction.y);
}

void test_player_cannot_voluntarily_split_below_large_colony_threshold(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->mass = SplitRequiredPreCostMass() - 1.0f;
  player->radius = ShroomMassToRadius(player->mass);

  TEST_ASSERT_FALSE(ShroomWorldSplitPlayer(&world, player));
  TEST_ASSERT_EQUAL_size_t(1, CountAlivePieces(player->player_id));
}

void test_split_pieces_wait_to_merge_until_cooldown_and_proximity(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->mass = SplitRequiredPreCostMass();
  player->radius = ShroomMassToRadius(player->mass);
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);

  player->merge_timer = 0.0f;
  piece->merge_timer = 0.0f;
  player->position = (ShroomVec2){1000.0f, 1000.0f};
  piece->position = (ShroomVec2){3000.0f, 3000.0f};

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_EQUAL_size_t(2, CountAlivePieces(player->player_id));

  piece->position = player->position;
  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_EQUAL_size_t(1, CountAlivePieces(player->player_id));
  TEST_ASSERT_FLOAT_WITHIN(
      0.001f, SplitRequiredPreCostMass() * (1.0f - SHROOM_SPLIT_MASS_LOSS_FRACTION), player->mass);
}

void test_consuming_primary_clears_old_split_fragments_after_respawn(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;
  ShroomPlayerState* fragment;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  victim->mass = SplitRequiredPreCostMass();
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = (ShroomVec2){3000.0f, 3000.0f};
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, victim));
  fragment = FindSplitPiece(victim->player_id);
  TEST_ASSERT_NOT_NULL(fragment);
  victim->merge_timer = 0.0f;
  fragment->merge_timer = 0.0f;
  victim->spawn_protection_timer = 0.0f;
  fragment->spawn_protection_timer = 0.0f;

  attacker->mass = victim->mass * SHROOM_CONSUME_MASS_ADVANTAGE;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = victim->position;

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_EQUAL_UINT32(2u, victim->player_id);
  TEST_ASSERT_EQUAL_UINT8(0u, victim->piece_index);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
  TEST_ASSERT_EQUAL_size_t(1, CountAlivePieces(victim->player_id));
  TEST_ASSERT_NULL(FindSplitPiece(victim->player_id));
}

void test_world_step_decays_oversized_player_and_ejects_spore_mass(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();
  world.spore_count = 0;

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){3000.0f, 3000.0f};
  player->mass = SHROOM_DECAY_MASS_THRESHOLD + 100.0f;
  player->radius = ShroomMassToRadius(player->mass);
  player->last_move_time_ms = 100000u;

  ShroomWorldStep(&world, 10.0f);

  TEST_ASSERT_TRUE(player->mass < (SHROOM_DECAY_MASS_THRESHOLD + 100.0f));
  TEST_ASSERT_EQUAL_size_t(1, world.spore_count);
  TEST_ASSERT_TRUE(world.spores[0].active);
  TEST_ASSERT_GREATER_THAN_UINT16(0u, world.spores[0].value);
}

void test_outer_zone_disables_decay_for_oversized_player(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();
  world.spore_count = 0;

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){300.0f, 300.0f};
  player->mass = SHROOM_DECAY_MASS_THRESHOLD + 100.0f;
  player->radius = ShroomMassToRadius(player->mass);
  player->last_move_time_ms = 100000u;

  ShroomWorldStep(&world, 10.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DECAY_MASS_THRESHOLD + 100.0f, player->mass);
  TEST_ASSERT_EQUAL_size_t(0, world.spore_count);
}

void test_center_zone_decay_starts_at_lower_threshold(void) {
  ShroomPlayerState* player;

  ResetWorldForPlayers();
  world.spore_count = 0;

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);

  player->position = (ShroomVec2){world.width * 0.5f, world.height * 0.5f};
  player->mass = (SHROOM_DEFAULT_PLAYER_MASS * 2.0f) + 20.0f;
  player->radius = ShroomMassToRadius(player->mass);
  player->last_move_time_ms = 100000u;

  ShroomWorldStep(&world, 10.0f);

  TEST_ASSERT_TRUE(player->mass < ((SHROOM_DEFAULT_PLAYER_MASS * 2.0f) + 20.0f));
  TEST_ASSERT_EQUAL_size_t(1, world.spore_count);
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
  world.spores[0] = (ShroomSporeState){
      .entity_id = 1, .position = {3880.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE, .active = true};
  world.spores[1] = (ShroomSporeState){
      .entity_id = 2, .position = {4035.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE, .active = true};

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
  world.spores[0] = (ShroomSporeState){
      .entity_id = 1, .position = {3880.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE, .active = true};
  world.spores[1] = (ShroomSporeState){
      .entity_id = 2, .position = {4035.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE, .active = true};

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

void test_bot_pursues_nearby_spore_within_search_radius(void) {
  ShroomPlayerState* bot;

  ResetWorldForPlayers();

  bot = ShroomWorldSpawnPlayer(&world, 1, true);
  TEST_ASSERT_NOT_NULL(bot);

  bot->mass = SHROOM_DEFAULT_PLAYER_MASS;
  bot->radius = ShroomMassToRadius(bot->mass);
  bot->position = (ShroomVec2){3000.0f, 3000.0f};
  world.spore_count = 1;
  world.spores[0] = (ShroomSporeState){
      .entity_id = 1, .position = {3120.0f, 3000.0f}, .value = SHROOM_SPORE_VALUE, .active = true};

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, bot->input_direction.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bot->input_direction.y);
}

void test_bot_ignores_spore_beyond_search_radius(void) {
  /* A spore outside SHROOM_BOT_SPORE_SEARCH_RADIUS lives in a grid cell that the bot scan never
   * visits, so the bot should fall back to the "move toward world center" branch. With the bot
   * already at the center, the resulting input direction should be zero. */
  ShroomPlayerState* bot;
  const ShroomVec2 center = (ShroomVec2){SHROOM_WORLD_WIDTH * 0.5f, SHROOM_WORLD_HEIGHT * 0.5f};

  ResetWorldForPlayers();

  bot = ShroomWorldSpawnPlayer(&world, 1, true);
  TEST_ASSERT_NOT_NULL(bot);

  bot->mass = SHROOM_DEFAULT_PLAYER_MASS;
  bot->radius = ShroomMassToRadius(bot->mass);
  bot->position = center;
  world.spore_count = 1;
  world.spores[0] = (ShroomSporeState){.entity_id = 1,
                                       .position = {center.x + 1500.0f, center.y},
                                       .value = SHROOM_SPORE_VALUE,
                                       .active = true};

  ShroomWorldStep(&world, 0.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bot->input_direction.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, bot->input_direction.y);
}

void test_corner_consume_works_near_boundaries(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;
  const float expected_gain = 100.0f * SHROOM_CONSUME_MASS_GAIN_FACTOR;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  /* Position both players in the bottom-right corner */
  attacker->mass = 140.0f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position =
      (ShroomVec2){world.width - attacker->radius, world.height - attacker->radius};

  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = (ShroomVec2){world.width - victim->radius, world.height - victim->radius};
  victim->spawn_protection_timer = 0.0f;

  ShroomWorldStep(&world, 0.0f);

  /* Consume should work even in corners */
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 140.0f + expected_gain, attacker->mass);
  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
}

void test_edge_consume_works_when_target_pinned_against_wall(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;
  const float expected_gain = 100.0f * SHROOM_CONSUME_MASS_GAIN_FACTOR;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  /* Victim pinned against right wall */
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = (ShroomVec2){world.width - victim->radius, 3000.0f};
  victim->spawn_protection_timer = 0.0f;

  /* Attacker approaches from the left, overlapping into the wall */
  attacker->mass = 140.0f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){world.width - victim->radius - 5.0f, 3000.0f};

  ShroomWorldStep(&world, 0.0f);

  /* Consume should work even when target is pinned against wall */
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 140.0f + expected_gain, attacker->mass);
  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
}

void test_corner_consume_works_with_movement_clamping(void) {
  ShroomPlayerState* attacker;
  ShroomPlayerState* victim;
  const float expected_gain = 100.0f * SHROOM_CONSUME_MASS_GAIN_FACTOR;

  ResetWorldForPlayers();

  attacker = ShroomWorldSpawnPlayer(&world, 1, false);
  victim = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(attacker);
  TEST_ASSERT_NOT_NULL(victim);

  /* Victim in bottom-right corner */
  victim->mass = 100.0f;
  victim->radius = ShroomMassToRadius(victim->mass);
  victim->position = (ShroomVec2){world.width - 100.0f, world.height - 100.0f};

  /* Attacker starts further away and moves toward victim */
  attacker->mass = 140.0f;
  attacker->radius = ShroomMassToRadius(attacker->mass);
  attacker->position = (ShroomVec2){world.width - 300.0f, world.height - 300.0f};
  attacker->input_direction = (ShroomVec2){0.707f, 0.707f}; /* diagonal toward corner */

  /* Step multiple times to let movement and clamping happen */
  for (int i = 0; i < 30; i++) {
    ShroomWorldStep(&world, 0.1f);
  }

  /* Consume should work even after movement clamping in corners */
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 140.0f + expected_gain, attacker->mass);
  TEST_ASSERT_TRUE(victim->alive);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, victim->mass);
}

void test_split_piece_survives_immediately_after_creation(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;
  ShroomPlayerState* other_player;

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  other_player = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(player);
  TEST_ASSERT_NOT_NULL(other_player);

  /* Set up player with enough mass to split */
  player->mass = SplitRequiredPreCostMass();
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};
  player->position = (ShroomVec2){3000.0f, 3000.0f};

  /* Other player is nearby but smaller */
  other_player->mass = SHROOM_DEFAULT_PLAYER_MASS;
  other_player->radius = ShroomMassToRadius(other_player->mass);
  other_player->position = (ShroomVec2){3100.0f, 3000.0f};

  /* Split the player */
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);
  TEST_ASSERT_EQUAL_size_t(2, CountAlivePieces(player->player_id));

  /* Step the world once - split piece should still be alive */
  ShroomWorldStep(&world, 0.016f);

  /* Both pieces should still be alive after one step */
  TEST_ASSERT_TRUE(player->alive);
  TEST_ASSERT_TRUE(piece->alive);
  TEST_ASSERT_EQUAL_size_t(2, CountAlivePieces(player->player_id));
}

void test_split_piece_not_consumed_by_nearby_larger_player(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;
  ShroomPlayerState* larger_player;
  const float starting_mass = SplitRequiredPreCostMass();

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  larger_player = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(player);
  TEST_ASSERT_NOT_NULL(larger_player);

  /* Set up player with enough mass to split */
  player->mass = starting_mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};
  player->position = (ShroomVec2){3000.0f, 3000.0f};

  /* Larger player is nearby and could consume the split piece */
  larger_player->mass = starting_mass * 1.5f;
  larger_player->radius = ShroomMassToRadius(larger_player->mass);
  larger_player->position = (ShroomVec2){3050.0f, 3000.0f};

  /* Split the player */
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);

  /* Step the world - the split piece should survive even though larger player is nearby */
  ShroomWorldStep(&world, 0.016f);

  /* Split piece should still be alive */
  TEST_ASSERT_TRUE(piece->alive);
  TEST_ASSERT_EQUAL_size_t(2, CountAlivePieces(player->player_id));
}

void test_split_spawn_protection_blocks_larger_player_consumption(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;
  ShroomPlayerState* larger_player;
  const float starting_mass = SplitRequiredPreCostMass();

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  larger_player = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(player);
  TEST_ASSERT_NOT_NULL(larger_player);

  /* Set up player with enough mass to split */
  player->mass = starting_mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};
  player->position = (ShroomVec2){3000.0f, 3000.0f};

  /* Larger player is nearby and could consume the split piece */
  larger_player->mass = starting_mass * 1.5f;
  larger_player->radius = ShroomMassToRadius(larger_player->mass);
  larger_player->position = (ShroomVec2){3050.0f, 3000.0f};

  /* Split the player */
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);

  /* Check state immediately after split. */
  TEST_ASSERT_TRUE(piece->alive);
  TEST_ASSERT_EQUAL_UINT32(player->player_id, piece->player_id);
  TEST_ASSERT_EQUAL_UINT8(1, piece->piece_index);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, starting_mass * (1.0f - SHROOM_SPLIT_MASS_LOSS_FRACTION) / 2.0f,
                           piece->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, starting_mass * (1.0f - SHROOM_SPLIT_MASS_LOSS_FRACTION) / 2.0f,
                           player->mass);
  TEST_ASSERT_TRUE(piece->merge_timer > 0.0f);
  TEST_ASSERT_TRUE(piece->spawn_protection_timer > 0.0f);
  TEST_ASSERT_TRUE(player->spawn_protection_timer > 0.0f);

  /* Step the world: spawn protection should block nearby larger-player consumption. */
  ShroomWorldStep(&world, 0.016f);

  /* Split piece should still be alive. */
  TEST_ASSERT_TRUE(piece->alive);
  TEST_ASSERT_EQUAL_size_t(2, CountAlivePieces(player->player_id));
  TEST_ASSERT_TRUE(player->alive);
  TEST_ASSERT_EQUAL_UINT32(player->player_id, piece->player_id);
}

void test_split_spawn_protection_expires_before_merge_timer(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;
  ShroomPlayerState* larger_player;
  const float starting_mass = SplitRequiredPreCostMass();

  ResetWorldForPlayers();

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  larger_player = ShroomWorldSpawnPlayer(&world, 2, false);
  TEST_ASSERT_NOT_NULL(player);
  TEST_ASSERT_NOT_NULL(larger_player);

  player->mass = starting_mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};
  player->position = (ShroomVec2){3000.0f, 3000.0f};

  larger_player->mass = starting_mass * 1.5f;
  larger_player->radius = ShroomMassToRadius(larger_player->mass);
  larger_player->position = (ShroomVec2){5200.0f, 5200.0f};

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);

  ShroomWorldStep(&world, SHROOM_SPLIT_PROTECTION_SECONDS + 0.05f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, player->spawn_protection_timer);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, piece->spawn_protection_timer);
  TEST_ASSERT_TRUE(player->merge_timer > 0.0f);
  TEST_ASSERT_TRUE(piece->merge_timer > 0.0f);
}

void test_center_zone_max_players_stress_remains_bounded(void) {
  const ShroomVec2 center = {SHROOM_WORLD_WIDTH * 0.5f, SHROOM_WORLD_HEIGHT * 0.5f};
  const int columns = 16;
  const float spacing = 42.0f;
  const int steps = 90;
  clock_t start_time;
  double elapsed_seconds;

  ResetWorldForPlayers();

  for (size_t index = 0; index < SHROOM_MAX_PLAYER_ENTITIES; ++index) {
    ShroomPlayerState* player = ShroomWorldSpawnPlayer(&world, (ShroomPlayerId)(index + 1u), false);
    const int column = (int)(index % (size_t)columns);
    const int row = (int)(index / (size_t)columns);
    const float offset_x = ((float)column - 7.5f) * spacing;
    const float offset_y = ((float)row - 7.5f) * spacing;

    TEST_ASSERT_NOT_NULL(player);
    player->position = (ShroomVec2){center.x + offset_x, center.y + offset_y};
    player->mass = SHROOM_DEFAULT_PLAYER_MASS * 2.0f;
    player->radius = ShroomMassToRadius(player->mass);
    player->input_direction =
        (ShroomVec2){(index % 2u) == 0u ? 1.0f : -1.0f, (index % 3u) == 0u ? 1.0f : -1.0f};
    player->is_bot = false;
    player->ai_controlled = false;
  }

  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_PLAYER_ENTITIES, world.player_count);

  start_time = clock();
  for (int step = 0; step < steps; ++step) {
    ShroomWorldStep(&world, 1.0f / 60.0f);
  }
  elapsed_seconds = (double)(clock() - start_time) / (double)CLOCKS_PER_SEC;

  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_PLAYER_ENTITIES, world.player_count);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)steps, world.tick);
  if (getenv("SHROOM_VALGRIND") == NULL) {
    TEST_ASSERT_TRUE(elapsed_seconds < 5.0);
  }

  for (size_t index = 0; index < world.player_count; ++index) {
    const ShroomPlayerState* player = &world.players[index];

    TEST_ASSERT_TRUE(isfinite(player->position.x));
    TEST_ASSERT_TRUE(isfinite(player->position.y));
    TEST_ASSERT_TRUE(isfinite(player->mass));
    TEST_ASSERT_TRUE(player->position.x >= player->radius);
    TEST_ASSERT_TRUE(player->position.x <= world.width - player->radius);
    TEST_ASSERT_TRUE(player->position.y >= player->radius);
    TEST_ASSERT_TRUE(player->position.y <= world.height - player->radius);
  }
}

void test_full_participant_lobby_supports_maximum_split_pieces(void) {
  ResetWorldForPlayers();

  for (size_t participant = 0u; participant < SHROOM_MAX_PARTICIPANTS - 1u; ++participant) {
    ShroomPlayerState* primary =
        ShroomWorldSpawnPlayer(&world, (ShroomPlayerId)(participant + 1u), true);

    TEST_ASSERT_NOT_NULL(primary);
    for (size_t piece = 1u; piece < SHROOM_MAX_SPLIT_PIECES; ++piece) {
      primary->mass = SHROOM_MAX_PLAYER_MASS;
      primary->radius = ShroomMassToRadius(primary->mass);
      TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, primary));
    }
  }

  TEST_ASSERT_EQUAL_size_t((SHROOM_MAX_PARTICIPANTS - 1u) * SHROOM_MAX_SPLIT_PIECES,
                           world.player_count);

  {
    ShroomPlayerState* final_participant =
        ShroomWorldSpawnPlayer(&world, SHROOM_MAX_PARTICIPANTS, true);
    TEST_ASSERT_NOT_NULL(final_participant);
    for (size_t piece = 1u; piece < SHROOM_MAX_SPLIT_PIECES; ++piece) {
      final_participant->mass = SHROOM_MAX_PLAYER_MASS;
      final_participant->radius = ShroomMassToRadius(final_participant->mass);
      TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, final_participant));
    }
  }

  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_PLAYER_ENTITIES, world.player_count);
  TEST_ASSERT_NULL(ShroomWorldSpawnPlayer(&world, SHROOM_MAX_PARTICIPANTS + 1u, false));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_world_init_sets_expected_defaults);
  RUN_TEST(test_world_init_with_seed_repeats_same_layout);
  RUN_TEST(test_world_init_with_different_seed_changes_layout);
  RUN_TEST(test_world_init_populates_active_spores_with_value);
  RUN_TEST(test_world_init_populates_powerups_across_supported_types);
  RUN_TEST(test_zone_classification_matches_center_mid_and_outer);
  RUN_TEST(test_mass_helpers_respect_expected_scaling_and_bounds);
  RUN_TEST(test_human_spawn_protection_blocks_consumption_then_expires);
  RUN_TEST(test_bot_spawn_keeps_existing_unprotected_behavior);
  RUN_TEST(test_consume_predicate_respects_mass_zone_and_protection_boundaries);
  RUN_TEST(test_decay_predicate_respects_zone_threshold_boundaries);
  RUN_TEST(test_split_predicate_respects_mass_life_and_piece_boundaries);
  RUN_TEST(test_merge_predicate_respects_cooldown_proximity_and_identity);
  RUN_TEST(test_spawn_player_prefers_safe_outer_spawn);
  RUN_TEST(test_spawn_density_tightens_with_large_lobbies);
  RUN_TEST(test_spawn_distribution_avoids_player_overlap_in_crowded_lobbies);
  RUN_TEST(test_world_step_moves_player_and_increments_tick);
  RUN_TEST(test_world_step_clamps_player_to_world_bounds);
  RUN_TEST(test_world_step_collects_spores_and_gains_mass);
  RUN_TEST(test_world_step_collects_speed_powerup_and_expires_effect);
  RUN_TEST(test_mass_shield_powerup_blocks_consumption_until_expired);
  RUN_TEST(test_powerup_respawns_after_timer);
  RUN_TEST(test_world_step_consumes_player_when_mass_advantage_and_overlap_match);
  RUN_TEST(test_world_step_does_not_consume_without_required_mass_advantage);
  RUN_TEST(test_center_zone_uses_lower_consume_advantage);
  RUN_TEST(test_spawn_player_reuses_inactive_slot);
  RUN_TEST(test_world_step_caps_mass_gain_at_configured_maximum);
  RUN_TEST(test_player_can_voluntarily_split_at_large_colony_threshold);
  RUN_TEST(test_remove_player_clears_maximum_colony_and_reuses_capacity);
  RUN_TEST(test_remove_player_rejects_invalid_owner_without_mutating_world);
  RUN_TEST(test_player_split_uses_requested_aim_direction);
  RUN_TEST(test_player_cannot_voluntarily_split_below_large_colony_threshold);
  RUN_TEST(test_split_pieces_wait_to_merge_until_cooldown_and_proximity);
  RUN_TEST(test_consuming_primary_clears_old_split_fragments_after_respawn);
  RUN_TEST(test_world_step_decays_oversized_player_and_ejects_spore_mass);
  RUN_TEST(test_outer_zone_disables_decay_for_oversized_player);
  RUN_TEST(test_center_zone_decay_starts_at_lower_threshold);
  RUN_TEST(test_small_bot_prefers_safer_spore_over_slightly_closer_center_spore);
  RUN_TEST(test_large_bot_prefers_center_pressure_spore_choice);
  RUN_TEST(test_bot_flees_nearby_threat_even_with_available_prey);
  RUN_TEST(test_bot_pursues_nearby_spore_within_search_radius);
  RUN_TEST(test_bot_ignores_spore_beyond_search_radius);
  RUN_TEST(test_corner_consume_works_near_boundaries);
  RUN_TEST(test_edge_consume_works_when_target_pinned_against_wall);
  RUN_TEST(test_corner_consume_works_with_movement_clamping);
  RUN_TEST(test_split_piece_survives_immediately_after_creation);
  RUN_TEST(test_split_piece_not_consumed_by_nearby_larger_player);
  RUN_TEST(test_split_spawn_protection_blocks_larger_player_consumption);
  RUN_TEST(test_split_spawn_protection_expires_before_merge_timer);
  RUN_TEST(test_center_zone_max_players_stress_remains_bounded);
  RUN_TEST(test_full_participant_lobby_supports_maximum_split_pieces);
  return UNITY_END();
}
