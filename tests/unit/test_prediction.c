#include "unity.h"

#include "client/prediction.h"

void setUp(void) {}

void tearDown(void) {}

static void test_input_applies_immediately_and_clamps_to_world(void) {
  ShroomVec2 position = ShroomPredictionApplyInput(
      (ShroomVec2){50.0f, 50.0f}, (ShroomVec2){1.0f, 0.0f}, 120.0f, 0.5f, 10.0f, 100.0f, 100.0f);

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, position.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, position.y);
}

static void test_acknowledged_inputs_drop_while_lost_inputs_replay(void) {
  ShroomPendingInput inputs[3] = {
      {.sequence = 10u, .direction = {1.0f, 0.0f}},
      {.sequence = 11u, .direction = {0.0f, 1.0f}},
      {.sequence = 12u, .direction = {1.0f, 0.0f}},
  };
  uint32_t count = 3u;

  ShroomPredictionDiscardAcknowledged(inputs, &count, 10u);
  ShroomVec2 replayed = ShroomPredictionReplay((ShroomVec2){100.0f, 100.0f}, inputs, count, 30.0f,
                                               1.0f / 30.0f, 5.0f, 500.0f, 500.0f);

  TEST_ASSERT_EQUAL_UINT32(2u, count);
  TEST_ASSERT_EQUAL_UINT32(11u, inputs[0].sequence);
  TEST_ASSERT_EQUAL_UINT32(12u, inputs[1].sequence);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 101.0f, replayed.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 101.0f, replayed.y);
}

static void test_small_error_smooths_without_immediate_snap(void) {
  ShroomVec2 rendered = {100.0f, 100.0f};

  TEST_ASSERT_FALSE(ShroomPredictionReconcileRender(&rendered, (ShroomVec2){120.0f, 100.0f}));
  ShroomPredictionSmoothRender(&rendered, (ShroomVec2){120.0f, 100.0f}, 1.0f / 60.0f);

  TEST_ASSERT_GREATER_THAN_FLOAT(100.0f, rendered.x);
  TEST_ASSERT_LESS_THAN_FLOAT(120.0f, rendered.x);
}

static void test_large_error_hard_snaps(void) {
  ShroomVec2 rendered = {100.0f, 100.0f};

  TEST_ASSERT_TRUE(ShroomPredictionReconcileRender(&rendered, (ShroomVec2){300.0f, 250.0f}));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 300.0f, rendered.x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 250.0f, rendered.y);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_input_applies_immediately_and_clamps_to_world);
  RUN_TEST(test_acknowledged_inputs_drop_while_lost_inputs_replay);
  RUN_TEST(test_small_error_smooths_without_immediate_snap);
  RUN_TEST(test_large_error_hard_snaps);
  return UNITY_END();
}
