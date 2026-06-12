#include "game.h"
#include "screen.h"

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
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  const float panel_width = 340.0f;
  const float panel_height = 470.0f;

  ClearBackground((Color){18, 20, 32, 255});

  ShroomImGui_SetNextWindowPos((screen_width - (int)panel_width) * 0.5f,
                               (screen_height - (int)panel_height) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(panel_width, panel_height, SHROOM_IMGUI_COND_ALWAYS);
  if (!ShroomImGui_Begin("Main Menu", NULL,
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

  if (ShroomImGui_Button("Quick Play", -1.0f, 38.0f)) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }
  if (ShroomImGui_Button("Server Browser", -1.0f, 38.0f)) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  if (ShroomImGui_Button("Offline Practice", -1.0f, 38.0f)) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }
  if (ShroomImGui_Button("Settings", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SETTINGS);
  }
  if (ShroomImGui_Button("Help", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_HELP);
  }
  if (ShroomImGui_Button("Credits", -1.0f, 38.0f)) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_CREDITS);
  }
  if (ShroomImGui_Button("Exit", -1.0f, 38.0f)) {
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
