#include "unity.h"

#include "client/client_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_root[] = "/tmp/shroomio-client-paths-XXXXXX";
static char g_cache_root[SHROOM_CLIENT_PATH_MAX];
static char g_legacy_path[SHROOM_CLIENT_PATH_MAX];

static void BuildPath(char* destination, size_t size, const char* suffix) {
  TEST_ASSERT_GREATER_THAN_INT(0, snprintf(destination, size, "%s/%s", g_root, suffix));
}

static void WriteText(const char* path, const char* text) {
  FILE* file = fopen(path, "wb");

  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1u, strlen(text), file));
  TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void ReadText(const char* path, char* text, size_t size) {
  FILE* file = fopen(path, "rb");

  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_NOT_NULL(fgets(text, (int)size, file));
  TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static bool ValidTextFile(const char* path, const void* context) {
  char text[32] = {0};
  FILE* file;

  (void)context;
  file = fopen(path, "rb");
  if (file == NULL) {
    return false;
  }
  const bool valid = (fgets(text, sizeof(text), file) != NULL) && (strcmp(text, "valid\n") == 0) &&
                     (fgetc(file) == EOF);
  fclose(file);
  return valid;
}

void setUp(void) {
  TEST_ASSERT_NOT_NULL(mkdtemp(g_root));
  BuildPath(g_cache_root, sizeof(g_cache_root), "cache");
  BuildPath(g_legacy_path, sizeof(g_legacy_path), "legacy.txt");
  ShroomClientPathsSetTestCacheRoot(g_cache_root);
}

void tearDown(void) {
  char path[SHROOM_CLIENT_PATH_MAX];

  BuildPath(path, sizeof(path), "cache/shroomio/state.txt.tmp");
  unlink(path);
  BuildPath(path, sizeof(path), "cache/shroomio/state.txt");
  unlink(path);
  BuildPath(path, sizeof(path), "cache/shroomio");
  rmdir(path);
  unlink(path);
  BuildPath(path, sizeof(path), "cache");
  rmdir(path);
  BuildPath(path, sizeof(path), "blocked");
  unlink(path);
  BuildPath(path, sizeof(path), "blocked/cache/shroomio/state.txt");
  unlink(path);
  BuildPath(path, sizeof(path), "blocked/cache/shroomio");
  rmdir(path);
  BuildPath(path, sizeof(path), "blocked/cache");
  rmdir(path);
  BuildPath(path, sizeof(path), "blocked");
  rmdir(path);
  unlink(g_legacy_path);
  ShroomClientPathsSetTestCacheRoot(NULL);
  rmdir(g_root);
  snprintf(g_root, sizeof(g_root), "%s", "/tmp/shroomio-client-paths-XXXXXX");
}

void test_platform_cache_paths_follow_native_conventions(void) {
  char path[SHROOM_CLIENT_PATH_MAX];

  TEST_ASSERT_TRUE(ShroomClientPathsBuildCacheFile(path, sizeof(path), SHROOM_CLIENT_PLATFORM_LINUX,
                                                   "/home/player", "/var/cache/player",
                                                   "state.txt"));
  TEST_ASSERT_EQUAL_STRING("/var/cache/player/shroomio/state.txt", path);
  TEST_ASSERT_TRUE(ShroomClientPathsBuildCacheFile(path, sizeof(path), SHROOM_CLIENT_PLATFORM_LINUX,
                                                   "/home/player", NULL, "state.txt"));
  TEST_ASSERT_EQUAL_STRING("/home/player/.cache/shroomio/state.txt", path);
  TEST_ASSERT_TRUE(ShroomClientPathsBuildCacheFile(
      path, sizeof(path), SHROOM_CLIENT_PLATFORM_WINDOWS, "C:\\Users\\player",
      "C:\\Users\\player\\AppData\\Local", "state.txt"));
  TEST_ASSERT_EQUAL_STRING("C:\\Users\\player\\AppData\\Local\\shroomio\\state.txt", path);
  TEST_ASSERT_TRUE(ShroomClientPathsBuildCacheFile(path, sizeof(path), SHROOM_CLIENT_PLATFORM_MACOS,
                                                   "/Users/player", NULL, "state.txt"));
  TEST_ASSERT_EQUAL_STRING("/Users/player/Library/Caches/shroomio/state.txt", path);
  TEST_ASSERT_FALSE(ShroomClientPathsBuildCacheFile(
      path, sizeof(path), SHROOM_CLIENT_PLATFORM_LINUX, "/home/player", NULL, "../state.txt"));
}

void test_valid_legacy_file_migrates_with_private_permissions(void) {
  char destination[SHROOM_CLIENT_PATH_MAX];
  char directory[SHROOM_CLIENT_PATH_MAX];
  char text[32];
  struct stat status;

  WriteText(g_legacy_path, "valid\n");
  TEST_ASSERT_TRUE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                     g_legacy_path, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_INT(-1, access(g_legacy_path, F_OK));
  ReadText(destination, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("valid\n", text);
  TEST_ASSERT_EQUAL_INT(0, stat(destination, &status));
  TEST_ASSERT_EQUAL_INT(0600, status.st_mode & 0777);
  BuildPath(directory, sizeof(directory), "cache/shroomio");
  TEST_ASSERT_EQUAL_INT(0, stat(directory, &status));
  TEST_ASSERT_EQUAL_INT(0700, status.st_mode & 0777);
}

void test_valid_destination_takes_precedence_and_discards_stale_legacy(void) {
  char destination[SHROOM_CLIENT_PATH_MAX];
  char text[32];
  struct stat status;

  WriteText(g_legacy_path, "valid\n");
  TEST_ASSERT_TRUE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                     g_legacy_path, 1024u, ValidTextFile, NULL));
  WriteText(g_legacy_path, "stale\n");
  TEST_ASSERT_TRUE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                     g_legacy_path, 1024u, ValidTextFile, NULL));
  ReadText(destination, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("valid\n", text);
  TEST_ASSERT_EQUAL_INT(-1, access(g_legacy_path, F_OK));
  TEST_ASSERT_EQUAL_INT(0, stat(destination, &status));
  TEST_ASSERT_EQUAL_INT(0600, status.st_mode & 0777);
}

