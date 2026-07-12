#include "unity.h"

#include "server/snapshot_stats.h"
#include "shared/sim.h"

#include <stdint.h>

static void AssertHighIdStatsSerialize(ShroomPlayerId player_id) {
  ShroomWorldState world;
  ShroomPlayerState* player;
  const ShroomRoundStats* stats;
  ShroomSnapshotPlayerState snapshot_player = {0};

  ShroomWorldInitWithSeed(&world, 474u);
  player = ShroomWorldSpawnPlayer(&world, player_id, false);
  TEST_ASSERT_NOT_NULL(player);
  player->position = world.spores[0].position;
  ShroomWorldStep(&world, 1.0f / SHROOM_SERVER_TICK_RATE);

  stats = ShroomWorldGetRoundStats(&world, player_id);
  TEST_ASSERT_NOT_NULL(stats);
  TEST_ASSERT_GREATER_THAN_UINT32(0u, stats->spores_collected);

  ShroomServerPopulateSnapshotRoundStats(&world, player_id, &snapshot_player);
  TEST_ASSERT_EQUAL_UINT16((uint16_t)stats->spores_collected, snapshot_player.round_spores);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)stats->kills, snapshot_player.round_kills);
}

void setUp(void) {}

void tearDown(void) {}

void test_snapshot_stats_support_player_id_64(void) { AssertHighIdStatsSerialize(64u); }

void test_snapshot_stats_support_player_id_65(void) { AssertHighIdStatsSerialize(65u); }

void test_snapshot_stats_support_maximum_player_id(void) { AssertHighIdStatsSerialize(UINT32_MAX); }

void test_snapshot_stats_default_to_zero_for_unknown_player(void) {
  ShroomWorldState world;
  ShroomSnapshotPlayerState snapshot_player = {.round_spores = 9u, .round_kills = 7u};

  ShroomWorldInitWithSeed(&world, 474u);
  ShroomServerPopulateSnapshotRoundStats(&world, UINT32_MAX, &snapshot_player);

  TEST_ASSERT_EQUAL_UINT16(0u, snapshot_player.round_spores);
  TEST_ASSERT_EQUAL_UINT8(0u, snapshot_player.round_kills);
}

void test_round_stat_slot_is_reused_after_participant_removal(void) {
  ShroomWorldState world;

  ShroomWorldInitWithSeed(&world, 474u);
  for (uint32_t index = 0u; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    TEST_ASSERT_NOT_NULL(ShroomWorldSpawnPlayer(&world, 1000u + index, false));
  }

  TEST_ASSERT_EQUAL_size_t(1u, ShroomWorldRemovePlayer(&world, 1017u));
  TEST_ASSERT_NULL(ShroomWorldGetRoundStats(&world, 1017u));
  TEST_ASSERT_NOT_NULL(ShroomWorldSpawnPlayer(&world, UINT32_MAX, false));
  TEST_ASSERT_NOT_NULL(ShroomWorldGetRoundStats(&world, UINT32_MAX));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_snapshot_stats_support_player_id_64);
  RUN_TEST(test_snapshot_stats_support_player_id_65);
  RUN_TEST(test_snapshot_stats_support_maximum_player_id);
  RUN_TEST(test_snapshot_stats_default_to_zero_for_unknown_player);
  RUN_TEST(test_round_stat_slot_is_reused_after_participant_removal);
  return UNITY_END();
}
