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

ShroomLayoutRect ShroomLayoutCenteredRect(float width, float height) {
  const float fitted_width =
      ShroomLayoutFitMetric(width, g_layout_scale, (float)GetScreenWidth(), 16.0f);
  const float fitted_height =
      ShroomLayoutFitMetric(height, g_layout_scale, (float)GetScreenHeight(), 16.0f);

  return (ShroomLayoutRect){
      .x = ((float)GetScreenWidth() - fitted_width) * 0.5f,
      .y = ((float)GetScreenHeight() - fitted_height) * 0.5f,
      .width = fitted_width,
      .height = fitted_height,
  };
}

ShroomLayoutRect ShroomLayoutBottomOverlayRect(float width, float height, float edge_margin,
                                               float minimum_top,
                                               ShroomLayoutHorizontalAnchor horizontal_anchor) {
  const ShroomLayoutOverlayRect rect = ShroomLayoutBottomOverlayMetrics(
      (float)GetScreenWidth(), (float)GetScreenHeight(), width, height, g_layout_scale, edge_margin,
      minimum_top, horizontal_anchor);

  return (ShroomLayoutRect){
      .x = rect.x,
      .y = rect.y,
      .width = rect.width,
      .height = rect.height,
  };
}

void ShroomLayoutSetNextWindowBottomRight(float width, float height, float edge_margin) {
  const ShroomLayoutRect rect =
      ShroomLayoutBottomOverlayRect(width, height, edge_margin, 0.0f, SHROOM_LAYOUT_ANCHOR_RIGHT);

  ShroomImGui_SetNextWindowPos(rect.x, rect.y, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(rect.width, rect.height, SHROOM_IMGUI_COND_ALWAYS);
}

bool ShroomLayoutBeginCenteredPanel(const char* title, float width, float height, float alpha,
                                    int flags) {
  const ShroomLayoutRect rect = ShroomLayoutCenteredRect(width, height);

  ShroomImGui_SetNextWindowPos(rect.x, rect.y, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowSize(rect.width, rect.height, SHROOM_IMGUI_COND_ALWAYS);
  ShroomImGui_SetNextWindowBgAlpha(alpha);
  return ShroomImGui_Begin(title, NULL, flags);
}

bool ShroomLayoutButtonFullWidth(const char* label, float height) {
  return ShroomImGui_Button(label, -1.0f, ShroomLayoutMetric(height));
}

void ShroomLayoutSetNextLabeledItemWidth(const char* label) {
  const float width = ShroomLayoutLabeledItemWidth(ShroomImGui_GetContentRegionAvailWidth(),
                                                   ShroomImGui_CalcTextWidth(label),
                                                   ShroomImGui_GetItemInnerSpacingX());
  ShroomImGui_SetNextItemWidth(width);
}

void ShroomLayoutHeading(const char* text) {
  ShroomImGui_Text(text);
  ShroomImGui_Separator();
  ShroomImGui_Spacing();
}
