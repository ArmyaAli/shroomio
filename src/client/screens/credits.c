#include <stddef.h>
#include "screen.h"
#include "raylib.h"
#include "raygui.h"

static bool CreditsInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void CreditsDraw(ShroomScreenManager* manager) {
  (void)manager;
  
  BeginDrawing();
  ClearBackground((Color){30, 30, 50, 255});
  
  int screen_width = GetScreenWidth();
  
  GuiLabel((Rectangle){screen_width / 2 - 100, 50, 200, 40}, "CREDITS");
  
  DrawText("shroomio", 100, 150, 30, WHITE);
  DrawText("A multiplayer arena game", 100, 200, 20, GRAY);
  DrawText("Built with raylib and raygui", 100, 240, 20, GRAY);
  
  int button_width = 200;
  int button_x = screen_width / 2 - button_width / 2;
  
  if (GuiButton((Rectangle){button_x, 400, button_width, 50}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }
  
  EndDrawing();
}

static void CreditsHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterCredits(ShroomScreenManager* manager) {
  if (manager == NULL) return;
  
  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_CREDITS];
  screen->type = SHROOM_SCREEN_CREDITS;
  screen->name = "Credits";
  screen->init = CreditsInit;
  screen->draw = CreditsDraw;
  screen->handle_input = CreditsHandleInput;
}
