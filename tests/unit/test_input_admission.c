#include "unity.h"

#include <stdint.h>

#include "server/input_admission.h"

void setUp(void) {}
void tearDown(void) {}

static void test_first_and_forward_sequences_are_admitted(void) {
  ShroomInputAdmission admission = {0};

  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 10u, 1000u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 11u, 1000u));
  TEST_ASSERT_EQUAL_UINT32(11u, admission.last_sequence);
  TEST_ASSERT_EQUAL_UINT64(2u, admission.accepted_count);
}

static void test_reserved_zero_is_rejected_before_first_sequence(void) {
  ShroomInputAdmission admission = {0};

  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_STALE, ShroomInputAdmissionCheck(&admission, 0u, 1000u));
  TEST_ASSERT_FALSE(admission.has_sequence);
  TEST_ASSERT_EQUAL_UINT64(1u, admission.stale_count);
  TEST_ASSERT_EQUAL_UINT64(0u, admission.accepted_count);
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 1u, 1000u));
  TEST_ASSERT_EQUAL_UINT32(1u, admission.last_sequence);
}

static void test_duplicate_and_reordered_sequences_are_rejected(void) {
  ShroomInputAdmission admission = {0};

  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 50u, 0u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_STALE, ShroomInputAdmissionCheck(&admission, 50u, 1u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_STALE, ShroomInputAdmissionCheck(&admission, 49u, 2u));
  TEST_ASSERT_EQUAL_UINT32(50u, admission.last_sequence);
  TEST_ASSERT_EQUAL_UINT64(2u, admission.stale_count);
}

static void test_sequence_order_is_wrap_aware(void) {
  ShroomInputAdmission admission = {0};

  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED,
                    ShroomInputAdmissionCheck(&admission, UINT32_MAX - 1u, 0u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, UINT32_MAX, 1u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 1u, 2u));
  TEST_ASSERT_EQUAL_UINT32(1u, admission.last_sequence);
}

static void test_reserved_zero_does_not_advance_wrapped_sequence(void) {
  ShroomInputAdmission admission = {0};

  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, UINT32_MAX, 0u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_STALE, ShroomInputAdmissionCheck(&admission, 0u, 1u));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, admission.last_sequence);
  TEST_ASSERT_EQUAL_UINT64(1u, admission.stale_count);
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 1u, 2u));
  TEST_ASSERT_EQUAL_UINT32(1u, admission.last_sequence);
}

static void test_burst_capacity_and_30_hz_refill_are_enforced(void) {
  ShroomInputAdmission admission = {0};

  for (uint32_t sequence = 1u; sequence <= SHROOM_SERVER_INPUT_BURST_CAPACITY; ++sequence) {
    TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED,
                      ShroomInputAdmissionCheck(&admission, sequence, 1000u));
  }
  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_RATE, ShroomInputAdmissionCheck(&admission, 11u, 1000u));
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 11u, 1034u));
  TEST_ASSERT_EQUAL_UINT64(1u, admission.rate_limited_count);
}

static void test_stale_attempts_consume_flood_credit(void) {
  ShroomInputAdmission admission = {0};

  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 8u, 0u));
  for (uint32_t attempt = 1u; attempt < SHROOM_SERVER_INPUT_BURST_CAPACITY; ++attempt) {
    TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_STALE, ShroomInputAdmissionCheck(&admission, 8u, 0u));
  }
  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_RATE, ShroomInputAdmissionCheck(&admission, 9u, 0u));
  TEST_ASSERT_EQUAL_UINT32(8u, admission.last_sequence);
}

static void test_rate_rejection_does_not_advance_latest_sequence(void) {
  ShroomInputAdmission admission = {0};

  for (uint32_t sequence = 1u; sequence <= SHROOM_SERVER_INPUT_BURST_CAPACITY; ++sequence) {
    TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, sequence, 0u));
  }
  TEST_ASSERT_EQUAL(SHROOM_INPUT_REJECTED_RATE, ShroomInputAdmissionCheck(&admission, 99u, 0u));
  TEST_ASSERT_EQUAL_UINT32(SHROOM_SERVER_INPUT_BURST_CAPACITY, admission.last_sequence);
  TEST_ASSERT_EQUAL(SHROOM_INPUT_ADMITTED, ShroomInputAdmissionCheck(&admission, 99u, 34u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_first_and_forward_sequences_are_admitted);
  RUN_TEST(test_reserved_zero_is_rejected_before_first_sequence);
  RUN_TEST(test_duplicate_and_reordered_sequences_are_rejected);
  RUN_TEST(test_sequence_order_is_wrap_aware);
  RUN_TEST(test_reserved_zero_does_not_advance_wrapped_sequence);
  RUN_TEST(test_burst_capacity_and_30_hz_refill_are_enforced);
  RUN_TEST(test_stale_attempts_consume_flood_credit);
  RUN_TEST(test_rate_rejection_does_not_advance_latest_sequence);
  return UNITY_END();
}
