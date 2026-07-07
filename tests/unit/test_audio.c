#include "unity.h"

#include "client/audio.h"

void setUp(void) { ShroomClientAudioTestResetThrottleState(); }

void tearDown(void) {}

static void test_spore_sfx_is_throttled_during_rapid_growth(void) {
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_SPORE, 10.00));
  TEST_ASSERT_FALSE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_SPORE, 10.05));
  TEST_ASSERT_FALSE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_SPORE, 10.15));
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_SPORE, 10.16));
}

static void test_consume_sfx_is_throttled_but_recovers_quickly(void) {
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_CONSUME, 4.00));
  TEST_ASSERT_FALSE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_CONSUME, 4.10));
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_CONSUME, 4.15));
}

static void test_warning_sfx_has_longer_cooldown(void) {
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_WARNING, 8.00));
  TEST_ASSERT_FALSE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_WARNING, 8.50));
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_WARNING, 8.75));
}

static void test_death_sfx_is_not_throttled(void) {
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_DEATH, 2.00));
  TEST_ASSERT_TRUE(ShroomClientAudioTestCanPlaySfx(SHROOM_CLIENT_SFX_DEATH, 2.01));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_spore_sfx_is_throttled_during_rapid_growth);
  RUN_TEST(test_consume_sfx_is_throttled_but_recovers_quickly);
  RUN_TEST(test_warning_sfx_has_longer_cooldown);
  RUN_TEST(test_death_sfx_is_not_throttled);
  return UNITY_END();
}
