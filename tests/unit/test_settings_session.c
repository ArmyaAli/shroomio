#include "unity.h"

#include "client/settings_session.h"
#include "raylib.h"

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

void test_reserved_key_policy_identifies_interface_keys(void) {
  TEST_ASSERT_TRUE(ClientSettingsKeyIsReserved(KEY_ENTER));
  TEST_ASSERT_TRUE(ClientSettingsKeyIsReserved(KEY_TAB));
  TEST_ASSERT_FALSE(ClientSettingsKeyIsReserved(KEY_ESCAPE));
  TEST_ASSERT_FALSE(ClientSettingsKeyIsReserved(KEY_T));
  TEST_ASSERT_FALSE(ClientSettingsKeyIsReserved(KEY_NULL));
}

void test_validation_replaces_reserved_bindings_with_slot_defaults(void) {
  current.key_chat_open = KEY_ENTER;
  current.key_hud_toggle = KEY_TAB;
  current.key_pause_menu = KEY_ENTER;
  current.key_push_to_talk = KEY_TAB;

  ClientSettingsValidate(&current);

  TEST_ASSERT_EQUAL_INT(KEY_T, current.key_chat_open);
  TEST_ASSERT_EQUAL_INT(KEY_F2, current.key_hud_toggle);
  TEST_ASSERT_EQUAL_INT(KEY_ESCAPE, current.key_pause_menu);
  TEST_ASSERT_EQUAL_INT(KEY_V, current.key_push_to_talk);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_discard_restores_original_settings);
  RUN_TEST(test_commit_makes_pending_settings_the_new_baseline);
  RUN_TEST(test_restore_defaults_changes_pending_values_only);
  RUN_TEST(test_reserved_key_policy_identifies_interface_keys);
  RUN_TEST(test_validation_replaces_reserved_bindings_with_slot_defaults);
  return UNITY_END();
}
