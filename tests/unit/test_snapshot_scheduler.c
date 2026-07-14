#include "unity.h"

#include "shared/config.h"
#include "shared/protocol.h"
#include "shared/snapshot_scheduler.h"

void setUp(void) {}
void tearDown(void) {}

static uint64_t CountEmissions(uint32_t rate, uint32_t ticks) {
  ShroomSnapshotScheduler scheduler;
  uint64_t count = 0u;

  TEST_ASSERT_TRUE(ShroomSnapshotSchedulerInit(&scheduler, (uint32_t)SHROOM_SERVER_TICK_RATE, rate));
  for (uint32_t tick = 0u; tick < ticks; ++tick) {
    if (ShroomSnapshotSchedulerStep(&scheduler)) {
      count += 1u;
    }
  }
  TEST_ASSERT_EQUAL_UINT64(ticks, scheduler.tick_count);
  TEST_ASSERT_EQUAL_UINT64(count, scheduler.emission_count);
  return count;
}

static void test_exact_15_and_20_hz_counts_do_not_drift(void) {
  const uint32_t ten_minutes = (uint32_t)SHROOM_SERVER_TICK_RATE * 600u;

  TEST_ASSERT_EQUAL_UINT64(15u * 600u, CountEmissions(15u, ten_minutes));
  TEST_ASSERT_EQUAL_UINT64(20u * 600u, CountEmissions(20u, ten_minutes));
}

static void test_20_hz_uses_three_tick_spacing(void) {
  ShroomSnapshotScheduler scheduler;

  TEST_ASSERT_TRUE(ShroomSnapshotSchedulerInit(&scheduler, 60u, 20u));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerStep(&scheduler));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerStep(&scheduler));
  TEST_ASSERT_TRUE(ShroomSnapshotSchedulerStep(&scheduler));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerStep(&scheduler));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerStep(&scheduler));
  TEST_ASSERT_TRUE(ShroomSnapshotSchedulerStep(&scheduler));
}

static void test_invalid_rates_are_rejected(void) {
  ShroomSnapshotScheduler scheduler;

  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerInit(NULL, 60u, 15u));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerInit(&scheduler, 0u, 15u));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerInit(&scheduler, 60u, 14u));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerInit(&scheduler, 60u, 21u));
  TEST_ASSERT_FALSE(ShroomSnapshotSchedulerInit(&scheduler, 15u, 20u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_exact_15_and_20_hz_counts_do_not_drift);
  RUN_TEST(test_20_hz_uses_three_tick_spacing);
  RUN_TEST(test_invalid_rates_are_rejected);
  return UNITY_END();
}
