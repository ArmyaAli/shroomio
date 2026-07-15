#ifndef SHROOM_CLIENT_LAYOUT_METRICS_H
#define SHROOM_CLIENT_LAYOUT_METRICS_H

float ShroomLayoutClampScale(float scale);
float ShroomLayoutScaleMetric(float base_value, float scale);
float ShroomLayoutFitMetric(float base_value, float scale, float viewport_size, float edge_margin);
float ShroomLayoutLabeledItemWidth(float available_width, float label_width, float item_spacing);

typedef struct ShroomLayoutResponsiveRow {
  int columns;
  float item_width;
} ShroomLayoutResponsiveRow;

ShroomLayoutResponsiveRow ShroomLayoutResponsiveRowMetrics(float available_width,
                                                           float minimum_item_width,
                                                           int maximum_columns, float spacing);
float ShroomLayoutReservedContentHeight(float available_height, float footer_height, float spacing);
float ShroomLayoutWrappedCardHeight(float text_height, int item_count, float scale);

typedef enum ShroomLayoutHorizontalAnchor {
  SHROOM_LAYOUT_ANCHOR_LEFT = 0,
  SHROOM_LAYOUT_ANCHOR_CENTER,
  SHROOM_LAYOUT_ANCHOR_RIGHT,
} ShroomLayoutHorizontalAnchor;

typedef struct ShroomLayoutOverlayRect {
  float x;
  float y;
  float width;
  float height;
} ShroomLayoutOverlayRect;

ShroomLayoutOverlayRect
ShroomLayoutBottomOverlayMetrics(float viewport_width, float viewport_height, float base_width,
                                 float base_height, float scale, float edge_margin,
                                 float minimum_top, ShroomLayoutHorizontalAnchor horizontal_anchor);

#endif
