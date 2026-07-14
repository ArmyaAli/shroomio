#include "unity.h"

#include "client/game.h"
#include "client/match_presentation.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void test_new_round_resets_transient_state_and_preserves_session_state(void) {
  Game game = {0};
  ENetHost* host = (ENetHost*)(uintptr_t)1u;
  ENetPeer* peer = (ENetPeer*)(uintptr_t)2u;

  game.settings.diagnostics_enabled = true;
  game.settings.hud_density = CLIENT_HUD_MINIMAL;
  game.net.host = host;
  game.net.peer = peer;
  game.net.player_id = 37u;
  game.net.lobby_id = 11u;
  game.spectator_mode = true;
  game.spectator_follow_mode = true;
  game.spectated_entity_id = 91u;

  game.leaderboard_overlay_open = true;
  game.menu_overlay_open = true;
  game.diagnostics_overlay_open = true;
  game.leave_confirmation_open = true;
  game.inspect_overlay_open = true;
  game.inspect_overlay_progress = 1.0f;
  game.inspect_prompt_timer = 2.0f;
  game.selected_inspect_index = 3;
  game.inspect_target_count = 4;
  game.selected_inspect_player_id = 55u;
  game.chat_open = true;
  game.chat_minimized = true;
  game.chat_focus_input = true;
  game.chat_inactive_timer = 3.0f;
  strcpy(game.chat_input_buf, "old round draft");
  game.chat_scroll_to_bottom = true;
  game.return_to_menu_requested = true;
  game.play_again_requested = true;
  game.split_requested = true;
  game.eject_requested = true;
  game.split_hold_timer = 1.0f;
  game.piece_focus_changed = true;
  game.screen_flash_timer = 1.0f;
  game.screen_flash_color = (Color){1, 2, 3, 4};
  game.death_cutscene_timer = 1.0f;
  game.death_cutscene_duration = 2.0f;
  game.death_cutscene_final_mass = 300.0f;
  game.death_cutscene_survival_time = 30.0f;
  game.death_cutscene_final_rank = 2;
  strcpy(game.death_cutscene_killer_name, "Old Winner");
  game.death_camera_hold_timer = 1.0f;
  game.death_camera_hold_pos = (Vector2){10.0f, 20.0f};

  ShroomGameResetTransientRoundState(&game);

  TEST_ASSERT_FALSE(game.leaderboard_overlay_open);
  TEST_ASSERT_FALSE(game.menu_overlay_open);
  TEST_ASSERT_TRUE(game.diagnostics_overlay_open);
  TEST_ASSERT_FALSE(game.leave_confirmation_open);
  TEST_ASSERT_FALSE(game.inspect_overlay_open);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.inspect_overlay_progress);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.inspect_prompt_timer);
  TEST_ASSERT_EQUAL_INT(0, game.selected_inspect_index);
  TEST_ASSERT_EQUAL_INT(0, game.inspect_target_count);
  TEST_ASSERT_EQUAL_UINT32(0u, game.selected_inspect_player_id);
  TEST_ASSERT_FALSE(game.chat_open);
  TEST_ASSERT_FALSE(game.chat_minimized);
  TEST_ASSERT_FALSE(game.chat_focus_input);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.chat_inactive_timer);
  TEST_ASSERT_EQUAL_STRING("", game.chat_input_buf);
  TEST_ASSERT_FALSE(game.chat_scroll_to_bottom);
  TEST_ASSERT_FALSE(game.return_to_menu_requested);
  TEST_ASSERT_FALSE(game.play_again_requested);
  TEST_ASSERT_FALSE(game.split_requested);
  TEST_ASSERT_FALSE(game.eject_requested);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.split_hold_timer);
  TEST_ASSERT_FALSE(game.piece_focus_changed);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.screen_flash_timer);
  TEST_ASSERT_EQUAL_UINT8(0u, game.screen_flash_color.a);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_cutscene_timer);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_cutscene_duration);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_cutscene_final_mass);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_cutscene_survival_time);
  TEST_ASSERT_EQUAL_INT(0, game.death_cutscene_final_rank);
  TEST_ASSERT_EQUAL_STRING("", game.death_cutscene_killer_name);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_camera_hold_timer);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_camera_hold_pos.x);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, game.death_camera_hold_pos.y);

  TEST_ASSERT_EQUAL_PTR(host, game.net.host);
  TEST_ASSERT_EQUAL_PTR(peer, game.net.peer);
  TEST_ASSERT_EQUAL_UINT32(37u, game.net.player_id);
  TEST_ASSERT_EQUAL_UINT32(11u, game.net.lobby_id);
  TEST_ASSERT_TRUE(game.spectator_mode);
  TEST_ASSERT_TRUE(game.spectator_follow_mode);
  TEST_ASSERT_EQUAL_UINT32(91u, game.spectated_entity_id);
  TEST_ASSERT_EQUAL_INT(CLIENT_HUD_MINIMAL, game.settings.hud_density);
  TEST_ASSERT_TRUE(game.settings.diagnostics_enabled);
}

static void test_null_game_is_ignored(void) { ShroomGameResetTransientRoundState(NULL); }

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_new_round_resets_transient_state_and_preserves_session_state);
  RUN_TEST(test_null_game_is_ignored);
  return UNITY_END();
}
