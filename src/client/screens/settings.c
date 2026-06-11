#include <stddef.h>
#include "screen.h"
#include "raylib.h"
#include "raygui.h"

static bool SettingsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void SettingsDraw(ShroomScreenManager* manager) {
  (void)manager;

  BeginDrawing();
  ClearBackground((Color){30, 30, 50, 255});

  int screen_width = GetScreenWidth();

  GuiLabel((Rectangle){screen_width / 2 - 100, 50, 200, 40}, "SETTINGS");

  int button_width = 200;
  int button_x = screen_width / 2 - button_width / 2;

  if (GuiButton((Rectangle){button_x, 400, button_width, 50}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }

  EndDrawing();
}

static void SettingsHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterSettings(ShroomScreenManager* manager) {
  if (manager == NULL)
    return;

  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_SETTINGS];
  screen->type = SHROOM_SCREEN_SETTINGS;
  screen->name = "Settings";
  screen->init = SettingsInit;
  screen->draw = SettingsDraw;
  screen->handle_input = SettingsHandleInput;
}
