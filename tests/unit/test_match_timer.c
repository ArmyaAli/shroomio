#include "unity.h"
#include "../src/shared/sim.h"

#include <math.h>

static ShroomWorldState world;

void setUp(void) { ShroomWorldInitWithSeed(&world, 42u); }

void tearDown(void) {}

static void ResetWorldForPlayers(void) {
  world.tick = 0;
  world.player_count = 0;
  world.spore_count = 0;
  world.powerup_count = 0;
  world.next_entity_id = 1;
}

void test_match_timer_initializes_with_default_duration(void) {
  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RUNNING, world.match_phase);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_MATCH_DURATION_SECONDS, world.match_time_remaining);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_MATCH_DURATION_SECONDS, world.match_duration_seconds);
}

void test_match_timer_counts_down_during_step(void) {
  const float initial_time = world.match_time_remaining;
  const float dt = 1.0f / SHROOM_SERVER_TICK_RATE;

  ShroomWorldStep(&world, dt);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, initial_time - dt, world.match_time_remaining);
  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RUNNING, world.match_phase);
}

void test_match_timer_transitions_to_results_at_zero(void) {
  ShroomWorldSetMatchDuration(&world, 2.0f);

  for (int i = 0; i < 65; ++i) {
    ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);
  }

  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RESULTS, world.match_phase);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, world.match_time_remaining);
}

void test_match_timer_transitions_to_reset_after_results(void) {
  ShroomWorldSetMatchDuration(&world, 1.0f);

  for (int i = 0; i < 1000; ++i) {
    ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);
  }

  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RESET, world.match_phase);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, world.match_results_time_remaining);
}

void test_podium_sorted_by_mass_with_stable_tiebreak(void) {
  ResetWorldForPlayers();

  ShroomPlayerState* p1 = ShroomWorldSpawnPlayer(&world, 10, false);
  ShroomPlayerState* p2 = ShroomWorldSpawnPlayer(&world, 20, false);
  ShroomPlayerState* p3 = ShroomWorldSpawnPlayer(&world, 30, false);
  ShroomPlayerState* p4 = ShroomWorldSpawnPlayer(&world, 5, false);

  p1->mass = 150.0f;
  p2->mass = 200.0f;
  p3->mass = 200.0f;
  p4->mass = 100.0f;

  ShroomComputeMatchPodium(&world);

  TEST_ASSERT_EQUAL_UINT32(20, world.podium_player_ids[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, world.podium_masses[0]);
  TEST_ASSERT_EQUAL_UINT32(30, world.podium_player_ids[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, world.podium_masses[1]);
  TEST_ASSERT_EQUAL_UINT32(10, world.podium_player_ids[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, world.podium_masses[2]);
}

void test_podium_handles_fewer_players_than_slots(void) {
  ResetWorldForPlayers();

  ShroomPlayerState* p1 = ShroomWorldSpawnPlayer(&world, 1, false);
  p1->mass = 100.0f;

  ShroomComputeMatchPodium(&world);

  TEST_ASSERT_EQUAL_UINT32(1, world.podium_player_ids[0]);
  TEST_ASSERT_EQUAL_UINT32(0, world.podium_player_ids[1]);
  TEST_ASSERT_EQUAL_UINT32(0, world.podium_player_ids[2]);
}

void test_reset_match_restores_players_to_default_mass(void) {
  ResetWorldForPlayers();

  ShroomPlayerState* p1 = ShroomWorldSpawnPlayer(&world, 1, false);
  ShroomPlayerState* p2 = ShroomWorldSpawnPlayer(&world, 2, false);

  p1->mass = 500.0f;
  p2->mass = 300.0f;
  world.match_phase = SHROOM_MATCH_PHASE_RESET;

  ShroomWorldResetMatch(&world);

  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RUNNING, world.match_phase);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, p1->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_DEFAULT_PLAYER_MASS, p2->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS,
                           p1->spawn_protection_timer);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, SHROOM_PLAYER_SPAWN_PROTECTION_SECONDS,
                           p2->spawn_protection_timer);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, world.match_duration_seconds, world.match_time_remaining);
}

