#ifndef SHROOM_CLIENT_LAYOUT_METRICS_H
#define SHROOM_CLIENT_LAYOUT_METRICS_H

float ShroomLayoutClampScale(float scale);
float ShroomLayoutScaleMetric(float base_value, float scale);
float ShroomLayoutFitMetric(float base_value, float scale, float viewport_size, float edge_margin);

#endif
