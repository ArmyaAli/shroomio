#include <stddef.h>
#include "screen.h"
#include "raylib.h"
#include "raygui.h"
#include <stddef.h>

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
  (void)manager;
  
  BeginDrawing();
  ClearBackground((Color){20, 20, 40, 255});
  
  int screen_width = GetScreenWidth();
  int screen_height = GetScreenHeight();
  
  int button_width = 250;
  int button_height = 50;
  int button_x = screen_width / 2 - button_width / 2;
  int button_y = 200;
  int button_spacing = 70;
  
  GuiLabel((Rectangle){screen_width / 2 - 150, 80, 300, 60}, "SHROOMIO");
  GuiSetStyle(DEFAULT, TEXT_SIZE, 30);
  DrawText("SHROOMIO", screen_width / 2 - MeasureText("SHROOMIO", 60) / 2, 80, 60, WHITE);
  GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
  
  if (GuiButton((Rectangle){button_x, button_y, button_width, button_height}, "PLAY")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SERVER_BROWSER);
  }
  
  if (GuiButton((Rectangle){button_x, button_y + button_spacing, button_width, button_height}, "SETTINGS")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_SETTINGS);
  }
  
  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 2, button_width, button_height}, "HELP")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_HELP);
  }
  
  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 3, button_width, button_height}, "CREDITS")) {
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_CREDITS);
  }
  
  if (GuiButton((Rectangle){button_x, button_y + button_spacing * 4, button_width, button_height}, "EXIT")) {
    ShroomScreenManagerRequestExit(manager);
  }
  
  EndDrawing();
}

static void MainMenuHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerRequestExit(manager);
  }
}

static void MainMenuCleanup(ShroomScreenManager* manager) {
  (void)manager;
}

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
