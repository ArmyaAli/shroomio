#include "unity.h"
#include "../src/shared/sim.h"

void setUp(void) {}

void tearDown(void) {}

static ShroomPlayerState* FindSplitPiece(ShroomWorldState* world, ShroomPlayerId player_id) {
  for (size_t index = 0; index < world->player_count; ++index) {
    ShroomPlayerState* player = &world->players[index];
    if (player->alive && (player->player_id == player_id) && (player->piece_index > 0u)) {
      return player;
    }
  }
  return NULL;
}

void test_human_split_piece_accepts_direct_input_without_ai_control(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;
  ShroomPlayerState* piece;

  ShroomWorldInitWithSeed(&world, 11u);
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = SHROOM_DEFAULT_PLAYER_MASS * 5.0f;
  player->radius = ShroomMassToRadius(player->mass);

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayerToward(&world, player, (ShroomVec2){1.0f, 0.0f}));
  piece = FindSplitPiece(&world, player->player_id);
  TEST_ASSERT_NOT_NULL(piece);
  TEST_ASSERT_FALSE(piece->ai_controlled);

  ShroomPlayerSetInput(piece, (ShroomVec2){0.0f, -1.0f});
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, piece->input_direction.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, piece->input_direction.y);
}

void test_bot_split_piece_keeps_ai_control(void) {
  ShroomWorldState world;
  ShroomPlayerState* bot;
  ShroomPlayerState* piece;

  ShroomWorldInitWithSeed(&world, 12u);
  bot = ShroomWorldSpawnPlayer(&world, 1u, true);
  TEST_ASSERT_NOT_NULL(bot);
  bot->mass = SHROOM_DEFAULT_PLAYER_MASS * 5.0f;
  bot->radius = ShroomMassToRadius(bot->mass);

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayerToward(&world, bot, (ShroomVec2){1.0f, 0.0f}));
  piece = FindSplitPiece(&world, bot->player_id);
  TEST_ASSERT_NOT_NULL(piece);
  TEST_ASSERT_TRUE(piece->ai_controlled);
}

void test_mid_mass_player_can_split_after_rebalance(void) {
  ShroomWorldState world;
  ShroomPlayerState* player;

  ShroomWorldInitWithSeed(&world, 13u);
  player = ShroomWorldSpawnPlayer(&world, 1u, false);
  TEST_ASSERT_NOT_NULL(player);
  player->mass = 400.0f;
  player->radius = ShroomMassToRadius(player->mass);

  TEST_ASSERT_TRUE(ShroomWorldSplitPlayerToward(&world, player, (ShroomVec2){1.0f, 0.0f}));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_human_split_piece_accepts_direct_input_without_ai_control);
  RUN_TEST(test_bot_split_piece_keeps_ai_control);
  RUN_TEST(test_mid_mass_player_can_split_after_rebalance);
  return UNITY_END();
}
