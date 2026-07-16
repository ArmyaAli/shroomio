#include "unity.h"

#include "client/settings_deferred.h"

static int writes;
static bool fail_writes;
static ClientSettings last_written;

static bool Writer(const ClientSettings* settings, void* context) {
  (void)context;
  ++writes;
  last_written = *settings;
  return !fail_writes;
}

void setUp(void) {
  writes = 0;
  fail_writes = false;
  last_written = (ClientSettings){0};
}
void tearDown(void) {}

static ClientSettings Settings(int zoom) {
  ClientSettings settings;
  ClientSettingsSetDefaults(&settings);
  settings.camera_zoom = (float)zoom;
  return settings;
}

void test_burst_coalesces_and_keeps_newest_snapshot(void) {
  ShroomSettingsDeferred state;
  ClientSettings first = Settings(1);
  ClientSettings second = Settings(2);

  ShroomSettingsDeferredInit(&state, Writer, NULL);
  ShroomSettingsDeferredMarkDirty(&state, &first, 10.0);
  ShroomSettingsDeferredMarkDirty(&state, &second, 10.1);
  ShroomSettingsDeferredUpdate(&state, 10.59);
  TEST_ASSERT_EQUAL_INT(0, writes);
  ShroomSettingsDeferredUpdate(&state, 10.6);
  TEST_ASSERT_EQUAL_INT(1, writes);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, last_written.camera_zoom);
}

void test_failure_retries_and_flushes(void) {
  ShroomSettingsDeferred state;
  ClientSettings settings = Settings(2);

  ShroomSettingsDeferredInit(&state, Writer, NULL);
  ShroomSettingsDeferredMarkDirty(&state, &settings, 0.0);
  fail_writes = true;
  ShroomSettingsDeferredUpdate(&state, 0.5);
  TEST_ASSERT_TRUE(ShroomSettingsDeferredConsumeWarning(&state));
  TEST_ASSERT_FALSE(ShroomSettingsDeferredFlush(&state, 0.9));
  fail_writes = false;
  TEST_ASSERT_TRUE(ShroomSettingsDeferredFlush(&state, 1.0));
  TEST_ASSERT_EQUAL_INT(3, writes);
}

void test_shutdown_flushes_before_debounce(void) {
  ShroomSettingsDeferred state;
  ClientSettings settings = Settings(2);

  ShroomSettingsDeferredInit(&state, Writer, NULL);
  ShroomSettingsDeferredMarkDirty(&state, &settings, 20.0);
  TEST_ASSERT_TRUE(ShroomSettingsDeferredFlush(&state, 20.1));
  TEST_ASSERT_EQUAL_INT(1, writes);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_burst_coalesces_and_keeps_newest_snapshot);
  RUN_TEST(test_failure_retries_and_flushes);
  RUN_TEST(test_shutdown_flushes_before_debounce);
  return UNITY_END();
}
