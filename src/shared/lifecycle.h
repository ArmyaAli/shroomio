#ifndef SHROOM_LIFECYCLE_H
#define SHROOM_LIFECYCLE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum ShroomLifecycleState {
  SHROOM_LIFECYCLE_UNINITIALIZED = 0,
  SHROOM_LIFECYCLE_INITIALIZING = 1,
  SHROOM_LIFECYCLE_INITIALIZED = 2,
  SHROOM_LIFECYCLE_RUNNING = 3,
  SHROOM_LIFECYCLE_PAUSED = 4,
  SHROOM_LIFECYCLE_SHUTTING_DOWN = 5,
  SHROOM_LIFECYCLE_SHUTDOWN = 6,
  SHROOM_LIFECYCLE_ERROR = 7,
} ShroomLifecycleState;

typedef enum ShroomLifecycleEvent {
  SHROOM_LIFECYCLE_EVENT_INIT = 0,
  SHROOM_LIFECYCLE_EVENT_START = 1,
  SHROOM_LIFECYCLE_EVENT_PAUSE = 2,
  SHROOM_LIFECYCLE_EVENT_RESUME = 3,
  SHROOM_LIFECYCLE_EVENT_STOP = 4,
  SHROOM_LIFECYCLE_EVENT_SHUTDOWN = 5,
  SHROOM_LIFECYCLE_EVENT_ERROR = 6,
} ShroomLifecycleEvent;

typedef struct ShroomLifecycle {
  ShroomLifecycleState state;
  bool shutdown_requested;
  uint32_t error_code;
  const char* error_message;
} ShroomLifecycle;

void ShroomLifecycleInit(ShroomLifecycle* lifecycle);

bool ShroomLifecycleCanTransition(const ShroomLifecycle* lifecycle, ShroomLifecycleEvent event);

bool ShroomLifecycleTransition(ShroomLifecycle* lifecycle, ShroomLifecycleEvent event);

void ShroomLifecycleRequestShutdown(ShroomLifecycle* lifecycle);

bool ShroomLifecycleIsRunning(const ShroomLifecycle* lifecycle);

bool ShroomLifecycleIsShutdownRequested(const ShroomLifecycle* lifecycle);

void ShroomLifecycleSetError(ShroomLifecycle* lifecycle, uint32_t error_code,
                             const char* error_message);

const char* ShroomLifecycleStateToString(ShroomLifecycleState state);

const char* ShroomLifecycleEventToString(ShroomLifecycleEvent event);

#endif
