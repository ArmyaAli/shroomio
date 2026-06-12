#include <stddef.h>

#include "screen.h"

#include "raygui.h"
#include "raylib.h"

static void DrawHelpSection(int x, int y, int width, int height, const char* title,
                            const char* line_1, const char* line_2, const char* line_3,
                            const char* line_4) {
  DrawRectangle(x, y, width, height, Fade(BLACK, 0.24f));
  DrawRectangleLines(x, y, width, height, Fade(RAYWHITE, 0.14f));
  DrawText(title, x + 18, y + 14, 24, RAYWHITE);
  DrawText(line_1, x + 18, y + 52, 19, LIGHTGRAY);
  DrawText(line_2, x + 18, y + 78, 19, LIGHTGRAY);
  DrawText(line_3, x + 18, y + 104, 19, LIGHTGRAY);
  DrawText(line_4, x + 18, y + 130, 19, LIGHTGRAY);
}

static bool HelpInit(ShroomScreenManager* manager) {
  (void)manager;
  return true;
}

static void HelpDraw(ShroomScreenManager* manager) {
  const int screen_width = GetScreenWidth();
  const int left_column_x = 74;
  const int right_column_x = screen_width / 2 + 18;
  const int section_width = screen_width / 2 - 92;

  BeginDrawing();
  ClearBackground((Color){28, 30, 46, 255});

  DrawText("HOW TO PLAY", screen_width / 2 - 118, 38, 36, RAYWHITE);
  DrawText("A quick in-client reference for movement, growth, zones, and online sessions.",
           screen_width / 2 - 330, 82, 22, GRAY);

  DrawHelpSection(left_column_x, 132, section_width, 168, "Controls",
                  "Mouse aim sets movement direction around your colony.",
                  "WASD or arrow keys nudge movement for finer control.",
                  "Tab opens the leaderboard overlay during a match.",
                  "Esc opens pause or match options depending on mode.");

  DrawHelpSection(right_column_x, 132, section_width, 168, "Growth / Consume Rules",
                  "Collect spores to gain mass and increase your collision size.",
                  "You can consume another player only when you are meaningfully larger.",
                  "If a larger player catches you, you respawn in the outer ring.",
                  "Growth trades mobility for size, so reposition early.");

  DrawHelpSection(left_column_x, 322, section_width, 168, "Arena Zones",
                  "Outer ring is the safest space for recovery and respawns.",
                  "Mid ring balances escape routes with moderate player traffic.",
                  "Center ring has the highest contest and biggest swing potential.",
                  "Zone banners appear when you cross between risk bands.");

  DrawHelpSection(right_column_x, 322, section_width, 168, "Online Play Tips",
                  "Quick Play and Server Browser connect to real online sessions.",
                  "Offline Practice is local only and never attempts a server connect.",
                  "Direct Connect accepts a known host and port when browsing is incomplete.",
                  "Recent servers in the browser are saved for quick reconnects.");

  DrawRectangle(74, 514, screen_width - 148, 72, Fade(BLACK, 0.18f));
  DrawRectangleLines(74, 514, screen_width - 148, 72, Fade(RAYWHITE, 0.12f));
  DrawText("Match shortcuts", 92, 532, 22, RAYWHITE);
  DrawText("Enter resumes overlays, M returns to the main menu, and R retries failed online joins.",
           92, 562, 19, LIGHTGRAY);

  if (GuiButton((Rectangle){screen_width / 2 - 100, 618, 200, 46}, "BACK")) {
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
  if (manager == NULL) {
    return;
  }

  ShroomScreen* screen = &manager->screens[SHROOM_SCREEN_HELP];
  screen->type = SHROOM_SCREEN_HELP;
  screen->name = "Help";
  screen->init = HelpInit;
  screen->draw = HelpDraw;
  screen->handle_input = HelpHandleInput;
}
