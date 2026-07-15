#include "unity.h"

#include "client/client_settings.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char settings_path[256];

static void SidePath(char* destination, size_t size, const char* suffix) {
  snprintf(destination, size, "%s%s", settings_path, suffix);
}

static void CleanupFiles(void) {
  char path[300];
  const char* suffixes[] = {"", ".tmp", ".bak", ".bak.tmp"};

  for (size_t index = 0u; index < sizeof(suffixes) / sizeof(suffixes[0]); ++index) {
    SidePath(path, sizeof(path), suffixes[index]);
    unlink(path);
  }
}

static ClientSettings ExampleSettings(int scale, int volume, const char* name) {
  ClientSettings settings;

  ClientSettingsSetDefaults(&settings);
  settings.ui_scale_percent = scale;
  settings.master_volume_percent = volume;
  settings.voice_enabled = false;
  settings.voice_self_muted = true;
  settings.voice_output_volume_percent = 37;
  snprintf(settings.voice_capture_device, sizeof(settings.voice_capture_device), "%s",
           "Studio Mic");
  snprintf(settings.player_name, sizeof(settings.player_name), "%s", name);
  return settings;
}

static bool WriteLegacyFile(const char* path, const ClientSettings* settings) {
  FILE* file = fopen(path, "wb");

  if (file == NULL) {
    return false;
  }
  fprintf(file, "ui_scale_percent=%d\n", settings->ui_scale_percent);
  fprintf(file, "player_name=%s\n", settings->player_name);
  fprintf(file, "master_volume_percent=%d\n", settings->master_volume_percent);
  fprintf(file, "music_volume_percent=%d\n", settings->music_volume_percent);
  fprintf(file, "effects_volume_percent=%d\n", settings->effects_volume_percent);
  fprintf(file, "invert_mouse=%d\n", settings->invert_mouse ? 1 : 0);
  fprintf(file, "diagnostics_enabled=%d\n", settings->diagnostics_enabled ? 1 : 0);
  fprintf(file, "show_ping_ms=%d\n", settings->show_ping_ms ? 1 : 0);
  fprintf(file, "menu_animations_enabled=%d\n", settings->menu_animations_enabled ? 1 : 0);
  fprintf(file, "death_cutscene_enabled=%d\n", settings->death_cutscene_enabled ? 1 : 0);
  fprintf(file, "preferred_region_index=%d\n", settings->preferred_region_index);
  fprintf(file, "palette_preset=%d\n", (int)settings->palette_preset);
  fprintf(file, "hud_density=%d\n", (int)settings->hud_density);
  fprintf(file, "particle_quality=%d\n", (int)settings->particle_quality);
  fprintf(file, "mushroom_species=%d\n", (int)settings->mushroom_species);
  fprintf(file, "camera_zoom_x100=%d\n", (int)(settings->camera_zoom * 100.0f));
  fprintf(file, "key_chat_open=%d\n", settings->key_chat_open);
  fprintf(file, "key_hud_toggle=%d\n", settings->key_hud_toggle);
  fprintf(file, "key_pause_menu=%d\n", settings->key_pause_menu);
  fprintf(file, "key_push_to_talk=%d\n", settings->key_push_to_talk);
  return fclose(file) == 0;
}

static bool FileContains(const char* path, const char* needle) {
  FILE* file = fopen(path, "rb");
  char contents[2048] = {0};
  size_t count;

  if (file == NULL) {
    return false;
  }
  count = fread(contents, 1u, sizeof(contents) - 1u, file);
  fclose(file);
  contents[count] = '\0';
  return strstr(contents, needle) != NULL;
}

static bool RewriteAsSchemaTwo(const char* path) {
  char temporary[300];
  FILE* input = fopen(path, "rb");
  FILE* output;
  char line[256];
  bool success = true;

  if (input == NULL) {
    return false;
  }
  snprintf(temporary, sizeof(temporary), "%s.v2", path);
  output = fopen(temporary, "wb");
  if (output == NULL) {
    fclose(input);
    return false;
  }
  while (fgets(line, sizeof(line), input) != NULL) {
    if (strncmp(line, "account_features_enabled=", 25u) == 0) {
      continue;
    }
    if (strncmp(line, "schema_version=", 15u) == 0) {
      success = success && (fputs("schema_version=2\n", output) >= 0);
    } else {
      success = success && (fputs(line, output) >= 0);
    }
  }
  success = success && !ferror(input) && (fclose(input) == 0) && (fclose(output) == 0) &&
            (rename(temporary, path) == 0);
  if (!success) {
    remove(temporary);
  }
  return success;
}

