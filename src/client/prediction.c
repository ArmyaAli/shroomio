#include "prediction.h"

#include <math.h>
#include <stddef.h>

static float Clamp(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

ShroomVec2 ShroomPredictionApplyInput(ShroomVec2 position, ShroomVec2 direction, float speed,
                                      float delta_time, float radius, float world_width,
                                      float world_height) {
  position.x += direction.x * speed * delta_time;
  position.y += direction.y * speed * delta_time;
  position.x = Clamp(position.x, radius, world_width - radius);
  position.y = Clamp(position.y, radius, world_height - radius);
  return position;
}

void ShroomPredictionDiscardAcknowledged(ShroomPendingInput* inputs, uint32_t* count,
                                         uint32_t acknowledged_sequence) {
  uint32_t keep_count = 0u;

  if ((inputs == NULL) || (count == NULL)) {
    return;
  }
  for (uint32_t index = 0u; index < *count; ++index) {
    if (inputs[index].sequence > acknowledged_sequence) {
      inputs[keep_count++] = inputs[index];
    }
  }
  *count = keep_count;
}

ShroomVec2 ShroomPredictionReplay(ShroomVec2 authoritative_position,
                                  const ShroomPendingInput* inputs, uint32_t count, float speed,
                                  float tick_delta_time, float radius, float world_width,
                                  float world_height) {
  for (uint32_t index = 0u; (inputs != NULL) && (index < count); ++index) {
    authoritative_position =
        ShroomPredictionApplyInput(authoritative_position, inputs[index].direction, speed,
                                   tick_delta_time, radius, world_width, world_height);
  }
  return authoritative_position;
}

bool ShroomPredictionReconcileRender(ShroomVec2* render_position, ShroomVec2 predicted_position) {
  const float threshold_sqr = SHROOM_PREDICTION_SNAP_DISTANCE * SHROOM_PREDICTION_SNAP_DISTANCE;
  float error_x;
  float error_y;

  if (render_position == NULL) {
    return false;
  }
  error_x = predicted_position.x - render_position->x;
  error_y = predicted_position.y - render_position->y;
  if ((error_x * error_x) + (error_y * error_y) > threshold_sqr) {
    *render_position = predicted_position;
    return true;
  }
  return false;
}

void ShroomPredictionSmoothRender(ShroomVec2* render_position, ShroomVec2 predicted_position,
                                  float delta_time) {
  const float blend = fminf(1.0f, delta_time * SHROOM_PREDICTION_CORRECTION_RATE);

  if (render_position == NULL) {
    return;
  }
  render_position->x += (predicted_position.x - render_position->x) * blend;
  render_position->y += (predicted_position.y - render_position->y) * blend;
}
