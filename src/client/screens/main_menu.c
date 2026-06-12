#include <stddef.h>

#include "game.h"
#include "screen.h"

#include "raygui.h"
#include "raylib.h"

static bool MainMenuInit(ShroomScreenManager* manager) {
  (void)manager;
  GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
  return true;
}

static void MainMenuUpdate(ShroomScreenManager* manager, float delta_time) {
  (void)manager;
  (void)delta_time;
}

static void MainMenuDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  BeginDrawing();
  ClearBackground((Color){20, 20, 40, 255});

  int screen_width = GetScreenWidth();

  int button_width = 280;
  int button_height = 48;
  int button_x = screen_width / 2 - button_width / 2;
  int button_y = 188;
  int button_spacing = 58;

  GuiLabel((Rectangle){screen_width / 2 - 150, 80, 300, 60}, "SHROOMIO");
  GuiSetStyle(DEFAULT, TEXT_SIZE, 30);
  DrawText("SHROOMIO", screen_width / 2 - MeasureText("SHROOMIO", 60) / 2, 80, 60, WHITE);
  GuiSetStyle(DEFAULT, TEXT_SIZE, 20);

  DrawText("Choose how to play", screen_width / 2 - 112, 148, 22, GRAY);

  if (GuiButton((Rectangle){button_x, button_y, button_width, button_height}, "QUICK PLAY")) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }

  if (GuiButton((Rectangle){button_x, button_y + button_spacing, button_width, button_height},
                "SERVER BROWSER")) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }

  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 2, button_width, button_height},
                "OFFLINE PRACTICE")) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_OFFLINE_PRACTICE;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }

  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 3, button_width, button_height},
                "SETTINGS")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SETTINGS);
  }

  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 4, button_width, button_height},
                "HELP")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_HELP);
  }

  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 5, button_width, button_height},
                "CREDITS")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_CREDITS);
  }

  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 6, button_width, button_height},
                "EXIT")) {
    ShroomScreenManagerRequestExit(manager);
  }

  EndDrawing();
}

static void MainMenuHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerRequestExit(manager);
  }
}

static void MainMenuCleanup(ShroomScreenManager* manager) { (void)manager; }

void ShroomScreenRegisterMainMenu(ShroomScreenManager* manager) {
  if (manager == NULL) {
    return;
  }

  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_MAIN_MENU];
  screen->type = SHROOM_SCREEN_MAIN_MENU;
  screen->name = "Main Menu";
  screen->init = MainMenuInit;
  screen->update = MainMenuUpdate;
  screen->draw = MainMenuDraw;
  screen->handle_input = MainMenuHandleInput;
  screen->cleanup = MainMenuCleanup;
}
