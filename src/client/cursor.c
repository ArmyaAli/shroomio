#include "cursor.h"

#include "raylib.h"

static ShroomCursorMode g_cursor_mode = SHROOM_CURSOR_DEFAULT;

static void DrawMushroomCursor(Vector2 mouse, Color cap, Color stem, Color accent, float alpha) {
  const Vector2 tip = mouse;
  const Vector2 base = {tip.x + 9.0f, tip.y + 22.0f};
  const Color shadow = Fade((Color){6, 8, 7, 255}, 0.36f * alpha);

  DrawTriangle(tip, (Vector2){tip.x + 2.0f, tip.y + 23.0f}, (Vector2){tip.x + 18.0f, tip.y + 15.0f},
               shadow);
  DrawEllipse((int)base.x, (int)(base.y - 1.0f), 7.0f, 10.0f, Fade(stem, alpha));
  DrawEllipseLines((int)base.x, (int)(base.y - 1.0f), 7.0f, 10.0f, Fade(BROWN, 0.52f * alpha));
  DrawCircleSector((Vector2){tip.x + 11.0f, tip.y + 11.0f}, 13.0f, 198.0f, 344.0f, 18,
                   Fade(cap, alpha));
  DrawCircleSectorLines((Vector2){tip.x + 11.0f, tip.y + 11.0f}, 13.0f, 198.0f, 344.0f, 18,
                        Fade(accent, 0.86f * alpha));
  DrawCircleV((Vector2){tip.x + 7.0f, tip.y + 8.0f}, 2.1f, Fade(RAYWHITE, 0.82f * alpha));
  DrawCircleV((Vector2){tip.x + 14.0f, tip.y + 9.0f}, 1.7f, Fade(RAYWHITE, 0.76f * alpha));
  DrawCircleV((Vector2){tip.x + 12.0f, tip.y + 14.0f}, 1.5f, Fade(RAYWHITE, 0.68f * alpha));
}

static void DrawSporeCursor(Vector2 mouse) {
  DrawTriangle(mouse, (Vector2){mouse.x + 4.0f, mouse.y + 26.0f},
               (Vector2){mouse.x + 22.0f, mouse.y + 19.0f}, Fade((Color){8, 14, 10, 255}, 0.38f));
  DrawCircleV((Vector2){mouse.x + 12.0f, mouse.y + 12.0f}, 7.5f,
              Fade((Color){202, 236, 156, 255}, 0.96f));
  DrawCircleLines((int)(mouse.x + 12.0f), (int)(mouse.y + 12.0f), 8.0f,
                  Fade((Color){88, 152, 78, 255}, 0.88f));
  DrawLineEx((Vector2){mouse.x + 4.0f, mouse.y + 22.0f}, (Vector2){mouse.x + 18.0f, mouse.y + 6.0f},
             3.0f, Fade((Color){106, 176, 92, 255}, 0.88f));
  DrawCircleV((Vector2){mouse.x + 16.0f, mouse.y + 8.0f}, 2.0f, Fade(RAYWHITE, 0.78f));
}

static void DrawConsumeCursor(Vector2 mouse) {
  DrawMushroomCursor(mouse, (Color){222, 68, 58, 255}, (Color){245, 225, 178, 255},
                     (Color){255, 198, 100, 255}, 1.0f);
  DrawCircleV((Vector2){mouse.x + 11.0f, mouse.y + 17.0f}, 6.0f,
              Fade((Color){36, 8, 9, 255}, 0.82f));
  DrawTriangle((Vector2){mouse.x + 7.0f, mouse.y + 15.0f},
               (Vector2){mouse.x + 10.0f, mouse.y + 15.0f},
               (Vector2){mouse.x + 8.5f, mouse.y + 19.0f}, Fade(RAYWHITE, 0.92f));
  DrawTriangle((Vector2){mouse.x + 13.0f, mouse.y + 15.0f},
               (Vector2){mouse.x + 16.0f, mouse.y + 15.0f},
               (Vector2){mouse.x + 14.5f, mouse.y + 19.0f}, Fade(RAYWHITE, 0.92f));
}

void ShroomCursorInit(void) { HideCursor(); }

void ShroomCursorShutdown(void) { ShowCursor(); }

void ShroomCursorBeginFrame(void) { g_cursor_mode = SHROOM_CURSOR_DEFAULT; }

void ShroomCursorSetMode(ShroomCursorMode mode) { g_cursor_mode = mode; }

void ShroomCursorDraw(void) {
  const Vector2 mouse = GetMousePosition();

  if (!IsCursorHidden()) {
    HideCursor();
  }

  switch (g_cursor_mode) {
  case SHROOM_CURSOR_GAMEPLAY:
    DrawSporeCursor(mouse);
    break;
  case SHROOM_CURSOR_CONSUME:
    DrawConsumeCursor(mouse);
    break;
  case SHROOM_CURSOR_DISABLED:
    DrawMushroomCursor(mouse, (Color){98, 112, 96, 255}, (Color){156, 150, 126, 255},
                       (Color){112, 126, 108, 255}, 0.58f);
    break;
  case SHROOM_CURSOR_DEFAULT:
  default:
    DrawMushroomCursor(mouse, (Color){206, 82, 74, 255}, (Color){246, 226, 178, 255},
                       (Color){255, 214, 128, 255}, 1.0f);
    break;
  }
}
