#ifndef SHROOM_CLIENT_LAYOUT_H
#define SHROOM_CLIENT_LAYOUT_H

#include <stdbool.h>

bool ShroomLayoutBeginCenteredPanel(const char* title, float width, float height, float alpha,
                                    int flags);
bool ShroomLayoutButtonFullWidth(const char* label, float height);
void ShroomLayoutHeading(const char* text);

#endif
