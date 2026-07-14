#include "voice_mixer.h"

#include <math.h>

void ShroomVoiceMixAdd(float* mix, const float* source, size_t sample_count, float volume) {
  if ((mix == NULL) || (source == NULL) || !isfinite(volume) || (volume <= 0.0f)) {
    return;
  }
  for (size_t index = 0u; index < sample_count; ++index) {
    mix[index] += source[index] * volume;
  }
}

void ShroomVoiceMixClamp(float* mix, size_t sample_count) {
  if (mix == NULL) {
    return;
  }
  for (size_t index = 0u; index < sample_count; ++index) {
    if (!isfinite(mix[index])) {
      mix[index] = 0.0f;
    } else if (mix[index] > 1.0f) {
      mix[index] = 1.0f;
    } else if (mix[index] < -1.0f) {
      mix[index] = -1.0f;
    }
  }
}
