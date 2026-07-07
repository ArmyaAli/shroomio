#include "cursor.h"

static void DrawCapCursor(Vector2 mouse, Color cap, Color stem, Color accent, bool split_mark) {
  DrawCircleV((Vector2){mouse.x + 8.0f, mouse.y + 7.0f}, 8.0f, Fade(BLACK, 0.35f));
  DrawCircleSector((Vector2){mouse.x + 8.0f, mouse.y + 7.0f}, 9.0f, 180.0f, 360.0f, 18, cap);
  DrawRectangleRounded((Rectangle){mouse.x + 5.0f, mouse.y + 7.0f, 6.0f, 12.0f}, 0.45f, 6, stem);
  DrawCircleV((Vector2){mouse.x + 5.0f, mouse.y + 4.0f}, 2.0f, accent);
  DrawCircleV((Vector2){mouse.x + 10.0f, mouse.y + 3.0f}, 1.5f, accent);
  DrawTriangle(mouse, (Vector2){mouse.x + 2.0f, mouse.y + 14.0f},
               (Vector2){mouse.x + 11.0f, mouse.y + 11.0f}, Fade(stem, 0.92f));

  if (split_mark) {
    DrawLineEx((Vector2){mouse.x + 17.0f, mouse.y + 7.0f},
               (Vector2){mouse.x + 30.0f, mouse.y + 7.0f}, 2.0f, accent);
    DrawTriangle((Vector2){mouse.x + 31.0f, mouse.y + 7.0f},
                 (Vector2){mouse.x + 25.0f, mouse.y + 3.0f},
                 (Vector2){mouse.x + 25.0f, mouse.y + 11.0f}, accent);
  }
}

void ShroomCursorHideSystem(void) { HideCursor(); }

void ShroomCursorShowSystem(void) { ShowCursor(); }

void ShroomCursorDraw(ShroomCursorMode mode) {
  const Vector2 mouse = GetMousePosition();
  Color cap = (Color){106, 188, 88, 255};
  Color stem = (Color){244, 221, 176, 255};
  Color accent = (Color){255, 232, 160, 255};
  bool split_mark = false;

  switch (mode) {
  case SHROOM_CURSOR_CONSUME:
    cap = (Color){228, 55, 48, 255};
    accent = (Color){255, 244, 210, 255};
    break;
  case SHROOM_CURSOR_SPLIT:
    cap = (Color){230, 174, 46, 255};
    accent = (Color){255, 255, 220, 255};
    split_mark = true;
    break;
  case SHROOM_CURSOR_DISABLED:
    cap = (Color){124, 110, 112, 255};
    stem = (Color){190, 176, 150, 255};
    accent = (Color){192, 184, 160, 255};
    break;
  case SHROOM_CURSOR_GAMEPLAY:
    cap = (Color){106, 188, 88, 255};
    break;
  case SHROOM_CURSOR_DEFAULT:
  default:
    cap = (Color){196, 72, 86, 255};
    break;
  }

  DrawCapCursor(mouse, cap, stem, accent, split_mark);
}