void setUp(void) {
  snprintf(settings_path, sizeof(settings_path), "/tmp/shroomio-settings-%ld.cfg", (long)getpid());
  CleanupFiles();
}

void tearDown(void) { CleanupFiles(); }

void test_versioned_round_trip_leaves_no_temporary_file(void) {
  ClientSettings saved = ExampleSettings(130, 64, "Versioned Player");
  ClientSettings loaded;
  struct stat status;
  char temporary[300];

  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&saved, settings_path));
  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_EQUAL_INT(130, loaded.ui_scale_percent);
  TEST_ASSERT_EQUAL_INT(64, loaded.master_volume_percent);
  TEST_ASSERT_EQUAL_STRING("Versioned Player", loaded.player_name);
  TEST_ASSERT_FALSE(loaded.voice_enabled);
  TEST_ASSERT_TRUE(loaded.voice_self_muted);
  TEST_ASSERT_EQUAL_INT(37, loaded.voice_output_volume_percent);
  TEST_ASSERT_EQUAL_STRING("Studio Mic", loaded.voice_capture_device);
  TEST_ASSERT_TRUE(FileContains(settings_path, "schema_version=3\n"));
  TEST_ASSERT_FALSE(loaded.account_features_enabled);
  TEST_ASSERT_EQUAL_INT(0, stat(settings_path, &status));
  TEST_ASSERT_EQUAL_INT(0, status.st_mode & (S_IRWXG | S_IRWXO));
  SidePath(temporary, sizeof(temporary), ".tmp");
  TEST_ASSERT_EQUAL_INT(-1, access(temporary, F_OK));
}

void test_save_preserves_previous_valid_version_as_backup(void) {
  ClientSettings first = ExampleSettings(120, 51, "First");
  ClientSettings second = ExampleSettings(150, 72, "Second");
  ClientSettings backup;
  char backup_path[300];

  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&first, settings_path));
  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&second, settings_path));
  SidePath(backup_path, sizeof(backup_path), ".bak");
  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&backup, backup_path));
  TEST_ASSERT_EQUAL_INT(120, backup.ui_scale_percent);
  TEST_ASSERT_EQUAL_STRING("First", backup.player_name);
}

void test_complete_unversioned_file_migrates_to_current_schema(void) {
  ClientSettings legacy = ExampleSettings(140, 61, "Legacy Player");
  ClientSettings loaded;

  TEST_ASSERT_TRUE(WriteLegacyFile(settings_path, &legacy));
  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_EQUAL_INT(140, loaded.ui_scale_percent);
  TEST_ASSERT_EQUAL_STRING("Legacy Player", loaded.player_name);
  TEST_ASSERT_TRUE(loaded.voice_enabled);
  TEST_ASSERT_FALSE(loaded.voice_self_muted);
  TEST_ASSERT_EQUAL_INT(80, loaded.voice_output_volume_percent);
  TEST_ASSERT_EQUAL_STRING("", loaded.voice_capture_device);
  TEST_ASSERT_TRUE(FileContains(settings_path, "schema_version=3\n"));
}

void test_corrupt_primary_recovers_prior_valid_backup(void) {
  ClientSettings first = ExampleSettings(125, 48, "Backup Player");
  ClientSettings second = ExampleSettings(155, 88, "Latest Player");
  ClientSettings loaded;
  FILE* file;

  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&first, settings_path));
  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&second, settings_path));
  file = fopen(settings_path, "wb");
  TEST_ASSERT_NOT_NULL(file);
  fputs("schema_version=2\nui_scale_percent=truncated", file);
  fclose(file);

  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_EQUAL_INT(125, loaded.ui_scale_percent);
  TEST_ASSERT_EQUAL_STRING("Backup Player", loaded.player_name);
  TEST_ASSERT_TRUE(FileContains(settings_path, "schema_version=3\n"));
}

