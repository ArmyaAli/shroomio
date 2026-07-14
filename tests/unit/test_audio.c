#include "unity.h"

#include "client/audio.h"

#include <string.h>

typedef struct FakeAudioBackend {
  bool ready;
  bool fail_init;
  bool fail_load;
  int init_count;
  int close_count;
  int load_count;
  int unload_count;
  int apply_count;
  int update_count;
  double now_seconds;
  ClientSettings applied_settings;
} FakeAudioBackend;

static FakeAudioBackend fake;

static ClientSettings TestSettings(void) {
  return (ClientSettings){
      .master_volume_percent = 80,
      .music_volume_percent = 55,
      .effects_volume_percent = 75,
  };
}

static bool FakeInit(void* context) {
  FakeAudioBackend* backend = context;
  backend->init_count += 1;
  backend->ready = !backend->fail_init;
  return backend->ready;
}

static bool FakeReady(void* context) { return ((FakeAudioBackend*)context)->ready; }

static void FakeClose(void* context) {
  FakeAudioBackend* backend = context;
  backend->close_count += 1;
  backend->ready = false;
}

static bool FakeLoad(void* context) {
  FakeAudioBackend* backend = context;
  backend->load_count += 1;
  return !backend->fail_load;
}

static void FakeUnload(void* context) { ((FakeAudioBackend*)context)->unload_count += 1; }

static void FakeApply(void* context, const ClientSettings* settings) {
  FakeAudioBackend* backend = context;
  backend->apply_count += 1;
  backend->applied_settings = *settings;
}

static void FakeUpdate(void* context, const ClientSettings* settings) {
  FakeAudioBackend* backend = context;
  backend->update_count += 1;
  backend->applied_settings = *settings;
}

static double FakeNow(void* context) { return ((FakeAudioBackend*)context)->now_seconds; }

static void InstallFakeBackend(void) {
  const ShroomClientAudioTestBackend backend = {
      .context = &fake,
      .init_device = FakeInit,
      .device_ready = FakeReady,
      .close_device = FakeClose,
      .load_assets = FakeLoad,
      .unload_assets = FakeUnload,
      .apply_settings = FakeApply,
      .update_music = FakeUpdate,
      .now_seconds = FakeNow,
  };
  ShroomClientAudioTestSetBackend(&backend);
}

void setUp(void) {
  memset(&fake, 0, sizeof(fake));
  ShroomClientAudioTestResetThrottleState();
}

void tearDown(void) { ShroomClientAudioTestSetBackend(NULL); }

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

static void test_process_audio_init_is_idempotent_and_shutdown_clears_assets(void) {
  ClientSettings settings;

  settings = TestSettings();
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_TRUE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.init_count);
  TEST_ASSERT_EQUAL_INT(1, fake.load_count);

  ShroomClientAudioShutdown();
  ShroomClientAudioShutdown();
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.unload_count);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);
}

static void test_restart_recovers_injected_backend_failure_and_preserves_settings(void) {
  ClientSettings settings;

  settings = TestSettings();
  settings.master_volume_percent = 37;
  settings.music_volume_percent = 61;
  settings.effects_volume_percent = 83;
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));

  fake.ready = false;
  fake.fail_init = true;
  TEST_ASSERT_FALSE(ShroomClientAudioRestart(&settings));
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);
  TEST_ASSERT_NOT_NULL(strstr(ShroomClientAudioGetStatus(), "failed"));

  fake.fail_init = false;
  TEST_ASSERT_TRUE(ShroomClientAudioRestart(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(37, fake.applied_settings.master_volume_percent);
  TEST_ASSERT_EQUAL_INT(61, fake.applied_settings.music_volume_percent);
  TEST_ASSERT_EQUAL_INT(83, fake.applied_settings.effects_volume_percent);
}

static void test_failed_asset_load_clears_handles_and_can_retry(void) {
  ClientSettings settings;

  settings = TestSettings();
  fake.fail_load = true;
  InstallFakeBackend();
  TEST_ASSERT_FALSE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.unload_count);
  TEST_ASSERT_NOT_NULL(strstr(ShroomClientAudioGetStatus(), "failed"));

  fake.fail_load = false;
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioTestAssetsLoaded());
}

