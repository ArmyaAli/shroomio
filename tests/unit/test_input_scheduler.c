#include "unity.h"

#include <math.h>

#include "client/input_scheduler.h"

void setUp(void) {}
void tearDown(void) {}

static uint32_t RunCadence(uint32_t frame_rate, float seconds) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;
  const uint32_t frames = (uint32_t)((float)frame_rate * seconds);
  uint32_t sends = 0u;

  for (uint32_t frame = 0u; frame < frames; ++frame) {
    if (ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / (float)frame_rate, &actions)) {
      ShroomClientInputSchedulerCommit(&scheduler, &actions);
      sends += 1u;
    }
  }
  return sends;
}

static void test_cadence_is_30_hz_at_common_render_rates(void) {
  TEST_ASSERT_UINT32_WITHIN(1u, 30u, RunCadence(30u, 1.0f));
  TEST_ASSERT_UINT32_WITHIN(1u, 30u, RunCadence(60u, 1.0f));
  TEST_ASSERT_UINT32_WITHIN(1u, 30u, RunCadence(144u, 1.0f));
}

static void test_long_stall_coalesces_to_one_current_input(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 2.0f, &actions));
  ShroomClientInputSchedulerCommit(&scheduler, &actions);

  TEST_ASSERT_EQUAL_UINT64(1u, scheduler.sent_count);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(58u, scheduler.suppressed_catchup_count);
  TEST_ASSERT_TRUE(scheduler.accumulator_seconds < (1.0 / SHROOM_CLIENT_INPUT_RATE_HZ));
}

static void test_fractional_time_survives_send(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  TEST_ASSERT_FALSE(ShroomClientInputSchedulerPrepare(&scheduler, 0.020f, &actions));
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 0.020f, &actions));
  ShroomClientInputSchedulerCommit(&scheduler, &actions);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0066667f, (float)scheduler.accumulator_seconds);
}

static void test_actions_wait_for_send_and_commit_exactly_once(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;
  const ShroomVec2 direction = {0.25f, -0.75f};

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, true, true, direction, 77u));
  TEST_ASSERT_FALSE(ShroomClientInputSchedulerPrepare(&scheduler, 0.010f, &actions));
  TEST_ASSERT_EQUAL_UINT32(1u, scheduler.action_queue_count);

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 0.024f, &actions));
  TEST_ASSERT_TRUE(actions.split_requested);
  TEST_ASSERT_TRUE(actions.eject_requested);
  TEST_ASSERT_EQUAL_FLOAT(direction.x, actions.direction.x);
  TEST_ASSERT_EQUAL_FLOAT(direction.y, actions.direction.y);
  TEST_ASSERT_EQUAL_UINT32(77u, actions.focused_entity_id);

  ShroomClientInputSchedulerCommit(&scheduler, &actions);
  TEST_ASSERT_EQUAL_UINT32(0u, scheduler.action_queue_count);
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f, &actions));
  TEST_ASSERT_FALSE(actions.split_requested);
  TEST_ASSERT_FALSE(actions.eject_requested);
}

static void test_failed_send_keeps_latched_actions(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, true, false,
                                                          (ShroomVec2){1.0f, 0.0f}, 4u));
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
  TEST_ASSERT_EQUAL_UINT32(1u, scheduler.action_queue_count);
  const uint64_t intent_id = actions.intent_id;
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
  TEST_ASSERT_TRUE(actions.split_requested);
  TEST_ASSERT_EQUAL_UINT64(intent_id, actions.intent_id);
  TEST_ASSERT_EQUAL_UINT32(4u, actions.focused_entity_id);
}

