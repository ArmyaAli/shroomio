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
