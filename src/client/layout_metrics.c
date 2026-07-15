#include "layout_metrics.h"

#include <math.h>

float ShroomLayoutClampScale(float scale) {
  if (!isfinite(scale)) {
    return 1.0f;
  }
  if (scale < 0.8f) {
    return 0.8f;
  }
  if (scale > 1.6f) {
    return 1.6f;
  }
  return scale;
}

float ShroomLayoutScaleMetric(float base_value, float scale) {
  return fmaxf(0.0f, base_value) * ShroomLayoutClampScale(scale);
}

float ShroomLayoutFitMetric(float base_value, float scale, float viewport_size, float edge_margin) {
  const float fitted_space =
      fmaxf(1.0f, viewport_size - (2.0f * ShroomLayoutScaleMetric(edge_margin, scale)));
  return fminf(ShroomLayoutScaleMetric(base_value, scale), fitted_space);
}

float ShroomLayoutLabeledItemWidth(float available_width, float label_width, float item_spacing) {
  const float available = isfinite(available_width) ? fmaxf(1.0f, available_width) : 1.0f;
  const float label = isfinite(label_width) ? fmaxf(0.0f, label_width) : 0.0f;
  const float spacing = isfinite(item_spacing) ? fmaxf(0.0f, item_spacing) : 0.0f;

  return fmaxf(1.0f, available - label - spacing);
}

ShroomLayoutResponsiveRow ShroomLayoutResponsiveRowMetrics(float available_width,
                                                           float minimum_item_width,
                                                           int maximum_columns, float spacing) {
  const float available = isfinite(available_width) ? fmaxf(1.0f, available_width) : 1.0f;
  const float minimum = isfinite(minimum_item_width) ? fmaxf(1.0f, minimum_item_width) : 1.0f;
  const float gap = isfinite(spacing) ? fmaxf(0.0f, spacing) : 0.0f;
  int columns = maximum_columns > 0 ? maximum_columns : 1;

  while ((columns > 1) &&
         (((available - ((float)(columns - 1) * gap)) / (float)columns) < minimum)) {
    --columns;
  }

  return (ShroomLayoutResponsiveRow){
      .columns = columns,
      .item_width = fmaxf(1.0f, (available - ((float)(columns - 1) * gap)) / (float)columns),
  };
}

float ShroomLayoutReservedContentHeight(float available_height, float footer_height,
                                        float spacing) {
  const float available = isfinite(available_height) ? fmaxf(1.0f, available_height) : 1.0f;
  const float footer = isfinite(footer_height) ? fmaxf(0.0f, footer_height) : 0.0f;
  const float gap = isfinite(spacing) ? fmaxf(0.0f, spacing) : 0.0f;

  return fmaxf(1.0f, available - footer - gap);
}

float ShroomLayoutWrappedCardHeight(float text_height, int item_count, float scale) {
  const float content = isfinite(text_height) ? fmaxf(0.0f, text_height) : 0.0f;
  const int gaps = item_count > 1 ? item_count - 1 : 0;

  return content + ShroomLayoutScaleMetric(60.0f + ((float)gaps * 8.0f), scale);
}

ShroomLayoutOverlayRect ShroomLayoutBottomOverlayMetrics(
    float viewport_width, float viewport_height, float base_width, float base_height, float scale,
    float edge_margin, float minimum_top, ShroomLayoutHorizontalAnchor horizontal_anchor) {
  const float width = isfinite(viewport_width) ? fmaxf(1.0f, viewport_width) : 1.0f;
  const float height = isfinite(viewport_height) ? fmaxf(1.0f, viewport_height) : 1.0f;
  const float margin = fminf(ShroomLayoutScaleMetric(edge_margin, scale),
                             fmaxf(0.0f, fminf(width, height) * 0.5f - 0.5f));
  const float top = fminf(fmaxf(margin, ShroomLayoutScaleMetric(minimum_top, scale)),
                          fmaxf(margin, height - margin - 1.0f));
  const float available_width = fmaxf(1.0f, width - (2.0f * margin));
  const float available_height = fmaxf(1.0f, height - top - margin);
  const float fitted_width = fminf(ShroomLayoutScaleMetric(base_width, scale), available_width);
  const float fitted_height = fminf(ShroomLayoutScaleMetric(base_height, scale), available_height);
  float x = margin;

  if (horizontal_anchor == SHROOM_LAYOUT_ANCHOR_CENTER) {
    x = (width - fitted_width) * 0.5f;
  } else if (horizontal_anchor == SHROOM_LAYOUT_ANCHOR_RIGHT) {
    x = width - margin - fitted_width;
  }

  return (ShroomLayoutOverlayRect){
      .x = fmaxf(margin, x),
      .y = fmaxf(top, height - margin - fitted_height),
      .width = fitted_width,
      .height = fitted_height,
  };
}
