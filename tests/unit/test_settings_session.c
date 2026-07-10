#include "unity.h"

#include "client/settings_session.h"

static ClientSettings current;
static ShroomSettingsSession session;

void setUp(void) {
  ClientSettingsSetDefaults(&current);
  current.ui_scale_percent = 125;
  ShroomSettingsSessionInit(&session, &current);
}

void tearDown(void) {}

void test_discard_restores_original_settings(void) {
  session.pending.ui_scale_percent = 160;
  ShroomSettingsSessionMarkDirty(&session);

  ShroomSettingsSessionDiscard(&session);

  TEST_ASSERT_FALSE(session.dirty);
  TEST_ASSERT_EQUAL_INT(125, session.pending.ui_scale_percent);
}

void test_commit_makes_pending_settings_the_new_baseline(void) {
  session.pending.ui_scale_percent = 140;
  ShroomSettingsSessionMarkDirty(&session);
  ShroomSettingsSessionCommit(&session);
  session.pending.ui_scale_percent = 90;
  ShroomSettingsSessionMarkDirty(&session);

  ShroomSettingsSessionDiscard(&session);

  TEST_ASSERT_EQUAL_INT(140, session.pending.ui_scale_percent);
  TEST_ASSERT_FALSE(session.dirty);
}

void test_restore_defaults_changes_pending_values_only(void) {
  ShroomSettingsSessionRestoreDefaults(&session);

  TEST_ASSERT_TRUE(session.dirty);
  TEST_ASSERT_EQUAL_INT(100, session.pending.ui_scale_percent);
  TEST_ASSERT_EQUAL_INT(125, session.original.ui_scale_percent);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_discard_restores_original_settings);
  RUN_TEST(test_commit_makes_pending_settings_the_new_baseline);
  RUN_TEST(test_restore_defaults_changes_pending_values_only);
  return UNITY_END();
}
