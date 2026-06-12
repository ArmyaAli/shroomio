#include <stddef.h>

#include "game.h"
#include "screen.h"

#include "raygui.h"
#include "raylib.h"

static bool ServerBrowserInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void ServerBrowserDraw(ShroomScreenManager* manager) {
  Game* game = manager != NULL ? (Game*)manager->user_data : NULL;

  BeginDrawing();
  ClearBackground((Color){30, 30, 50, 255});

  int screen_width = GetScreenWidth();
  int panel_x = screen_width / 2 - 260;

  GuiLabel((Rectangle){screen_width / 2 - 110, 50, 220, 40}, "SERVER BROWSER");

  DrawRectangle(panel_x, 128, 520, 214, Fade(BLACK, 0.32f));
  DrawRectangleLines(panel_x, 128, 520, 214, Fade(RAYWHITE, 0.18f));
  DrawText("Selected Server", panel_x + 24, 148, 26, RAYWHITE);
  DrawText("Local Development", panel_x + 24, 186, 24, RAYWHITE);
  DrawText("Address 127.0.0.1:7777", panel_x + 24, 220, 20, GRAY);
  DrawText("Use this to test the local multiplayer server path.", panel_x + 24, 252, 20, GRAY);
  DrawText("If the server is down, Quick Play will show a retryable connection state.",
           panel_x + 24, 280, 18, LIGHTGRAY);

  int button_width = 200;
  int button_x = screen_width / 2 - button_width / 2;

  if (GuiButton((Rectangle){button_x, 390, button_width, 50}, "JOIN SELECTED")) {
    if (game != NULL) {
      game->selected_mode = SHROOM_SESSION_MODE_QUICK_PLAY;
    }
    ShroomScreenManagerTransition(manager, SHROOM_SCREEN_GAME);
  }

  if (GuiButton((Rectangle){button_x, 454, button_width, 50}, "BACK")) {
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
