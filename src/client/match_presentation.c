#include "match_presentation.h"

#include "game.h"

#include <string.h>

void ShroomGameResetTransientRoundState(Game* game) {
  if (game == NULL) {
    return;
  }

  game->leaderboard_overlay_open = false;
  game->menu_overlay_open = false;
  game->leave_confirmation_open = false;
  game->inspect_overlay_open = false;
  game->inspect_overlay_progress = 0.0f;
  game->inspect_prompt_timer = 0.0f;
  game->selected_inspect_index = 0;
  game->inspect_target_count = 0;
  game->selected_inspect_player_id = 0u;

  game->chat_open = false;
  game->chat_minimized = false;
  game->chat_focus_input = false;
  game->chat_inactive_timer = 0.0f;
  memset(game->chat_input_buf, 0, sizeof(game->chat_input_buf));
  game->chat_scroll_to_bottom = false;

  game->return_to_menu_requested = false;
  game->play_again_requested = false;
  game->split_requested = false;
  game->eject_requested = false;
  game->split_hold_timer = 0.0f;
  game->piece_focus_changed = false;

  game->screen_flash_timer = 0.0f;
  game->screen_flash_color = (Color){0};
  game->death_cutscene_timer = 0.0f;
  game->death_cutscene_duration = 0.0f;
  game->death_cutscene_final_mass = 0.0f;
  game->death_cutscene_survival_time = 0.0f;
  game->death_cutscene_final_rank = 0;
  memset(game->death_cutscene_killer_name, 0, sizeof(game->death_cutscene_killer_name));
  game->death_camera_hold_timer = 0.0f;
  game->death_camera_hold_pos = (Vector2){0};
}
