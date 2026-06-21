#ifndef SHROOM_SCREEN_BACKGROUND_H
#define SHROOM_SCREEN_BACKGROUND_H

#include <stdbool.h>

void ShroomScreenResetFungalBackground(void);
void ShroomScreenUpdateFungalBackground(float delta_time, bool animate);
void ShroomScreenDrawFungalBackground(bool animate);

#endif
