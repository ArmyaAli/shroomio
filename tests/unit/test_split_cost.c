#include "unity.h"

#include "shared/config.h"
#include "shared/sim.h"

#include <math.h>

static ShroomWorldState world;

void setUp(void) { ShroomWorldInitWithSeed(&world, 7u); }

void tearDown(void) {}

static ShroomPlayerState* FindSplitPiece(ShroomPlayerId player_id) {
  size_t i;
  for (i = 0; i < world.player_count; ++i) {
    ShroomPlayerState* p = &world.players[i];
    if (p->alive && (p->player_id == player_id) && (p->piece_index > 0)) {
      return p;
    }
  }
  return NULL;
}

static void test_split_conserve_mass_minus_cost(void) {
  ShroomPlayerState* player;
  ShroomPlayerState* piece;
  const float starting_mass = SHROOM_SPLIT_MIN_MASS + 200.0f;

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = starting_mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
  piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);

  const float expected_cost = starting_mass * SHROOM_SPLIT_MASS_LOSS_FRACTION;
  const float expected_total = starting_mass - expected_cost;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_total, player->mass + piece->mass);

  const float expected_half = expected_total / 2.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_half, player->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_half, piece->mass);
}

static void test_can_split_at_exact_pre_cost_floor(void) {
  ShroomPlayerState* player;

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = SHROOM_SPLIT_MIN_MASS;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};

  TEST_ASSERT_TRUE(ShroomPlayerCanSplit(&world, player));
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));
}

static void test_can_split_above_floor_with_margin(void) {
  ShroomPlayerState* player;
  const float starting_mass = SHROOM_SPLIT_MIN_MASS + 100.0f;

  player = ShroomWorldSpawnPlayer(&world, 1, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = starting_mass;
  player->radius = ShroomMassToRadius(player->mass);
  player->input_direction = (ShroomVec2){1.0f, 0.0f};

  TEST_ASSERT_TRUE(ShroomPlayerCanSplit(&world, player));
  TEST_ASSERT_TRUE(ShroomWorldSplitPlayer(&world, player));

  ShroomPlayerState* piece = FindSplitPiece(player->player_id);
  TEST_ASSERT_NOT_NULL(piece);
  const float expected_half =
      (starting_mass - starting_mass * SHROOM_SPLIT_MASS_LOSS_FRACTION) / 2.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_half, player->mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_half, piece->mass);
}

static void test_mass_loss_fraction_is_positive(void) {
  /* Snapshot the constant; if anyone drops it to zero the cost mechanic
   * silently disappears — make that a deliberate, visible test edit. */
  TEST_ASSERT_TRUE(SHROOM_SPLIT_MASS_LOSS_FRACTION > 0.0f);
  TEST_ASSERT_TRUE(SHROOM_SPLIT_MASS_LOSS_FRACTION < 1.0f);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_split_conserve_mass_minus_cost);
  RUN_TEST(test_can_split_at_exact_pre_cost_floor);
  RUN_TEST(test_can_split_above_floor_with_margin);
  RUN_TEST(test_mass_loss_fraction_is_positive);
  return UNITY_END();
}