static void test_init_device_failure_leaves_clean_state(void) {
  ClientSettings settings;

  settings = TestSettings();
  fake.fail_init = true;
  InstallFakeBackend();
  TEST_ASSERT_FALSE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.init_count);
  TEST_ASSERT_EQUAL_INT(0, fake.close_count);
  TEST_ASSERT_EQUAL_INT(0, fake.load_count);
  TEST_ASSERT_NOT_NULL(strstr(ShroomClientAudioGetStatus(), "failed"));

  fake.fail_init = false;
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_TRUE(ShroomClientAudioTestAssetsLoaded());
}

static void test_muted_playback_init_succeeds(void) {
  ClientSettings settings;

  settings = TestSettings();
  settings.music_volume_percent = 0;
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_TRUE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(0, fake.applied_settings.music_volume_percent);
  TEST_ASSERT_EQUAL_INT(80, fake.applied_settings.master_volume_percent);
  TEST_ASSERT_EQUAL_INT(75, fake.applied_settings.effects_volume_percent);
}

static void test_stale_handles_reset_after_shutdown(void) {
  ClientSettings settings;

  settings = TestSettings();
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_TRUE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.init_count);
  TEST_ASSERT_EQUAL_INT(1, fake.load_count);

  ShroomClientAudioShutdown();
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.unload_count);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);

  /* After shutdown, handles are cleared. A second init reinitializes fresh. */
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_TRUE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(2, fake.init_count);
  TEST_ASSERT_EQUAL_INT(2, fake.load_count);
}

static void test_reinit_reload_cycle_tracks_counters(void) {
  ClientSettings settings;

  settings = TestSettings();
  InstallFakeBackend();

  /* First cycle */
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(1, fake.init_count);
  TEST_ASSERT_EQUAL_INT(1, fake.load_count);
  TEST_ASSERT_EQUAL_INT(0, fake.unload_count);
  TEST_ASSERT_EQUAL_INT(0, fake.close_count);

  ShroomClientAudioShutdown();
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(1, fake.unload_count);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);

  /* Second cycle */
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(2, fake.init_count);
  TEST_ASSERT_EQUAL_INT(2, fake.load_count);
  TEST_ASSERT_EQUAL_INT(1, fake.unload_count);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);

  ShroomClientAudioShutdown();
  TEST_ASSERT_EQUAL_INT(2, fake.unload_count);
  TEST_ASSERT_EQUAL_INT(2, fake.close_count);
}

static void test_shutdown_without_init_is_safe(void) {
  InstallFakeBackend();
  ShroomClientAudioShutdown();
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(0, fake.init_count);
  TEST_ASSERT_EQUAL_INT(0, fake.close_count);
  TEST_ASSERT_EQUAL_INT(0, fake.load_count);
  TEST_ASSERT_EQUAL_INT(0, fake.unload_count);
}

static void test_multiple_shutdown_calls_are_idempotent(void) {
  ClientSettings settings;

  settings = TestSettings();
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));

  ShroomClientAudioShutdown();
  ShroomClientAudioShutdown();
  ShroomClientAudioShutdown();
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);
  TEST_ASSERT_EQUAL_INT(1, fake.unload_count);
}

static void test_music_update_is_serviced_once_per_explicit_frame(void) {
  ClientSettings settings = TestSettings();

  InstallFakeBackend();
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(0, fake.update_count);

  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  for (int frame = 0; frame < 180; ++frame) {
    ShroomClientAudioUpdateMusic(&settings);
  }
  TEST_ASSERT_EQUAL_INT(180, fake.update_count);
  TEST_ASSERT_EQUAL_INT(settings.music_volume_percent, fake.applied_settings.music_volume_percent);

  ShroomClientAudioShutdown();
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(180, fake.update_count);
}

