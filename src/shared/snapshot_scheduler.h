#ifndef SHROOM_SNAPSHOT_SCHEDULER_H
#define SHROOM_SNAPSHOT_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct ShroomSnapshotScheduler {
  uint32_t tick_rate;
  uint32_t snapshot_rate;
  uint32_t phase;
  uint64_t tick_count;
  uint64_t emission_count;
} ShroomSnapshotScheduler;

bool ShroomSnapshotSchedulerInit(ShroomSnapshotScheduler* scheduler, uint32_t tick_rate,
                                 uint32_t snapshot_rate);
bool ShroomSnapshotSchedulerStep(ShroomSnapshotScheduler* scheduler);

#endif