static void test_distinct_action_metadata_is_preserved_in_fifo_order(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, true, false,
                                                          (ShroomVec2){1.0f, 0.25f}, 11u));
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, false, true,
                                                          (ShroomVec2){-0.5f, 0.75f}, 22u));

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
  TEST_ASSERT_TRUE(actions.split_requested);
  TEST_ASSERT_FALSE(actions.eject_requested);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, actions.direction.x);
  TEST_ASSERT_EQUAL_FLOAT(0.25f, actions.direction.y);
  TEST_ASSERT_EQUAL_UINT32(11u, actions.focused_entity_id);
  ShroomClientInputSchedulerCommit(&scheduler, &actions);

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
  TEST_ASSERT_FALSE(actions.split_requested);
  TEST_ASSERT_TRUE(actions.eject_requested);
  TEST_ASSERT_EQUAL_FLOAT(-0.5f, actions.direction.x);
  TEST_ASSERT_EQUAL_FLOAT(0.75f, actions.direction.y);
  TEST_ASSERT_EQUAL_UINT32(22u, actions.focused_entity_id);
  ShroomClientInputSchedulerCommit(&scheduler, &actions);
  TEST_ASSERT_EQUAL_UINT32(0u, scheduler.action_queue_count);
}

static void test_repeated_actions_emit_once_each(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, true, false,
                                                          (ShroomVec2){0.0f, 1.0f}, 31u));
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, true, false,
                                                          (ShroomVec2){0.0f, 1.0f}, 31u));

  for (uint32_t index = 0u; index < 2u; ++index) {
    TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
    TEST_ASSERT_TRUE(actions.split_requested);
    TEST_ASSERT_FALSE(actions.eject_requested);
    ShroomClientInputSchedulerCommit(&scheduler, &actions);
  }
  TEST_ASSERT_EQUAL_UINT32(0u, scheduler.action_queue_count);
  TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
  TEST_ASSERT_FALSE(actions.split_requested);
}

static void test_action_queue_overflow_drops_newest_and_counts_it(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  for (uint32_t index = 0u; index < SHROOM_CLIENT_ACTION_QUEUE_CAPACITY; ++index) {
    TEST_ASSERT_TRUE(ShroomClientInputSchedulerQueueActions(&scheduler, true, false,
                                                            (ShroomVec2){1.0f, 0.0f}, index + 1u));
  }
  TEST_ASSERT_FALSE(ShroomClientInputSchedulerQueueActions(&scheduler, false, true,
                                                           (ShroomVec2){-1.0f, 0.0f}, 999u));
  TEST_ASSERT_EQUAL_UINT32(SHROOM_CLIENT_ACTION_QUEUE_CAPACITY, scheduler.action_queue_count);
  TEST_ASSERT_EQUAL_UINT64(1u, scheduler.action_overflow_count);

  for (uint32_t index = 0u; index < SHROOM_CLIENT_ACTION_QUEUE_CAPACITY; ++index) {
    TEST_ASSERT_TRUE(ShroomClientInputSchedulerPrepare(&scheduler, 1.0f / 30.0f, &actions));
    TEST_ASSERT_TRUE(actions.split_requested);
    TEST_ASSERT_FALSE(actions.eject_requested);
    TEST_ASSERT_EQUAL_UINT32(index + 1u, actions.focused_entity_id);
    ShroomClientInputSchedulerCommit(&scheduler, &actions);
  }
  TEST_ASSERT_EQUAL_UINT32(0u, scheduler.action_queue_count);
}

static void test_invalid_delta_does_not_change_schedule(void) {
  ShroomClientInputScheduler scheduler = {0};
  ShroomClientScheduledActions actions;

  TEST_ASSERT_FALSE(ShroomClientInputSchedulerPrepare(&scheduler, -1.0f, &actions));
  TEST_ASSERT_FALSE(ShroomClientInputSchedulerPrepare(&scheduler, NAN, &actions));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, (float)scheduler.accumulator_seconds);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_cadence_is_30_hz_at_common_render_rates);
  RUN_TEST(test_long_stall_coalesces_to_one_current_input);
  RUN_TEST(test_fractional_time_survives_send);
  RUN_TEST(test_actions_wait_for_send_and_commit_exactly_once);
  RUN_TEST(test_failed_send_keeps_latched_actions);
  RUN_TEST(test_distinct_action_metadata_is_preserved_in_fifo_order);
  RUN_TEST(test_repeated_actions_emit_once_each);
  RUN_TEST(test_action_queue_overflow_drops_newest_and_counts_it);
  RUN_TEST(test_invalid_delta_does_not_change_schedule);
  return UNITY_END();
}