void test_failed_migration_retains_legacy_and_can_retry(void) {
  char blocked_root[SHROOM_CLIENT_PATH_MAX];
  char destination[SHROOM_CLIENT_PATH_MAX] = "unchanged";

  BuildPath(blocked_root, sizeof(blocked_root), "blocked/cache");
  WriteText(g_legacy_path, "valid\n");
  {
    char blocker[SHROOM_CLIENT_PATH_MAX];
    BuildPath(blocker, sizeof(blocker), "blocked");
    WriteText(blocker, "not a directory\n");
  }
  ShroomClientPathsSetTestCacheRoot(blocked_root);
  TEST_ASSERT_FALSE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                      g_legacy_path, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_STRING("", destination);
  TEST_ASSERT_EQUAL_INT(0, access(g_legacy_path, F_OK));
  {
    char blocker[SHROOM_CLIENT_PATH_MAX];
    BuildPath(blocker, sizeof(blocker), "blocked");
    TEST_ASSERT_EQUAL_INT(0, unlink(blocker));
  }
  TEST_ASSERT_TRUE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                     g_legacy_path, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_INT(-1, access(g_legacy_path, F_OK));
}

void test_malformed_legacy_data_is_not_migrated(void) {
  char destination[SHROOM_CLIENT_PATH_MAX];
  char expected_destination[SHROOM_CLIENT_PATH_MAX];

  WriteText(g_legacy_path, "malformed\n");
  TEST_ASSERT_TRUE(ShroomClientPathsGetCacheFile(expected_destination, sizeof(expected_destination),
                                                 "state.txt"));
  TEST_ASSERT_FALSE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                      g_legacy_path, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_STRING("", destination);
  TEST_ASSERT_EQUAL_INT(0, access(g_legacy_path, F_OK));
  TEST_ASSERT_EQUAL_INT(-1, access(expected_destination, F_OK));
}

