#include "game.h"
#include "imgui_wrapper.h"
#include "screen.h"

#include "raylib.h"

static void RestartQuickPlaySession(Game* game) {
  const GameSessionMode mode = game->active_mode;

  if ((mode == SHROOM_SESSION_MODE_QUICK_PLAY) || (mode == SHROOM_SESSION_MODE_LOBBY_PLAY)) {
    game->death_cutscene_timer = 0.0f;
    game->death_cutscene_duration = 0.0f;
    game->death_camera_hold_timer = 0.0f;
    return;
  }

  GameShutdown(game);
  GameInit(game, GetScreenWidth(), GetScreenHeight(), mode);
}

static bool GameplayInit(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }

  GameInit(game, GetScreenWidth(), GetScreenHeight(), game->selected_mode);
  return true;
}

static void GameplayUpdate(ShroomScreenManager* manager, float delta_time) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  GameUpdate(game, delta_time);
}

static void GameplayDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  GameDraw(game);

  if (game->play_again_requested) {
    game->play_again_requested = false;
    RestartQuickPlaySession(game);
    return;
  }

  if (game->return_to_menu_requested) {
    game->return_to_menu_requested = false;
    /* Capture final stats for results screen */
    game->final_mass = game->local_player != NULL ? game->local_player->mass : 0.0f;
    /* Build leaderboard to get final rank */
    LeaderboardEntry leaderboard[SHROOM_MAX_PLAYERS];
    size_t leaderboard_count = 0;
    BuildLeaderboard(game, leaderboard, &leaderboard_count);
    game->final_rank = GetLocalPlayerRank(game, leaderboard, leaderboard_count);
    game->show_results = true;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_RESULTS);
  }
}

