#include "game.h"
#include "screen.h"

#include "raylib.h"

static void RestartQuickPlaySession(Game* game) {
  GameShutdown(game);
  GameInit(game, GetScreenWidth(), GetScreenHeight(), SHROOM_SESSION_MODE_QUICK_PLAY);
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
}

static void GameplayHandleInput(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return;
  }

  if ((game->active_mode == SHROOM_SESSION_MODE_QUICK_PLAY) && !game->net.welcome_received) {
    if ((game->net.status == CLIENT_NET_ERROR) || (game->net.status == CLIENT_NET_DISCONNECTED)) {
      if (IsKeyPressed(KEY_R)) {
        RestartQuickPlaySession(game);
      }
      if (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
        ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
      }
      return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
      return;
    }
  }

  if (game->leave_confirmation_open) {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Y)) {
      ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
    } else if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_N)) {
      game->leave_confirmation_open = false;
      game->menu_overlay_open = true;
    }
    return;
  }

  if (IsKeyPressed(KEY_TAB)) {
    game->leaderboard_overlay_open = !game->leaderboard_overlay_open;
    if (game->leaderboard_overlay_open) {
      game->menu_overlay_open = false;
    }
  }

  if (IsKeyPressed(KEY_F3)) {
    game->diagnostics_overlay_open = !game->diagnostics_overlay_open;
    game->settings.diagnostics_enabled = game->diagnostics_overlay_open;
    ClientSettingsSave(&game->settings);
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    if (game->leaderboard_overlay_open) {
      game->leaderboard_overlay_open = false;
    } else {
      game->menu_overlay_open = !game->menu_overlay_open;
      if (game->menu_overlay_open) {
        game->leave_confirmation_open = false;
        game->leaderboard_overlay_open = false;
      }
    }
  }

  if (game->menu_overlay_open) {
    if (IsKeyPressed(KEY_ENTER)) {
      game->menu_overlay_open = false;
    }

    if (game->active_mode == SHROOM_SESSION_MODE_OFFLINE_PRACTICE) {
      if (IsKeyPressed(KEY_M)) {
        ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
      }
      return;
    }

    if (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_M)) {
      game->menu_overlay_open = false;
      game->leave_confirmation_open = true;
    }
  }

  if (game->leaderboard_overlay_open && IsKeyPressed(KEY_ENTER)) {
    game->leaderboard_overlay_open = false;
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
