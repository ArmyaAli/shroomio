#ifndef SHROOM_CLIENT_PREDICTION_H
#define SHROOM_CLIENT_PREDICTION_H

#include <stdbool.h>
#include <stdint.h>

#include "shared/vec2.h"

#define SHROOM_PREDICTION_SNAP_DISTANCE 96.0f
#define SHROOM_PREDICTION_CORRECTION_RATE 12.0f

typedef struct ShroomPendingInput {
  uint32_t sequence;
  ShroomVec2 direction;
} ShroomPendingInput;

ShroomVec2 ShroomPredictionApplyInput(ShroomVec2 position, ShroomVec2 direction, float speed,
                                      float delta_time, float radius, float world_width,
                                      float world_height);
void ShroomPredictionDiscardAcknowledged(ShroomPendingInput* inputs, uint32_t* count,
                                         uint32_t acknowledged_sequence);
ShroomVec2 ShroomPredictionReplay(ShroomVec2 authoritative_position,
                                  const ShroomPendingInput* inputs, uint32_t count, float speed,
                                  float tick_delta_time, float radius, float world_width,
                                  float world_height);
bool ShroomPredictionReconcileRender(ShroomVec2* render_position, ShroomVec2 predicted_position);
void ShroomPredictionSmoothRender(ShroomVec2* render_position, ShroomVec2 predicted_position,
                                  float delta_time);

#endif