void test_oversized_legacy_data_is_not_migrated(void) {
  char destination[SHROOM_CLIENT_PATH_MAX] = "unchanged";

  WriteText(g_legacy_path, "valid\nextra bytes\n");
  TEST_ASSERT_FALSE(ShroomClientPathsPrepareCacheFile(destination, sizeof(destination), "state.txt",
                                                      g_legacy_path, 8u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_STRING("", destination);
  TEST_ASSERT_EQUAL_INT(0, access(g_legacy_path, F_OK));
}

void test_malformed_destination_without_legacy_disables_cache(void) {
  char destination[SHROOM_CLIENT_PATH_MAX];
  char expected_destination[SHROOM_CLIENT_PATH_MAX];

  TEST_ASSERT_TRUE(ShroomClientPathsGetCacheFile(expected_destination,
                                                 sizeof(expected_destination), "state.txt"));
  TEST_ASSERT_TRUE(ShroomClientPathsPrepareCacheFile(
      destination, sizeof(destination), "state.txt", NULL, 1024u, ValidTextFile, NULL));
  WriteText(destination, "malformed\n");
  TEST_ASSERT_FALSE(ShroomClientPathsPrepareCacheFile(
      destination, sizeof(destination), "state.txt", NULL, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_STRING("", destination);
  TEST_ASSERT_EQUAL_INT(0, access(expected_destination, F_OK));
}

void test_symlinked_cache_directory_is_rejected(void) {
  char cache_directory[SHROOM_CLIENT_PATH_MAX];
  char cache_link[SHROOM_CLIENT_PATH_MAX];
  char destination[SHROOM_CLIENT_PATH_MAX] = "unchanged";

  BuildPath(cache_directory, sizeof(cache_directory), "cache");
  TEST_ASSERT_EQUAL_INT(0, mkdir(cache_directory, 0700));
  BuildPath(cache_directory, sizeof(cache_directory), "outside");
  TEST_ASSERT_EQUAL_INT(0, mkdir(cache_directory, 0700));
  BuildPath(cache_link, sizeof(cache_link), "cache/shroomio");
  TEST_ASSERT_EQUAL_INT(0, symlink(cache_directory, cache_link));
  WriteText(g_legacy_path, "valid\n");

  TEST_ASSERT_FALSE(ShroomClientPathsPrepareCacheFile(
      destination, sizeof(destination), "state.txt", g_legacy_path, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_STRING("", destination);
  TEST_ASSERT_EQUAL_INT(0, access(g_legacy_path, F_OK));

  unlink(cache_link);
  rmdir(cache_directory);
}

void test_symlinked_legacy_file_is_not_migrated(void) {
  char target[SHROOM_CLIENT_PATH_MAX];
  char destination[SHROOM_CLIENT_PATH_MAX] = "unchanged";

  BuildPath(target, sizeof(target), "legacy-target.txt");
  WriteText(target, "valid\n");
  TEST_ASSERT_EQUAL_INT(0, symlink(target, g_legacy_path));
  TEST_ASSERT_FALSE(ShroomClientPathsPrepareCacheFile(
      destination, sizeof(destination), "state.txt", g_legacy_path, 1024u, ValidTextFile, NULL));
  TEST_ASSERT_EQUAL_STRING("", destination);
  TEST_ASSERT_EQUAL_INT(0, access(g_legacy_path, F_OK));
  unlink(g_legacy_path);
  unlink(target);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_platform_cache_paths_follow_native_conventions);
  RUN_TEST(test_valid_legacy_file_migrates_with_private_permissions);
  RUN_TEST(test_valid_destination_takes_precedence_and_discards_stale_legacy);
  RUN_TEST(test_failed_migration_retains_legacy_and_can_retry);
  RUN_TEST(test_malformed_legacy_data_is_not_migrated);
  RUN_TEST(test_oversized_legacy_data_is_not_migrated);
  RUN_TEST(test_malformed_destination_without_legacy_disables_cache);
  RUN_TEST(test_symlinked_cache_directory_is_rejected);
  RUN_TEST(test_symlinked_legacy_file_is_not_migrated);
  return UNITY_END();
}
