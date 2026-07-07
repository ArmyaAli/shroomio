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
  TEST_ASSERT_FLOAT_WITHIN(0.001f, world.match_duration_seconds, world.match_time_remaining);
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
  RUN_TEST(test_reset_match_clears_podium);
  RUN_TEST(test_set_match_duration_clamps_minimum);
  RUN_TEST(test_post_reset_timer_restarts);
  return UNITY_END();
}
