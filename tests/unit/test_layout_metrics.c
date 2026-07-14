#include "unity.h"

#include "client/layout_metrics.h"

#include <math.h>

void setUp(void) {}

void tearDown(void) {}

void test_scale_clamps_to_documented_endpoints(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, ShroomLayoutClampScale(0.2f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ShroomLayoutClampScale(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.6f, ShroomLayoutClampScale(2.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ShroomLayoutClampScale(NAN));
}

void test_metrics_scale_at_supported_values(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 80.0f, ShroomLayoutScaleMetric(100.0f, 0.8f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, ShroomLayoutScaleMetric(100.0f, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 160.0f, ShroomLayoutScaleMetric(100.0f, 1.6f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ShroomLayoutScaleMetric(-10.0f, 1.0f));
}

void test_fitted_metrics_stay_inside_viewport_margins(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 544.0f, ShroomLayoutFitMetric(340.0f, 1.6f, 1280.0f, 16.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 668.8f, ShroomLayoutFitMetric(514.0f, 1.6f, 720.0f, 16.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, ShroomLayoutFitMetric(100.0f, 1.6f, 20.0f, 16.0f));
}

void test_labeled_item_width_reserves_label_and_spacing(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 241.0f,
                           ShroomLayoutLabeledItemWidth(340.0f, 89.0f, 10.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 385.6f,
                           ShroomLayoutLabeledItemWidth(544.0f, 142.4f, 16.0f));
}

void test_labeled_item_width_handles_constrained_or_invalid_metrics(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           ShroomLayoutLabeledItemWidth(80.0f, 100.0f, 10.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f,
                           ShroomLayoutLabeledItemWidth(200.0f, NAN, NAN));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           ShroomLayoutLabeledItemWidth(NAN, 20.0f, 4.0f));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_scale_clamps_to_documented_endpoints);
  RUN_TEST(test_metrics_scale_at_supported_values);
  RUN_TEST(test_fitted_metrics_stay_inside_viewport_margins);
  RUN_TEST(test_labeled_item_width_reserves_label_and_spacing);
  RUN_TEST(test_labeled_item_width_handles_constrained_or_invalid_metrics);
  return UNITY_END();
}
