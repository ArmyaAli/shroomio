#include "layout.h"

#include "imgui_wrapper.h"
#include "layout_metrics.h"
#include "raylib.h"

static float g_layout_scale = 1.0f;

void ShroomLayoutSetScale(float scale) { g_layout_scale = ShroomLayoutClampScale(scale); }

float ShroomLayoutGetScale(void) { return g_layout_scale; }

float ShroomLayoutMetric(float base_value) {
  return ShroomLayoutScaleMetric(base_value, g_layout_scale);
}

bool ShroomLayoutBeginCenteredPanel(const char* title, float width, float height, float alpha,
                                    int flags) {
  const float fitted_width =
      ShroomLayoutFitMetric(width, g_layout_scale, (float)GetScreenWidth(), 16.0f);
  const float fitted_height =
      ShroomLayoutFitMetric(height, g_layout_scale, (float)GetScreenHeight(), 16.0f);

  ShroomImGui_SetNextWindowPos(((float)GetScreenWidth() - fitted_width) * 0.5f,
                               ((float)GetScreenHeight() - fitted_height) * 0.5f,
                               SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(fitted_width, fitted_height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(alpha);
  return ShroomImGui_Begin(title, NULL, flags);
}

bool ShroomLayoutButtonFullWidth(const char* label, float height) {
  return ShroomImGui_Button(label, -1.0f, ShroomLayoutMetric(height));
}

void ShroomLayoutHeading(const char* text) {
  ShroomImGui_Text(text);
  ShroomImGui_Separator();
  ShroomImGui_Spacing();
}
