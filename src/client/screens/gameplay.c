#include "game.h"
#include "screen.h"

#include "raylib.h"

static bool GameplayInit(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  if (game == NULL) {
    return false;
  }

  GameInit(game, GetScreenWidth(), GetScreenHeight());
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
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
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