static void GameplayHandleInput(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  bool is_online;

  if (game == NULL) {
    return;
  }

  if ((game->death_cutscene_duration > 0.0f) &&
      (game->death_cutscene_timer < game->death_cutscene_duration) && IsKeyPressed(KEY_ESCAPE)) {
    game->death_cutscene_timer = game->death_cutscene_duration;
    return;
  }

  is_online = (game->active_mode == SHROOM_SESSION_MODE_QUICK_PLAY) ||
              (game->active_mode == SHROOM_SESSION_MODE_LOBBY_PLAY);

  /* While chat is open, only Esc closes it (and only when ImGui isn't consuming it).
     All other keys are captured by the ImGui InputText widget. */
  if (game->chat_open) {
    if (!ShroomImGui_WantCaptureKeyboard() && IsKeyPressed(KEY_ESCAPE)) {
      game->chat_open = false;
      game->net.chat_unread_count = 0;
    }
    return;
  }

  if (is_online && !game->net.welcome_received) {
    if ((game->net.status == CLIENT_NET_ERROR) || (game->net.status == CLIENT_NET_DISCONNECTED)) {
      /* QUICK_PLAY can retry in-place; LOBBY_PLAY returns to lobby selection. */
      if ((game->active_mode == SHROOM_SESSION_MODE_QUICK_PLAY) && IsKeyPressed(KEY_R)) {
        RestartQuickPlaySession(game);
      }
      if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
        game->return_to_menu_requested = true;
      }
      return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      game->return_to_menu_requested = true;
      return;
    }
  }

  if ((IsKeyPressed(game->settings.key_chat_open) || IsKeyPressed(KEY_ENTER)) && is_online &&
      game->net.welcome_received && !game->menu_overlay_open && !game->leaderboard_overlay_open &&
      !game->leave_confirmation_open) {
    game->chat_inactive_timer = 0.0f;
    if (game->chat_minimized) {
      game->chat_minimized = false;
      game->net.chat_unread_count = 0;
      game->chat_scroll_to_bottom = true;
    } else if (!game->chat_open) {
      game->chat_open = true;
      game->chat_focus_input = true;
      game->net.chat_unread_count = 0;
      game->chat_scroll_to_bottom = true;
    }
  }
  /* Hold Space to split — at max mass, one split per life. */
  if (!game->spectator_mode && IsKeyDown(KEY_SPACE) && !game->menu_overlay_open &&
      !game->leaderboard_overlay_open && !game->leave_confirmation_open &&
      (game->local_player != NULL) && game->local_player->alive && !game->local_has_split &&
      (game->local_player->mass >= SHROOM_SPLIT_MIN_MASS)) {
    game->split_hold_timer += GetFrameTime();
    if (game->split_hold_timer >= SHROOM_SPLIT_HOLD_SECONDS) {
      game->split_requested = true;
      game->split_hold_timer = 0.0f;
    }
  } else {
    game->split_hold_timer = 0.0f;
  }

  if (!game->spectator_mode && !game->menu_overlay_open && !game->leaderboard_overlay_open &&
      !game->leave_confirmation_open && (game->local_player != NULL) && game->local_player->alive &&
      (game->local_player->mass >= SHROOM_EJECT_MIN_MASS) &&
      (IsKeyPressed(KEY_E) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
    game->eject_requested = true;
  }

  if (IsKeyPressed(KEY_TAB)) {
    if (game->spectator_mode && !game->menu_overlay_open && !game->leave_confirmation_open) {
      GameCycleSpectatorTarget(game,
                               IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1);
    } else if ((game->local_piece_count > 1) && !game->menu_overlay_open &&
               !game->leave_confirmation_open) {
      /* Cycle through the local player's split pieces. */
      ShroomPlayerId local_pid = game->local_player != NULL ? game->local_player->player_id : 0;
      if (local_pid != 0) {
        uint32_t pieces[SHROOM_MAX_SPLIT_PIECES];
        int piece_count = 0;
        int current_idx = 0;
        size_t wi;

        for (wi = 0; wi < game->world.player_count && piece_count < SHROOM_MAX_SPLIT_PIECES; ++wi) {
          const ShroomPlayerState* p = &game->world.players[wi];
          if (p->alive && (p->player_id == local_pid)) {
            const uint32_t focused_eid = game->focused_piece_entity_id != 0
                                             ? game->focused_piece_entity_id
                                             : game->net.entity_id;
            if (p->entity_id == focused_eid) {
              current_idx = piece_count;
            }
            pieces[piece_count++] = p->entity_id;
          }
        }

        if (piece_count > 1) {
          game->focused_piece_entity_id = pieces[(current_idx + 1) % piece_count];
          game->piece_focus_changed = true;
          game->split_hold_timer = 0.0f;
        }
      }
    } else {
      game->leaderboard_overlay_open = !game->leaderboard_overlay_open;
      if (game->leaderboard_overlay_open) {
        game->menu_overlay_open = false;
        game->leave_confirmation_open = false;
        game->inspect_overlay_open = false;
        game->selected_inspect_player_id = 0;
      }
    }
  }

  if (IsKeyPressed(KEY_F3)) {
    game->diagnostics_overlay_open = !game->diagnostics_overlay_open;
    game->settings.diagnostics_enabled = game->diagnostics_overlay_open;
    ClientSettingsSave(&game->settings);
  }

  if (game->spectator_mode && IsKeyPressed(KEY_F)) {
    game->spectator_follow_mode = !game->spectator_follow_mode;
  }

  if (IsKeyPressed(game->settings.key_hud_toggle)) {
    game->settings.hud_density = (game->settings.hud_density + 1) % 3;
    ClientSettingsSave(&game->settings);
  }

  if (IsKeyPressed(game->settings.key_pause_menu)) {
    if (game->leaderboard_overlay_open) {
      game->leaderboard_overlay_open = false;
    } else if (game->inspect_overlay_open) {
      game->inspect_overlay_open = false;
      game->selected_inspect_player_id = 0;
    } else if (game->leave_confirmation_open) {
      game->leave_confirmation_open = false;
    } else {
      game->menu_overlay_open = !game->menu_overlay_open;
      if (game->menu_overlay_open) {
        game->leaderboard_overlay_open = false;
        game->inspect_overlay_open = false;
        game->selected_inspect_player_id = 0;
      }
    }
  }

  if (game->menu_overlay_open) {
    if (IsKeyPressed(KEY_ENTER)) {
      game->menu_overlay_open = false;
    }
    if ((game->active_mode == SHROOM_SESSION_MODE_OFFLINE_PRACTICE) && IsKeyPressed(KEY_M)) {
      game->return_to_menu_requested = true;
      return;
    }
    if (IsKeyPressed(KEY_M)) {
      game->leave_confirmation_open = true;
      game->menu_overlay_open = false;
    }
  }

  if (game->leaderboard_overlay_open && IsKeyPressed(KEY_ENTER)) {
    game->leaderboard_overlay_open = false;
  }

  if (is_online && (game->net.status != CLIENT_NET_CONNECTED) && IsKeyPressed(KEY_B)) {
    game->return_to_menu_requested = true;
  }
}

static void GameplayCleanup(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  GameShutdown(game);
}

void ShroomScreenRegisterGame(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_GAME];
  screen->type = SHROOM_SCREEN_GAME;
  screen->name = "Game";
  screen->init = GameplayInit;
  screen->update = GameplayUpdate;
  screen->draw = GameplayDraw;
  screen->handle_input = GameplayHandleInput;
  screen->cleanup = GameplayCleanup;
}
