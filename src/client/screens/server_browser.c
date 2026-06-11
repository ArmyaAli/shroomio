#include <stddef.h>
#include "screen.h"
#include "raylib.h"
#include "raygui.h"

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  (void)manager;

  BeginDrawing();
  ClearBackground((Color){30, 30, 50, 255});

  int screen_width = GetScreenWidth();

  GuiLabel((Rectangle){screen_width / 2 - 100, 50, 200, 40}, "SERVER BROWSER");

  DrawText("Server list coming soon...", 100, 150, 20, GRAY);

  int button_width = 200;
  int button_x = screen_width / 2 - button_width / 2;

  if (GuiButton((Rectangle){button_x, 400, button_width, 50}, "BACK")) {
    ShroomScreenManagerGoBack(manager);
  }

  EndDrawing();
}

static void ServerBrowserHandleInput(ShroomScreenManager* manager) {
  if (IsKeyPressed(KEY_ESCAPE)) {
    ShroomScreenManagerGoBack(manager);
  }
}

void ShroomScreenRegisterServerBrowser(ShroomScreenManager* manager) {
  if (manager == NULL)
    return;

  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_SERVER_BROWSER];
  screen->type = SHROOM_SCREEN_SERVER_BROWSER;
  screen->name = "Server Browser";
  screen->init = ServerBrowserInit;
  screen->draw = ServerBrowserDraw;
  screen->handle_input = ServerBrowserHandleInput;
}
