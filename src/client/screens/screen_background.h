#ifndef SHROOM_SCREEN_BACKGROUND_H
#define SHROOM_SCREEN_BACKGROUND_H

#include <stdbool.h>

#ifdef TEST_MODE
typedef struct ShroomFungalBackgroundDebugState {
  float global_time;
  float mushroom_x;
  float mushroom_y;
  float mushroom_sway;
} ShroomFungalBackgroundDebugState;
#endif

void ShroomScreenResetFungalBackground(void);
void ShroomScreenUpdateFungalBackground(float delta_time, bool animate);
void ShroomScreenDrawFungalBackground(bool animate);

#ifdef TEST_MODE
ShroomFungalBackgroundDebugState ShroomScreenGetFungalBackgroundDebugState(int mushroom_index,
                                                                           bool animate,
                                                                           int screen_width,
                                                                           int screen_height);
#endif

#endif