void test_truncated_files_fall_back_to_validated_defaults(void) {
  ClientSettings loaded;
  char backup_path[300];
  FILE* file = fopen(settings_path, "wb");

  TEST_ASSERT_NOT_NULL(file);
  fputs("ui_scale_percent=160\n", file);
  fclose(file);
  SidePath(backup_path, sizeof(backup_path), ".bak");
  file = fopen(backup_path, "wb");
  TEST_ASSERT_NOT_NULL(file);
  fputs("schema_version=99\nui_scale_percent=160\n", file);
  fclose(file);

  TEST_ASSERT_FALSE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_EQUAL_INT(100, loaded.ui_scale_percent);
  TEST_ASSERT_EQUAL_INT(80, loaded.master_volume_percent);
  TEST_ASSERT_EQUAL_INT(KEY_T, loaded.key_chat_open);
}

void test_missing_final_record_terminator_is_treated_as_truncation(void) {
  ClientSettings saved = ExampleSettings(145, 73, "Truncated Player");
  ClientSettings loaded;
  struct stat status;

  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&saved, settings_path));
  TEST_ASSERT_EQUAL_INT(0, stat(settings_path, &status));
  TEST_ASSERT_GREATER_THAN(1, status.st_size);
  TEST_ASSERT_EQUAL_INT(0, truncate(settings_path, status.st_size - 1));

  TEST_ASSERT_FALSE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_EQUAL_INT(100, loaded.ui_scale_percent);
  TEST_ASSERT_EQUAL_INT(80, loaded.master_volume_percent);
}

void test_save_validates_out_of_range_values_before_replacement(void) {
  ClientSettings invalid = ExampleSettings(999, -20, "  Safe@@ Name  ");
  ClientSettings loaded;

  invalid.camera_zoom = 99.0f;
  invalid.key_chat_open = KEY_ENTER;
  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&invalid, settings_path));
  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_EQUAL_INT(100, loaded.ui_scale_percent);
  TEST_ASSERT_EQUAL_INT(80, loaded.master_volume_percent);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, loaded.camera_zoom);
  TEST_ASSERT_EQUAL_INT(KEY_T, loaded.key_chat_open);
  TEST_ASSERT_EQUAL_STRING("Safe Name", loaded.player_name);
}

void test_schema_two_migrates_with_account_features_disabled(void) {
  ClientSettings saved = ExampleSettings(120, 60, "Schema Two");
  ClientSettings loaded;

  saved.account_features_enabled = true;
  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&saved, settings_path));
  TEST_ASSERT_TRUE(RewriteAsSchemaTwo(settings_path));
  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_FALSE(loaded.account_features_enabled);
  TEST_ASSERT_TRUE(FileContains(settings_path, "schema_version=3\n"));
  TEST_ASSERT_TRUE(FileContains(settings_path, "account_features_enabled=0\n"));
}

void test_account_feature_opt_in_round_trips(void) {
  ClientSettings saved = ExampleSettings(100, 80, "Account Opt In");
  ClientSettings loaded;

  saved.account_features_enabled = true;
  TEST_ASSERT_TRUE(ClientSettingsSaveToPath(&saved, settings_path));
  TEST_ASSERT_TRUE(ClientSettingsLoadFromPath(&loaded, settings_path));
  TEST_ASSERT_TRUE(loaded.account_features_enabled);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_versioned_round_trip_leaves_no_temporary_file);
  RUN_TEST(test_save_preserves_previous_valid_version_as_backup);
  RUN_TEST(test_complete_unversioned_file_migrates_to_current_schema);
  RUN_TEST(test_corrupt_primary_recovers_prior_valid_backup);
  RUN_TEST(test_truncated_files_fall_back_to_validated_defaults);
  RUN_TEST(test_missing_final_record_terminator_is_treated_as_truncation);
  RUN_TEST(test_save_validates_out_of_range_values_before_replacement);
  RUN_TEST(test_schema_two_migrates_with_account_features_disabled);
  RUN_TEST(test_account_feature_opt_in_round_trips);
  return UNITY_END();
}
