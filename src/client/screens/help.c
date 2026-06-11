#include <stddef.h>
#include "screen.h"
#include "raylib.h"
#include "raygui.h"

static bool HelpInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void HelpDraw(ShroomScreenManager* manager) {
  (void)manager;
  
  BeginDrawing();
  ClearBackground((Color){30, 30, 50, 255});
  
  int screen_width = GetScreenWidth();
  
  GuiLabel((Rectangle){screen_width / 2 - 100, 50, 200, 40}, "HOW TO PLAY");
  
  DrawText("Collect spores to grow", 100, 150, 20, WHITE);
  DrawText("Consume smaller players", 100, 180, 20, WHITE);
  DrawText("Avoid larger players", 100, 210, 20, WHITE);
  DrawText("Use mouse to move", 100, 240, 20, WHITE);
  
  int button_width = 200;
  int button_x = screen_width / 2 - button_width / 2;
  
  if (GuiButton((Rectangle){button_x, 400, button_width, 50}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }
  
  EndDrawing();
}

static void HelpHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterHelp(ShroomScreenManager* manager) {
  if (manager == NULL) return;
  
  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_HELP];
  screen->type = SHROOM_SCREEN_HELP;
  screen->name = "Help";
  screen->init = HelpInit;
  screen->draw = HelpDraw;
  screen->handle_input = HelpHandleInput;
}
