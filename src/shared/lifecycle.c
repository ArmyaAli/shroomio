#include "lifecycle.h"
#include <stddef.h>

void ShroomLifecycleInit(ShroomLifecycle* lifecycle) {
  if (lifecycle == NULL) {
    return;
  }
  lifecycle->state = SHROOM_LIFECYCLE_UNINITIALIZED;
  lifecycle->shutdown_requested = false;
  lifecycle->error_code = 0;
  lifecycle->error_message = NULL;
}

bool ShroomLifecycleCanTransition(const ShroomLifecycle* lifecycle, ShroomLifecycleEvent event) {
  if (lifecycle == NULL) {
    return false;
  }

  switch (lifecycle->state) {
  case SHROOM_LIFECYCLE_UNINITIALIZED:
    return event == SHROOM_LIFECYCLE_EVENT_INIT;

  case SHROOM_LIFECYCLE_INITIALIZING:
    return event == SHROOM_LIFECYCLE_EVENT_START || event == SHROOM_LIFECYCLE_EVENT_ERROR;

  case SHROOM_LIFECYCLE_INITIALIZED:
    return event == SHROOM_LIFECYCLE_EVENT_START || event == SHROOM_LIFECYCLE_EVENT_SHUTDOWN ||
           event == SHROOM_LIFECYCLE_EVENT_ERROR;

  case SHROOM_LIFECYCLE_RUNNING:
    return event == SHROOM_LIFECYCLE_EVENT_PAUSE || event == SHROOM_LIFECYCLE_EVENT_STOP ||
           event == SHROOM_LIFECYCLE_EVENT_SHUTDOWN || event == SHROOM_LIFECYCLE_EVENT_ERROR;

  case SHROOM_LIFECYCLE_PAUSED:
    return event == SHROOM_LIFECYCLE_EVENT_RESUME || event == SHROOM_LIFECYCLE_EVENT_STOP ||
           event == SHROOM_LIFECYCLE_EVENT_SHUTDOWN || event == SHROOM_LIFECYCLE_EVENT_ERROR;

  case SHROOM_LIFECYCLE_SHUTTING_DOWN:
    return event == SHROOM_LIFECYCLE_EVENT_SHUTDOWN || event == SHROOM_LIFECYCLE_EVENT_ERROR;

  case SHROOM_LIFECYCLE_SHUTDOWN:
    return false;

  case SHROOM_LIFECYCLE_ERROR:
    return event == SHROOM_LIFECYCLE_EVENT_SHUTDOWN;

  default:
    return false;
  }
}

bool ShroomLifecycleTransition(ShroomLifecycle* lifecycle, ShroomLifecycleEvent event) {
  if (lifecycle == NULL) {
    return false;
  }

  if (!ShroomLifecycleCanTransition(lifecycle, event)) {
    return false;
  }

  switch (event) {
  case SHROOM_LIFECYCLE_EVENT_INIT:
    lifecycle->state = SHROOM_LIFECYCLE_INITIALIZING;
    break;

  case SHROOM_LIFECYCLE_EVENT_START:
    if (lifecycle->state == SHROOM_LIFECYCLE_INITIALIZING ||
        lifecycle->state == SHROOM_LIFECYCLE_INITIALIZED) {
      lifecycle->state = SHROOM_LIFECYCLE_RUNNING;
    } else if (lifecycle->state == SHROOM_LIFECYCLE_PAUSED) {
      lifecycle->state = SHROOM_LIFECYCLE_RUNNING;
    }
    break;

  case SHROOM_LIFECYCLE_EVENT_PAUSE:
    lifecycle->state = SHROOM_LIFECYCLE_PAUSED;
    break;

  case SHROOM_LIFECYCLE_EVENT_RESUME:
    lifecycle->state = SHROOM_LIFECYCLE_RUNNING;
    break;

  case SHROOM_LIFECYCLE_EVENT_STOP:
    lifecycle->state = SHROOM_LIFECYCLE_SHUTTING_DOWN;
    break;

  case SHROOM_LIFECYCLE_EVENT_SHUTDOWN:
    lifecycle->state = SHROOM_LIFECYCLE_SHUTDOWN;
    break;

  case SHROOM_LIFECYCLE_EVENT_ERROR:
    lifecycle->state = SHROOM_LIFECYCLE_ERROR;
    break;

  default:
    return false;
  }

  return true;
}

void ShroomLifecycleRequestShutdown(ShroomLifecycle* lifecycle) {
  if (lifecycle == NULL) {
    return;
  }
  lifecycle->shutdown_requested = true;
}

bool ShroomLifecycleIsRunning(const ShroomLifecycle* lifecycle) {
  if (lifecycle == NULL) {
    return false;
  }
  return lifecycle->state == SHROOM_LIFECYCLE_RUNNING ||
         lifecycle->state == SHROOM_LIFECYCLE_PAUSED;
}

bool ShroomLifecycleIsShutdownRequested(const ShroomLifecycle* lifecycle) {
  if (lifecycle == NULL) {
    return true;
  }
  return lifecycle->shutdown_requested || lifecycle->state == SHROOM_LIFECYCLE_SHUTTING_DOWN ||
         lifecycle->state == SHROOM_LIFECYCLE_SHUTDOWN;
}

void ShroomLifecycleSetError(ShroomLifecycle* lifecycle, uint32_t error_code,
                             const char* error_message) {
  if (lifecycle == NULL) {
    return;
  }
  lifecycle->error_code = error_code;
  lifecycle->error_message = error_message;
  lifecycle->state = SHROOM_LIFECYCLE_ERROR;
}

const char* ShroomLifecycleStateToString(ShroomLifecycleState state) {
  switch (state) {
  case SHROOM_LIFECYCLE_UNINITIALIZED:
    return "UNINITIALIZED";
  case SHROOM_LIFECYCLE_INITIALIZING:
    return "INITIALIZING";
  case SHROOM_LIFECYCLE_INITIALIZED:
    return "INITIALIZED";
  case SHROOM_LIFECYCLE_RUNNING:
    return "RUNNING";
  case SHROOM_LIFECYCLE_PAUSED:
    return "PAUSED";
  case SHROOM_LIFECYCLE_SHUTTING_DOWN:
    return "SHUTTING_DOWN";
  case SHROOM_LIFECYCLE_SHUTDOWN:
    return "SHUTDOWN";
  case SHROOM_LIFECYCLE_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

const char* ShroomLifecycleEventToString(ShroomLifecycleEvent event) {
  switch (event) {
  case SHROOM_LIFECYCLE_EVENT_INIT:
    return "INIT";
  case SHROOM_LIFECYCLE_EVENT_START:
    return "START";
  case SHROOM_LIFECYCLE_EVENT_PAUSE:
    return "PAUSE";
  case SHROOM_LIFECYCLE_EVENT_RESUME:
    return "RESUME";
  case SHROOM_LIFECYCLE_EVENT_STOP:
    return "STOP";
  case SHROOM_LIFECYCLE_EVENT_SHUTDOWN:
    return "SHUTDOWN";
  case SHROOM_LIFECYCLE_EVENT_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}
