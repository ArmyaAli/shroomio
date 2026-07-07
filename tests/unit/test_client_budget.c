#include "unity.h"

#include "client/game.h"

void setUp(void) {}

void tearDown(void) {}

static size_t CountSampledItems(size_t total_count, size_t budget) {
  size_t sampled = 0u;

  for (size_t index = 0u; index < total_count; ++index) {
    if (ShroomClientShouldSampleIndexedItem(index, total_count, budget)) {
      sampled += 1u;
    }
  }

  return sampled;
}

static void test_sampling_keeps_all_items_within_budget(void) {
  TEST_ASSERT_EQUAL_size_t(24u, CountSampledItems(24u, 96u));
}

static void test_sampling_caps_large_spore_sets_near_budget(void) {
  TEST_ASSERT_LESS_OR_EQUAL_size_t(SHROOM_CLIENT_PROXIMITY_SPORE_DOT_BUDGET,
                                   CountSampledItems(1100u,
                                                     SHROOM_CLIENT_PROXIMITY_SPORE_DOT_BUDGET));
}

static void test_sampling_skips_when_budget_is_zero(void) {
  TEST_ASSERT_FALSE(ShroomClientShouldSampleIndexedItem(0u, 1100u, 0u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sampling_keeps_all_items_within_budget);
  RUN_TEST(test_sampling_caps_large_spore_sets_near_budget);
  RUN_TEST(test_sampling_skips_when_budget_is_zero);
  return UNITY_END();
}
