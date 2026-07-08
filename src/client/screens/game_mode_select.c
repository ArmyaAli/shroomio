#include "game.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

static bool GameModeSelectInit(ShroomScreenManager* manager) {
  (void)manager;
  ShroomScreenResetFungalBackground();
  return true;
}

static void GameModeSelectUpdate(ShroomScreenManager* manager, float delta_time) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;
  ShroomScreenUpdateFungalBackground(delta_time, game != NULL && game->settings.menu_animations_enabled);
}

static void GameModeSelectDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const bool animate = game != NULL && game->settings.menu_animations_enabled;
  const float panel_width = 500.0f;
  const float panel_height = 600.0f;

  ShroomScreenDrawFungalBackground(animate);

  if (!ShroomLayoutBeginCenteredPanel("Select Game Mode", panel_width, panel_height, 0.85f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_TextWrapped("Choose a game mode to play:");
  ShroomImGui_Spacing();

  if (ShroomLayoutButtonFullWidth("Free-for-All (FFA)", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_FFA;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (ShroomLayoutButtonFullWidth("Teams 2v2", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_TEAMS_2V2;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (ShroomLayoutButtonFullWidth("Teams 3v3", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_TEAMS_3V3;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (ShroomLayoutButtonFullWidth("Teams 4v4", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_TEAMS_4V4;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (ShroomLayoutButtonFullWidth("Battle Royale", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_BATTLE_ROYALE;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (ShroomLayoutButtonFullWidth("King of the Hill", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_KING_OF_HILL;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (ShroomLayoutButtonFullWidth("Mass Race", 38.0f)) {
    GamePlayUiClickSound(game);
    game->selected_game_mode = SHROOM_GAME_MODE_MASS_RACE;
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }

  ShroomImGui_Spacing();
  if (ShroomLayoutButtonFullWidth("Back", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_MAIN_MENU);
  }

  ShroomImGui_End();
}

static void GameModeSelectHandleInput(ShroomScreenManager* manager) {
  (void)manager;
}

void ShroomScreenRegisterGameModeSelect(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_GAME_MODE_SELECT];
  screen->type = SHROOM_SCREEN_GAME_MODE_SELECT;
  screen->name = "Game Mode Select";
  screen->init = GameModeSelectInit;
  screen->update = GameModeSelectUpdate;
  screen->draw = GameModeSelectDraw;
  screen->handle_input = GameModeSelectHandleInput;
}
