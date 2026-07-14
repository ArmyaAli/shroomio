#include "input_scheduler.h"

#include <math.h>
#include <stddef.h>

static const double kInputIntervalSeconds = 1.0 / (double)SHROOM_CLIENT_INPUT_RATE_HZ;

void ShroomClientInputSchedulerReset(ShroomClientInputScheduler* scheduler) {
  if (scheduler != NULL) {
    *scheduler = (ShroomClientInputScheduler){0};
  }
}

void ShroomClientInputSchedulerPause(ShroomClientInputScheduler* scheduler) {
  if (scheduler == NULL) {
    return;
  }
  scheduler->accumulator_seconds = 0.0;
  scheduler->action_queue_head = 0u;
  scheduler->action_queue_count = 0u;
}

bool ShroomClientInputSchedulerQueueActions(ShroomClientInputScheduler* scheduler,
                                            bool split_requested, bool eject_requested,
                                            ShroomVec2 direction, uint32_t focused_entity_id) {
  uint32_t tail;

  if ((scheduler == NULL) || (!split_requested && !eject_requested)) {
    return false;
  }
  if (scheduler->action_queue_count >= SHROOM_CLIENT_ACTION_QUEUE_CAPACITY) {
    scheduler->action_overflow_count += 1u;
    return false;
  }
  scheduler->next_action_intent_id += 1u;
  if (scheduler->next_action_intent_id == 0u) {
    scheduler->next_action_intent_id = 1u;
  }
  tail = (scheduler->action_queue_head + scheduler->action_queue_count) %
         SHROOM_CLIENT_ACTION_QUEUE_CAPACITY;
  scheduler->action_queue[tail] = (ShroomClientScheduledActions){
      .split_requested = split_requested,
      .eject_requested = eject_requested,
      .direction = direction,
      .focused_entity_id = focused_entity_id,
      .intent_id = scheduler->next_action_intent_id,
  };
  scheduler->action_queue_count += 1u;
  return true;
}

bool ShroomClientInputSchedulerPrepare(ShroomClientInputScheduler* scheduler, float delta_time,
                                       ShroomClientScheduledActions* actions) {
  uint64_t elapsed_slots;

  if (actions != NULL) {
    *actions = (ShroomClientScheduledActions){0};
  }
  if ((scheduler == NULL) || (actions == NULL) || !isfinite(delta_time) || (delta_time <= 0.0f)) {
    return false;
  }

  scheduler->accumulator_seconds += (double)delta_time;
  if (scheduler->accumulator_seconds < kInputIntervalSeconds) {
    return false;
  }

  elapsed_slots = (uint64_t)(scheduler->accumulator_seconds / kInputIntervalSeconds);
  if (elapsed_slots > 1u) {
    scheduler->suppressed_catchup_count += elapsed_slots - 1u;
  }
  scheduler->accumulator_seconds = fmod(scheduler->accumulator_seconds, kInputIntervalSeconds);
  if (scheduler->action_queue_count > 0u) {
    *actions = scheduler->action_queue[scheduler->action_queue_head];
  }
  return true;
}

void ShroomClientInputSchedulerCommit(ShroomClientInputScheduler* scheduler,
                                      const ShroomClientScheduledActions* actions) {
  if ((scheduler == NULL) || (actions == NULL)) {
    return;
  }
  if ((actions->intent_id != 0u) && (scheduler->action_queue_count > 0u) &&
      (scheduler->action_queue[scheduler->action_queue_head].intent_id == actions->intent_id)) {
    scheduler->action_queue_head =
        (scheduler->action_queue_head + 1u) % SHROOM_CLIENT_ACTION_QUEUE_CAPACITY;
    scheduler->action_queue_count -= 1u;
  }
  scheduler->sent_count += 1u;
}
