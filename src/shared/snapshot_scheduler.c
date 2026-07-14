#include "snapshot_scheduler.h"

#include "config.h"
#include "protocol.h"

bool ShroomSnapshotSchedulerInit(ShroomSnapshotScheduler* scheduler, uint32_t tick_rate,
                                 uint32_t snapshot_rate) {
  if ((scheduler == NULL) || (tick_rate == 0u) || (snapshot_rate > tick_rate) ||
      (snapshot_rate < SHROOM_SNAPSHOT_RATE_MIN) || (snapshot_rate > SHROOM_SNAPSHOT_RATE_MAX)) {
    return false;
  }

  *scheduler = (ShroomSnapshotScheduler){
      .tick_rate = tick_rate,
      .snapshot_rate = snapshot_rate,
  };
  return true;
}

bool ShroomSnapshotSchedulerStep(ShroomSnapshotScheduler* scheduler) {
  if ((scheduler == NULL) || (scheduler->tick_rate == 0u) || (scheduler->snapshot_rate == 0u)) {
    return false;
  }

  scheduler->tick_count += 1u;
  scheduler->phase += scheduler->snapshot_rate;
  if (scheduler->phase < scheduler->tick_rate) {
    return false;
  }

  scheduler->phase -= scheduler->tick_rate;
  scheduler->emission_count += 1u;
  return true;
}
