#include "unity.h"

#include "client/results_summary.h"

void setUp(void) {}

void tearDown(void) {}

void test_elapsed_seconds_truncates_and_clamps_to_zero(void) {
  TEST_ASSERT_EQUAL_UINT32(65u, ShroomResultsElapsedSeconds(10.0f, 75.9f));
  TEST_ASSERT_EQUAL_UINT32(0u, ShroomResultsElapsedSeconds(10.0f, 9.0f));
  TEST_ASSERT_EQUAL_UINT32(0u, ShroomResultsElapsedSeconds(10.0f, 10.0f));
}

void test_duration_format_uses_minutes_and_zero_padded_seconds(void) {
  char duration[32];

  ShroomResultsFormatDuration(0u, duration, sizeof(duration));
  TEST_ASSERT_EQUAL_STRING("0:00", duration);

  ShroomResultsFormatDuration(65u, duration, sizeof(duration));
  TEST_ASSERT_EQUAL_STRING("1:05", duration);

  ShroomResultsFormatDuration(3661u, duration, sizeof(duration));
  TEST_ASSERT_EQUAL_STRING("61:01", duration);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_elapsed_seconds_truncates_and_clamps_to_zero);
  RUN_TEST(test_duration_format_uses_minutes_and_zero_padded_seconds);
  return UNITY_END();
}