static void test_runtime_device_loss_recovers_without_using_stale_handles(void) {
  ClientSettings settings = TestSettings();

  fake.now_seconds = 10.0;
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(1, fake.update_count);

  fake.ready = false;
  fake.fail_init = true;
  fake.now_seconds = 10.1;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_FALSE(ShroomClientAudioIsReady());
  TEST_ASSERT_FALSE(ShroomClientAudioTestAssetsLoaded());
  TEST_ASSERT_EQUAL_INT(1, fake.update_count);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);
  TEST_ASSERT_EQUAL_INT(0, fake.unload_count);
  TEST_ASSERT_NOT_NULL(strstr(ShroomClientAudioGetStatus(), "lost"));

  fake.now_seconds = 11.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(1, fake.init_count);
  fake.now_seconds = 11.1;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(2, fake.init_count);
  TEST_ASSERT_EQUAL_INT(1, fake.load_count);

  settings.master_volume_percent = 31;
  settings.music_volume_percent = 42;
  settings.effects_volume_percent = 53;
  fake.fail_init = false;
  fake.now_seconds = 13.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(2, fake.init_count);
  fake.now_seconds = 13.2;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(3, fake.init_count);
  TEST_ASSERT_EQUAL_INT(2, fake.load_count);
  TEST_ASSERT_EQUAL_INT(2, fake.apply_count);
  TEST_ASSERT_EQUAL_INT(31, fake.applied_settings.master_volume_percent);
  TEST_ASSERT_EQUAL_INT(42, fake.applied_settings.music_volume_percent);
  TEST_ASSERT_EQUAL_INT(53, fake.applied_settings.effects_volume_percent);
  TEST_ASSERT_EQUAL_STRING("Audio ready.", ShroomClientAudioGetStatus());

  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(2, fake.update_count);
}

static void test_recovery_backoff_is_bounded_and_repeated_loss_is_idempotent(void) {
  ClientSettings settings = TestSettings();

  fake.now_seconds = 20.0;
  InstallFakeBackend();
  TEST_ASSERT_TRUE(ShroomClientAudioInit(&settings));
  fake.ready = false;
  fake.fail_init = true;
  ShroomClientAudioUpdateMusic(&settings);
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);

  fake.now_seconds = 21.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(2, fake.init_count);
  fake.now_seconds = 23.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(3, fake.init_count);
  fake.now_seconds = 27.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(4, fake.init_count);
  fake.now_seconds = 35.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(5, fake.init_count);
  fake.now_seconds = 42.9;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(5, fake.init_count);
  fake.now_seconds = 43.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(6, fake.init_count);
  TEST_ASSERT_EQUAL_INT(1, fake.close_count);

  fake.fail_init = false;
  fake.now_seconds = 51.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(7, fake.init_count);
  TEST_ASSERT_EQUAL_INT(2, fake.load_count);

  fake.ready = false;
  fake.now_seconds = 60.0;
  ShroomClientAudioUpdateMusic(&settings);
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_EQUAL_INT(2, fake.close_count);
  fake.now_seconds = 61.0;
  ShroomClientAudioUpdateMusic(&settings);
  TEST_ASSERT_TRUE(ShroomClientAudioIsReady());
  TEST_ASSERT_EQUAL_INT(8, fake.init_count);
  TEST_ASSERT_EQUAL_INT(3, fake.load_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_spore_sfx_is_throttled_during_rapid_growth);
  RUN_TEST(test_consume_sfx_is_throttled_but_recovers_quickly);
  RUN_TEST(test_warning_sfx_has_longer_cooldown);
  RUN_TEST(test_death_sfx_is_not_throttled);
  RUN_TEST(test_process_audio_init_is_idempotent_and_shutdown_clears_assets);
  RUN_TEST(test_restart_recovers_injected_backend_failure_and_preserves_settings);
  RUN_TEST(test_failed_asset_load_clears_handles_and_can_retry);
  RUN_TEST(test_init_device_failure_leaves_clean_state);
  RUN_TEST(test_muted_playback_init_succeeds);
  RUN_TEST(test_stale_handles_reset_after_shutdown);
  RUN_TEST(test_reinit_reload_cycle_tracks_counters);
  RUN_TEST(test_shutdown_without_init_is_safe);
  RUN_TEST(test_multiple_shutdown_calls_are_idempotent);
  RUN_TEST(test_music_update_is_serviced_once_per_explicit_frame);
  RUN_TEST(test_runtime_device_loss_recovers_without_using_stale_handles);
  RUN_TEST(test_recovery_backoff_is_bounded_and_repeated_loss_is_idempotent);
  return UNITY_END();
}