void test_reset_match_removes_every_supported_split_fragment_count(void) {
  for (uint8_t fragment_count = 1u; fragment_count < SHROOM_MAX_SPLIT_PIECES; ++fragment_count) {
    ResetWorldForPlayers();
    ShroomPlayerState* primary = ShroomWorldSpawnPlayer(&world, 77u, false);
    const size_t expected_slots = 1u + fragment_count;

    primary->has_split = true;
    primary->ai_controlled = true;
    primary->split_velocity = (ShroomVec2){10.0f, -5.0f};
    primary->merge_timer = 2.0f;
    primary->speed_powerup_timer = 2.0f;
    primary->shield_powerup_timer = 2.0f;
    primary->magnet_powerup_timer = 2.0f;
    primary->decay_immune_powerup_timer = 2.0f;
    primary->eject_cooldown_timer = 2.0f;
    for (uint8_t index = 1u; index <= fragment_count; ++index) {
      world.players[index] = *primary;
      world.players[index].entity_id = world.next_entity_id++;
      world.players[index].piece_index = index;
      world.players[index].ai_controlled = true;
    }
    world.player_count = expected_slots;

    ShroomWorldResetMatch(&world);

    TEST_ASSERT_EQUAL_size_t(expected_slots, world.player_count);
    TEST_ASSERT_TRUE(primary->alive);
    TEST_ASSERT_EQUAL_UINT32(77u, primary->player_id);
    TEST_ASSERT_EQUAL_UINT8(0u, primary->piece_index);
    TEST_ASSERT_FALSE(primary->has_split);
    TEST_ASSERT_FALSE(primary->ai_controlled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->split_velocity.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->merge_timer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->speed_powerup_timer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->shield_powerup_timer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->magnet_powerup_timer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->decay_immune_powerup_timer);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, primary->eject_cooldown_timer);
    for (uint8_t index = 1u; index <= fragment_count; ++index) {
      TEST_ASSERT_FALSE(world.players[index].alive);
      TEST_ASSERT_EQUAL_UINT32(0u, world.players[index].player_id);
      TEST_ASSERT_EQUAL_UINT32(0u, world.players[index].entity_id);
    }
  }
}

void test_repeated_split_reset_cycles_reuse_fragment_slots(void) {
  ResetWorldForPlayers();
  ShroomPlayerState* primary = ShroomWorldSpawnPlayer(&world, 88u, true);

  for (int cycle = 0; cycle < 8; ++cycle) {
    primary->mass = SHROOM_SPLIT_MIN_MASS * 8.0f;
    for (uint8_t piece = 1u; piece < SHROOM_MAX_SPLIT_PIECES; ++piece) {
      TEST_ASSERT_TRUE(ShroomWorldSplitPlayerToward(&world, primary, (ShroomVec2){1.0f, 0.0f}));
    }
    TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_SPLIT_PIECES, world.player_count);

    ShroomWorldResetMatch(&world);

    size_t live_identity_count = 0u;
    for (size_t index = 0; index < world.player_count; ++index) {
      if (world.players[index].alive && (world.players[index].player_id == 88u)) {
        ++live_identity_count;
        TEST_ASSERT_EQUAL_UINT8(0u, world.players[index].piece_index);
      }
    }
    TEST_ASSERT_EQUAL_size_t(1u, live_identity_count);
  }
}

void test_reset_match_clears_podium(void) {
  ResetWorldForPlayers();

  ShroomPlayerState* p1 = ShroomWorldSpawnPlayer(&world, 1, false);
  p1->mass = 500.0f;
  ShroomComputeMatchPodium(&world);
  TEST_ASSERT_EQUAL_UINT32(1, world.podium_player_ids[0]);

  ShroomWorldResetMatch(&world);

  TEST_ASSERT_EQUAL_UINT32(0, world.podium_player_ids[0]);
  TEST_ASSERT_EQUAL_UINT32(0, world.podium_player_ids[1]);
  TEST_ASSERT_EQUAL_UINT32(0, world.podium_player_ids[2]);
}

void test_set_match_duration_clamps_minimum(void) {
  ShroomWorldSetMatchDuration(&world, 0.5f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, world.match_duration_seconds);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, world.match_time_remaining);
}

void test_post_reset_timer_restarts(void) {
  ShroomWorldSetMatchDuration(&world, 1.0f);

  for (int i = 0; i < 1000; ++i) {
    ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);
  }

  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RESET, world.match_phase);

  ShroomWorldResetMatch(&world);

  TEST_ASSERT_EQUAL_UINT32(SHROOM_MATCH_PHASE_RUNNING, world.match_phase);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, world.match_time_remaining);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_match_timer_initializes_with_default_duration);
  RUN_TEST(test_match_timer_counts_down_during_step);
  RUN_TEST(test_match_timer_transitions_to_results_at_zero);
  RUN_TEST(test_match_timer_transitions_to_reset_after_results);
  RUN_TEST(test_podium_sorted_by_mass_with_stable_tiebreak);
  RUN_TEST(test_podium_handles_fewer_players_than_slots);
  RUN_TEST(test_reset_match_restores_players_to_default_mass);
  RUN_TEST(test_reset_match_removes_every_supported_split_fragment_count);
  RUN_TEST(test_repeated_split_reset_cycles_reuse_fragment_slots);
  RUN_TEST(test_reset_match_clears_podium);
  RUN_TEST(test_set_match_duration_clamps_minimum);
  RUN_TEST(test_post_reset_timer_restarts);
  return UNITY_END();
}
