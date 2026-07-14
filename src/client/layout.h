#ifndef SHROOM_CLIENT_LAYOUT_H
#define SHROOM_CLIENT_LAYOUT_H

#include <stdbool.h>

typedef struct ShroomLayoutRect {
  float x;
  float y;
  float width;
  float height;
} ShroomLayoutRect;

void ShroomLayoutSetScale(float scale);
float ShroomLayoutGetScale(void);
float ShroomLayoutMetric(float base_value);
ShroomLayoutRect ShroomLayoutCenteredRect(float width, float height);
void ShroomLayoutSetNextWindowBottomRight(float width, float height, float edge_margin);
bool ShroomLayoutBeginCenteredPanel(const char* title, float width, float height, float alpha,
                                    int flags);
bool ShroomLayoutButtonFullWidth(const char* label, float height);
void ShroomLayoutSetNextLabeledItemWidth(const char* label);
void ShroomLayoutHeading(const char* text);

#endif
