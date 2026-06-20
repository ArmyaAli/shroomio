#include "screen_background.h"

#include "raylib.h"

#include <math.h>
#include <stdint.h>

static float WrappedUnit(float value) {
  value = fmodf(value, 1.0f);
  if (value < 0.0f) {
    value += 1.0f;
  }
  return value;
}

static void DrawFungalMushroom(float x, float y, float scale, float sway, int type) {
  Color cap_color;
  Color stem_color;
  const float sway_offset = sinf(sway) * 5.0f * scale;

  if (type == 0) {
    cap_color = (Color){180, 100, 200, 150};
    stem_color = (Color){220, 200, 180, 150};
  } else if (type == 1) {
    cap_color = (Color){100, 180, 150, 150};
    stem_color = (Color){200, 220, 180, 150};
  } else {
    cap_color = (Color){200, 150, 100, 150};
    stem_color = (Color){220, 200, 180, 150};
  }

  DrawRectangle((int)(x - 8.0f * scale + sway_offset), (int)y, (int)(16.0f * scale),
                (int)(40.0f * scale), stem_color);
  DrawEllipse((int)(x + sway_offset), (int)y, 40.0f * scale, 30.0f * scale, cap_color);
  DrawEllipse((int)(x - 10.0f * scale + sway_offset), (int)(y - 5.0f * scale), 8.0f * scale,
              6.0f * scale, (Color){255, 255, 255, 100});
  DrawEllipse((int)(x + 12.0f * scale + sway_offset), (int)(y - 3.0f * scale), 6.0f * scale,
              5.0f * scale, (Color){255, 255, 255, 100});
}

void ShroomScreenDrawFungalBackground(bool animate) {
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  const float time = animate ? (float)GetTime() : 0.0f;
  const Color gradient_top = (Color){30, 20, 50, 255};
  const Color gradient_bottom = (Color){50, 30, 70, 255};

  for (int y = 0; y < screen_height; y++) {
    const float t = screen_height > 0 ? (float)y / (float)screen_height : 0.0f;
    Color color;
    color.r = (uint8_t)(gradient_top.r + (gradient_bottom.r - gradient_top.r) * t);
    color.g = (uint8_t)(gradient_top.g + (gradient_bottom.g - gradient_top.g) * t);
    color.b = (uint8_t)(gradient_top.b + (gradient_bottom.b - gradient_top.b) * t);
    color.a = 255;
    DrawLine(0, y, screen_width, y, color);
  }

  for (int index = 0; index < 8; index++) {
    const float x_ratio = WrappedUnit(0.11f + (float)index * 0.137f);
    const float y_ratio = 0.70f + WrappedUnit(0.19f + (float)index * 0.21f) * 0.28f;
    const float scale = 0.55f + WrappedUnit(0.31f + (float)index * 0.17f) * 0.95f;
    DrawFungalMushroom(x_ratio * (float)screen_width, y_ratio * (float)screen_height, scale,
                       time * (0.45f + (float)index * 0.04f) + (float)index, index % 3);
  }

  for (int index = 0; index < 36; index++) {
    const float x_ratio =
        WrappedUnit(0.07f + (float)index * 0.173f + sinf(time * 0.18f + index) * 0.018f);
    const float y_ratio =
        WrappedUnit(0.13f + (float)index * 0.097f - time * (0.018f + (float)(index % 5) * 0.003f));
    const float size = 2.0f + (float)(index % 4);
    const float alpha = 0.28f + (float)(index % 5) * 0.07f;
    const Vector2 position = {x_ratio * (float)screen_width, y_ratio * (float)screen_height};

    DrawCircleV(position, size * 1.7f, Fade((Color){220, 200, 255, 255}, alpha * 0.35f));
    DrawCircleV(position, size, Fade((Color){200, 180, 255, 255}, alpha));
  }
}
