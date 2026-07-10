#include "results_summary.h"

#include <stdio.h>

uint32_t ShroomResultsElapsedSeconds(float start_time, float end_time) {
  if (end_time <= start_time) {
    return 0u;
  }

  return (uint32_t)(end_time - start_time);
}

void ShroomResultsFormatDuration(uint32_t duration_seconds, char* buffer, size_t buffer_size) {
  if ((buffer == NULL) || (buffer_size == 0u)) {
    return;
  }

  snprintf(buffer, buffer_size, "%u:%02u", duration_seconds / 60u, duration_seconds % 60u);
}
