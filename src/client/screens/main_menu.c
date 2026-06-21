#include "game.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

static bool MainMenuInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void MainMenuUpdate(ShroomScreenManager* manager, float delta_time) {
  (void)manager;
  (void)delta_time;
}

static void MainMenuDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const float panel_width = 340.0f;
  const float panel_height = 470.0f;

  ShroomScreenDrawFungalBackground((game == NULL) || game->settings.menu_animations_enabled);

  if (!ShroomLayoutBeginCenteredPanel("Main Menu", panel_width, panel_height, 0.85f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  ShroomImGui_Text("SHROOMIO");
  ShroomImGui_TextWrapped(
      "Grow by collecting spores, out-position bigger threats, and take over the arena.");
  ShroomImGui_Spacing();

  if (ShroomLayoutButtonFullWidth("Play Online", 38.0f)) {
    GamePlayUiClickSound(game);
    if (game != NULL) {
      ClientNetInit(&game->net, game->selected_server_host, game->selected_server_port);
      game->auto_join_lobby = true;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_LOBBY);
  }
  if (ShroomLayoutButtonFullWidth("Custom Server", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  if (ShroomLayoutButtonFullWidth("Offline Practice", 38.0f)) {
    GamePlayUiClickSound(game);
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }
  if (ShroomLayoutButtonFullWidth("Settings", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SETTINGS);
  }
  if (ShroomLayoutButtonFullWidth("Help", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_HELP);
  }
  if (ShroomLayoutButtonFullWidth("Credits", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_CREDITS);
  }
  if (ShroomLayoutButtonFullWidth("Exit", 38.0f)) {
    GamePlayUiClickSound(game);
    ShroomScreenManagerRequestExit(manager);
  }

  ShroomImGui_End();
}

static void MainMenuHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerRequestExit(manager);
  }
}

static void MainMenuCleanup(ShroomScreenManager* manager) { (void)manager; }

void ShroomScreenRegisterMainMenu(ShroomScreenManager* manager) {
  ShroomScreen* screen;

  if (manager == NULL) {
    return;
  }

  screen = &manager->screens[SHROOM_SCREEN_MAIN_MENU];
  screen->type = SHROOM_SCREEN_MAIN_MENU;
  screen->name = "Main Menu";
  screen->init = MainMenuInit;
  screen->update = MainMenuUpdate;
  screen->draw = MainMenuDraw;
  screen->handle_input = MainMenuHandleInput;
  screen->cleanup = MainMenuCleanup;
}
