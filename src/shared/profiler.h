#ifndef SHROOM_SHARED_PROFILER_H
#define SHROOM_SHARED_PROFILER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SHROOM_PROFILE_WINDOW_SIZE 120u

typedef struct ShroomProfileWindow {
  double samples_ms[SHROOM_PROFILE_WINDOW_SIZE];
  double sum_ms;
  double peak_ms;
  uint32_t index;
  uint32_t count;
} ShroomProfileWindow;

static inline bool ShroomProfileEnabled(void) {
  const char* value = getenv("SHROOM_PROFILE");

  return (value != NULL) && (value[0] != '\0') && (strcmp(value, "0") != 0) &&
         (strcmp(value, "false") != 0) && (strcmp(value, "FALSE") != 0);
}

static inline double ShroomProfileNanosToMs(uint64_t elapsed_nanos) {
  return (double)elapsed_nanos / 1000000.0;
}

static inline void ShroomProfileRecord(ShroomProfileWindow* window, double elapsed_ms) {
  if (window == NULL) {
    return;
  }

  if (window->count < SHROOM_PROFILE_WINDOW_SIZE) {
    window->count += 1u;
  } else {
    window->sum_ms -= window->samples_ms[window->index];
  }

  window->samples_ms[window->index] = elapsed_ms;
  window->sum_ms += elapsed_ms;
  window->index = (window->index + 1u) % SHROOM_PROFILE_WINDOW_SIZE;
  if (elapsed_ms > window->peak_ms) {
    window->peak_ms = elapsed_ms;
  }
}

static inline double ShroomProfileAverageMs(const ShroomProfileWindow* window) {
  return (window != NULL && window->count > 0u) ? (window->sum_ms / (double)window->count) : 0.0;
}

static inline void ShroomProfileResetPeak(ShroomProfileWindow* window) {
  if (window != NULL) {
    window->peak_ms = 0.0;
  }
}

#endif
