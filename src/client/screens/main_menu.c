#include "game.h"
#include "layout.h"
#include "screen.h"
#include "screen_background.h"

#include "imgui_wrapper.h"
#include "raylib.h"

#include <math.h>

static bool MainMenuAnimationsEnabled(const Game* game) {
  return (game == NULL) || game->settings.menu_animations_enabled;
}

static void MainMenuDrawAnimatedTitle(bool animate) {
  if (!animate) {
    ShroomImGui_Text("SHROOMIO");
    return;
  }

  const float time = (float)GetTime();
  const float glow = 0.72f + 0.28f * sinf(time * 2.0f);
  const int spore_index = (int)(time * 9.0f) % 18;
  char spore_trail[24];

  for (int index = 0; index < 18; ++index) {
    spore_trail[index] = index == spore_index ? '*' : '.';
  }
  spore_trail[18] = '\0';

  ShroomImGui_TextColored((ShroomImGuiColor){0.78f, 1.0f, 0.56f, 1.0f}, "SHROOMIO");
  ShroomImGui_TextColored((ShroomImGuiColor){0.60f, 0.95f, 0.72f, glow}, spore_trail);
}

static bool MainMenuInit(ShroomScreenManager* manager) {
  (void)manager;
  ShroomScreenResetFungalBackground();
  return true;
}

static void MainMenuUpdate(ShroomScreenManager* manager, float delta_time) {
  const Game* game = manager != NULL ? (const Game*)manager->user_data : NULL;
  ShroomScreenUpdateFungalBackground(delta_time, MainMenuAnimationsEnabled(game));
}

static void MainMenuDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;
  const bool animate = MainMenuAnimationsEnabled(game);
  const float panel_width = 340.0f;
  const float panel_height = 470.0f;

  ShroomScreenDrawFungalBackground(animate);

  if (!ShroomLayoutBeginCenteredPanel("Main Menu", panel_width, panel_height, 0.85f,
                                      SHROOM_IMGUI_WINDOW_NO_RESIZE | SHROOM_IMGUI_WINDOW_NO_MOVE |
                                          SHROOM_IMGUI_WINDOW_NO_COLLAPSE |
                                          SHROOM_IMGUI_WINDOW_NO_SAVED_SETTINGS)) {
    ShroomImGui_End();
    return;
  }

  MainMenuDrawAnimatedTitle(animate);
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
