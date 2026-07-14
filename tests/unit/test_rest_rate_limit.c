#include "unity.h"

#include "server/rest_rate_limit.h"

static ShroomRestRateLimiter limiter;

void setUp(void) { ShroomRestRateLimiterInit(&limiter); }
void tearDown(void) {}

static void test_login_allows_five_attempts_then_rejects(void) {
  ShroomRestRateLimitResult result = {0};

  for (int attempt = 0; attempt < 5; ++attempt) {
    result = ShroomRestRateLimitCheck(&limiter, "127.0.0.1", SHROOM_REST_RATE_LOGIN, 100u);
    TEST_ASSERT_TRUE(result.allowed);
    TEST_ASSERT_EQUAL_UINT32(5u, result.limit);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(4 - attempt), result.remaining);
  }
  result = ShroomRestRateLimitCheck(&limiter, "127.0.0.1", SHROOM_REST_RATE_LOGIN, 100u);
  TEST_ASSERT_FALSE(result.allowed);
  TEST_ASSERT_EQUAL_UINT32(60u, result.retry_after);
}

static void test_windows_slide_and_keys_and_routes_are_independent(void) {
  for (int attempt = 0; attempt < 5; ++attempt) {
    TEST_ASSERT_TRUE(
        ShroomRestRateLimitCheck(&limiter, "one", SHROOM_REST_RATE_REGISTER, 10u).allowed);
  }
  TEST_ASSERT_FALSE(
      ShroomRestRateLimitCheck(&limiter, "one", SHROOM_REST_RATE_REGISTER, 69u).allowed);
  TEST_ASSERT_TRUE(
      ShroomRestRateLimitCheck(&limiter, "one", SHROOM_REST_RATE_REGISTER, 70u).allowed);
  TEST_ASSERT_TRUE(ShroomRestRateLimitCheck(&limiter, "two", SHROOM_REST_RATE_REGISTER, 10u).allowed);
  TEST_ASSERT_TRUE(ShroomRestRateLimitCheck(&limiter, "one", SHROOM_REST_RATE_LOGIN, 10u).allowed);
}

static void test_refresh_uses_ten_attempt_limit(void) {
  ShroomRestRateLimitResult result = {0};

  for (int attempt = 0; attempt < 10; ++attempt) {
    result = ShroomRestRateLimitCheck(&limiter, "client", SHROOM_REST_RATE_REFRESH, 500u);
    TEST_ASSERT_TRUE(result.allowed);
  }
  result = ShroomRestRateLimitCheck(&limiter, "client", SHROOM_REST_RATE_REFRESH, 500u);
  TEST_ASSERT_FALSE(result.allowed);
  TEST_ASSERT_EQUAL_UINT32(10u, result.limit);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_login_allows_five_attempts_then_rejects);
  RUN_TEST(test_windows_slide_and_keys_and_routes_are_independent);
  RUN_TEST(test_refresh_uses_ten_attempt_limit);
  return UNITY_END();
}
