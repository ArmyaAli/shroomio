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

void test_help_rows_scale_and_collapse_at_supported_endpoints(void) {
  const float scales[] = {0.8f, 1.0f, 1.6f};

  for (size_t index = 0; index < sizeof(scales) / sizeof(scales[0]); ++index) {
    const float scale = scales[index];
    const ShroomLayoutResponsiveRow desktop = ShroomLayoutResponsiveRowMetrics(
        1180.0f, ShroomLayoutScaleMetric(104.0f, scale), 4,
        ShroomLayoutScaleMetric(10.0f, scale));
    const ShroomLayoutResponsiveRow narrow = ShroomLayoutResponsiveRowMetrics(
        360.0f, ShroomLayoutScaleMetric(104.0f, scale), 4,
        ShroomLayoutScaleMetric(10.0f, scale));

    TEST_ASSERT_EQUAL_INT(4, desktop.columns);
    TEST_ASSERT_TRUE(desktop.item_width >= ShroomLayoutScaleMetric(104.0f, scale));
    TEST_ASSERT_TRUE(narrow.columns >= 1);
    TEST_ASSERT_TRUE(narrow.item_width >= 1.0f);
  }

  TEST_ASSERT_EQUAL_INT(
      2, ShroomLayoutResponsiveRowMetrics(360.0f, 166.4f, 4, 16.0f).columns);
  TEST_ASSERT_EQUAL_INT(
      1, ShroomLayoutResponsiveRowMetrics(240.0f, 166.4f, 4, 16.0f).columns);
}

void test_help_content_reserves_scaled_back_action(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 463.2f,
                           ShroomLayoutReservedContentHeight(500.0f, 28.8f, 8.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 455.0f,
                           ShroomLayoutReservedContentHeight(500.0f, 36.0f, 9.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 426.4f,
                           ShroomLayoutReservedContentHeight(500.0f, 57.6f, 16.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                           ShroomLayoutReservedContentHeight(20.0f, 57.6f, 16.0f));
}

void test_help_card_height_accounts_for_wrapped_rows_and_scale(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 299.2f,
                           ShroomLayoutWrappedCardHeight(200.0f, 9, 0.8f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 324.0f,
                           ShroomLayoutWrappedCardHeight(200.0f, 9, 1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 372.8f,
                           ShroomLayoutWrappedCardHeight(200.0f, 7, 1.6f));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_scale_clamps_to_documented_endpoints);
  RUN_TEST(test_metrics_scale_at_supported_values);
  RUN_TEST(test_fitted_metrics_stay_inside_viewport_margins);
  RUN_TEST(test_labeled_item_width_reserves_label_and_spacing);
  RUN_TEST(test_labeled_item_width_handles_constrained_or_invalid_metrics);
  RUN_TEST(test_help_rows_scale_and_collapse_at_supported_endpoints);
  RUN_TEST(test_help_content_reserves_scaled_back_action);
  RUN_TEST(test_help_card_height_accounts_for_wrapped_rows_and_scale);
  return UNITY_END();
}
