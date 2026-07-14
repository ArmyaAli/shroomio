#ifndef SHROOM_CLIENT_INPUT_SCHEDULER_H
#define SHROOM_CLIENT_INPUT_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "shared/vec2.h"

#define SHROOM_CLIENT_INPUT_RATE_HZ 30u
#define SHROOM_CLIENT_ACTION_QUEUE_CAPACITY 16u

typedef struct ShroomClientScheduledActions {
  bool split_requested;
  bool eject_requested;
  ShroomVec2 direction;
  uint32_t focused_entity_id;
  uint64_t intent_id;
} ShroomClientScheduledActions;

typedef struct ShroomClientInputScheduler {
  double accumulator_seconds;
  ShroomClientScheduledActions action_queue[SHROOM_CLIENT_ACTION_QUEUE_CAPACITY];
  uint32_t action_queue_head;
  uint32_t action_queue_count;
  uint64_t next_action_intent_id;
  uint64_t sent_count;
  uint64_t suppressed_catchup_count;
  uint64_t action_overflow_count;
} ShroomClientInputScheduler;

void ShroomClientInputSchedulerReset(ShroomClientInputScheduler* scheduler);
void ShroomClientInputSchedulerPause(ShroomClientInputScheduler* scheduler);
bool ShroomClientInputSchedulerQueueActions(ShroomClientInputScheduler* scheduler,
                                            bool split_requested, bool eject_requested,
                                            ShroomVec2 direction, uint32_t focused_entity_id);
bool ShroomClientInputSchedulerPrepare(ShroomClientInputScheduler* scheduler, float delta_time,
                                       ShroomClientScheduledActions* actions);
void ShroomClientInputSchedulerCommit(ShroomClientInputScheduler* scheduler,
                                      const ShroomClientScheduledActions* actions);

#endif
