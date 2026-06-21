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
  const float sway_offset = sinf(sway) * 12.0f * scale;

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
  static float animation_time = 0.0f;
  const int screen_width = GetScreenWidth();
  const int screen_height = GetScreenHeight();
  const float frame_time = fminf(fmaxf(GetFrameTime(), 0.0f), 0.05f);
  const float time = animate ? animation_time : 0.0f;
  const float pulse = animate ? (0.5f + 0.5f * sinf(time * 0.5f)) : 0.0f;
  const Color gradient_top =
      (Color){(uint8_t)(30 + pulse * 16.0f), 20, (uint8_t)(50 + pulse * 24.0f), 255};
  const Color gradient_bottom =
      (Color){50, (uint8_t)(30 + pulse * 18.0f), (uint8_t)(70 + pulse * 20.0f), 255};

  if (animate) {
    animation_time += frame_time;
  }

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
    const float x_ratio = WrappedUnit(0.11f + (float)index * 0.137f + time * 0.02f);
    const float y_ratio = 0.70f + WrappedUnit(0.19f + (float)index * 0.21f) * 0.28f;
    const float scale = 0.55f + WrappedUnit(0.31f + (float)index * 0.17f) * 0.95f;
    DrawFungalMushroom(x_ratio * (float)screen_width, y_ratio * (float)screen_height, scale,
                       time * (0.6f + (float)index * 0.07f) + (float)index, index % 3);
  }

  if (animate) {
    for (int index = 0; index < 4; index++) {
      const float x =
          WrappedUnit(-0.2f + (float)index * 0.34f + time * 0.08f) * (float)screen_width;
      const float y =
          (0.18f + (float)index * 0.16f + sinf(time * 0.5f + index) * 0.04f) * (float)screen_height;
      const float radius = (float)screen_width * (0.18f + (float)index * 0.02f);

      DrawCircleV((Vector2){x, y}, radius, Fade((Color){80, 180, 140, 255}, 0.055f));
      DrawCircleV((Vector2){x + 42.0f, y + 28.0f}, radius * 0.52f,
                  Fade((Color){190, 150, 255, 255}, 0.045f));
    }
  }

  for (int index = 0; index < 10; index++) {
    const float phase = (float)index * 0.63f;
    const float x = WrappedUnit(0.08f + (float)index * 0.113f + time * 0.03f) * (float)screen_width;
    const float y =
        (0.18f + WrappedUnit(0.23f + (float)index * 0.171f) * 0.56f) * (float)screen_height;
    const float radius = 42.0f + sinf(time * 1.4f + phase) * 20.0f;
    const Color color = (Color){120, 220, 170, 255};

    DrawCircleLines((int)x, (int)y, radius, Fade(color, animate ? 0.24f : 0.08f));
    if (animate) {
      DrawCircleLines((int)(x + sinf(time * 0.4f + phase) * 14.0f), (int)y, radius * 0.62f,
                      Fade((Color){210, 190, 255, 255}, 0.12f));
    }
  }

  for (int index = 0; index < 36; index++) {
    const float x_ratio =
        WrappedUnit(0.07f + (float)index * 0.173f + sinf(time * 0.8f + index) * 0.05f);
    const float y_ratio =
        WrappedUnit(0.13f + (float)index * 0.097f - time * (0.06f + (float)(index % 5) * 0.012f));
    const float size = 2.5f + (float)(index % 4);
    const float alpha = (animate ? 0.36f : 0.24f) + (float)(index % 5) * 0.08f;
    const Vector2 position = {x_ratio * (float)screen_width, y_ratio * (float)screen_height};

    DrawCircleV(position, size * 1.7f, Fade((Color){220, 200, 255, 255}, alpha * 0.35f));
    DrawCircleV(position, size, Fade((Color){200, 180, 255, 255}, alpha));
  }
}
