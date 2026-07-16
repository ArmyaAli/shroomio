#include "unity.h"

#include "client/chat_cache.h"
#include "client/client_paths.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* kPath = "/tmp/shroomio-chat-cache-test.txt";
static const uint32_t kNow = 2000000u;

static ShroomChatCacheKey Key(const char* host, uint16_t port, uint32_t lobby_id) {
  ShroomChatCacheKey key = {.port = port, .lobby_id = lobby_id, .history_identity = lobby_id + 100u};
  snprintf(key.host, sizeof(key.host), "%s", host);
  return key;
}

static ChatMessage Message(uint32_t sender_id, uint32_t timestamp, const char* name,
                           const char* text) {
  ChatMessage message = {.sender_id = sender_id, .message_id = timestamp, .timestamp_sec = timestamp};
  snprintf(message.sender_name, sizeof(message.sender_name), "%s", name);
  snprintf(message.message, sizeof(message.message), "%s", text);
  return message;
}

void setUp(void) {
  unlink(kPath);
  unlink("/tmp/shroomio-chat-cache-test.txt.tmp");
}

void tearDown(void) {
  unlink(kPath);
  unlink("/tmp/shroomio-chat-cache-test.txt.tmp");
  ShroomClientPathsSetTestCacheRoot(NULL);
}

void test_cache_bounds_history_to_newest_messages(void) {
  const ShroomChatCacheKey key = Key("Example.COM", 7777u, 3u);
  ChatMessage loaded[SHROOM_CHAT_CACHE_MAX_MESSAGES];

  for (uint32_t index = 0u; index < SHROOM_CHAT_CACHE_MAX_MESSAGES + 10u; ++index) {
    char text[32];
    snprintf(text, sizeof(text), "message-%u", index);
    const ChatMessage message = Message(7u, kNow - 100u + index, "Sender", text);
    TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &message, kNow));
  }

  TEST_ASSERT_EQUAL_size_t(
      SHROOM_CHAT_CACHE_MAX_MESSAGES,
      ShroomChatCacheLoadContext(kPath, &key, kNow, loaded, SHROOM_CHAT_CACHE_MAX_MESSAGES));
  TEST_ASSERT_EQUAL_STRING("message-10", loaded[0].message);
  TEST_ASSERT_EQUAL_STRING("message-59", loaded[SHROOM_CHAT_CACHE_MAX_MESSAGES - 1u].message);
}

void test_cache_separates_servers_ports_and_lobbies(void) {
  const ShroomChatCacheKey first = Key("one.example", 7777u, 1u);
  const ShroomChatCacheKey other_server = Key("two.example", 7777u, 1u);
  const ShroomChatCacheKey other_port = Key("one.example", 7778u, 1u);
  const ShroomChatCacheKey other_lobby = Key("one.example", 7777u, 2u);
  const ChatMessage message = Message(1u, kNow, "One", "private context");
  ChatMessage loaded[2];

  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &first, &message, kNow));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomChatCacheLoadContext(kPath, &first, kNow, loaded, 2u));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomChatCacheLoadContext(kPath, &other_server, kNow, loaded, 2u));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomChatCacheLoadContext(kPath, &other_port, kNow, loaded, 2u));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomChatCacheLoadContext(kPath, &other_lobby, kNow, loaded, 2u));
}

void test_cache_separates_reused_lobby_ids_by_history_identity(void) {
  const ShroomChatCacheKey first = Key("one.example", 7777u, 5u);
  const ShroomChatCacheKey replacement =
      {.port = 7777u, .lobby_id = 5u, .history_identity = first.history_identity + 1u};
  const ChatMessage message = Message(1u, kNow, "One", "private context");
  ChatMessage loaded[2];

  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &first, &message, kNow));
  TEST_ASSERT_EQUAL_size_t(0u,
                           ShroomChatCacheLoadContext(kPath, &replacement, kNow, loaded, 2u));
}

void test_cache_sanitizes_in_place_and_deduplicates_reconnect_overlap(void) {
  const ShroomChatCacheKey key = Key("safe.example", 7777u, 4u);
  ChatMessage first = Message(9u, kNow, "Bad\nName", "hello\r\nworld\x01");
  ChatMessage duplicate = Message(9u, kNow, "Bad Name", "hello world");
  ChatMessage loaded[4];
  char in_place[32] = "  hello\n\tworld  ";

  ShroomChatCacheSanitizeText(in_place, sizeof(in_place), in_place);
  TEST_ASSERT_EQUAL_STRING("hello world", in_place);
  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &first, kNow));
  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &duplicate, kNow));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomChatCacheLoadContext(kPath, &key, kNow, loaded, 4u));
  TEST_ASSERT_EQUAL_STRING("Bad Name", loaded[0].sender_name);
  TEST_ASSERT_EQUAL_STRING("hello world", loaded[0].message);
}

void test_identical_text_with_distinct_ids_is_retained(void) {
  const ShroomChatCacheKey key = Key("repeat.example", 7777u, 4u);
  const ChatMessage first = Message(9u, kNow, "Player", "same reply");
  const ChatMessage second = Message(9u, kNow + 1u, "Player", "same reply");
  ChatMessage loaded[4];

  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &first, kNow));
  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &second, kNow + 1u));
  TEST_ASSERT_EQUAL_size_t(2u,
                           ShroomChatCacheLoadContext(kPath, &key, kNow + 1u, loaded, 4u));
}

void test_corrupt_and_oversized_cache_fail_closed(void) {
  const ShroomChatCacheKey key = Key("safe.example", 7777u, 4u);
  ChatMessage loaded[2];
  FILE* file = fopen(kPath, "w");
  TEST_ASSERT_NOT_NULL(file);
  fputs("SHROOM_CHAT_CACHE_V2\nnot|a|valid|record\n", file);
  fclose(file);
  TEST_ASSERT_EQUAL_size_t(0u, ShroomChatCacheLoadContext(kPath, &key, kNow, loaded, 2u));

  file = fopen(kPath, "w");
  TEST_ASSERT_NOT_NULL(file);
  TEST_ASSERT_EQUAL_INT(0, fseek(file, (long)SHROOM_CHAT_CACHE_MAX_FILE_BYTES, SEEK_SET));
  fputc('x', file);
  fclose(file);
  TEST_ASSERT_EQUAL_size_t(0u, ShroomChatCacheLoadContext(kPath, &key, kNow, loaded, 2u));
}

void test_retention_discards_expired_messages_and_rejects_future_data(void) {
  const ShroomChatCacheKey key = Key("safe.example", 7777u, 4u);
  const ChatMessage expired =
      Message(1u, kNow - SHROOM_CHAT_CACHE_RETENTION_SECONDS - 1u, "Old", "expired");
  const ChatMessage future = Message(1u, kNow + 301u, "Future", "invalid");
  const ChatMessage retained = Message(1u, kNow - 10u, "Now", "retained");
  ChatMessage loaded[2];

  TEST_ASSERT_FALSE(ShroomChatCacheStoreMessage(kPath, &key, &expired, kNow));
  TEST_ASSERT_FALSE(ShroomChatCacheStoreMessage(kPath, &key, &future, kNow));
  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &retained, kNow));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomChatCacheLoadContext(kPath, &key, kNow, loaded, 2u));
  TEST_ASSERT_EQUAL_STRING("retained", loaded[0].message);
}

void test_future_timestamp_validation_does_not_wrap(void) {
  const ShroomChatCacheKey key = Key("one.example", 7777u, 1u);
  const ChatMessage boundary = Message(1u, UINT32_MAX, "Boundary", "valid");

  TEST_ASSERT_TRUE(ShroomChatCacheStoreMessage(kPath, &key, &boundary, UINT32_MAX - 300u));
  unlink(kPath);
  TEST_ASSERT_FALSE(ShroomChatCacheStoreMessage(kPath, &key, &boundary, UINT32_MAX - 301u));
}

void test_default_path_migrates_valid_legacy_cache_out_of_working_directory(void) {
  char original_directory[SHROOM_CLIENT_PATH_MAX];
  char test_directory[] = "/tmp/shroomio-chat-migration-XXXXXX";
  char cache_root[SHROOM_CLIENT_PATH_MAX];
  char destination[SHROOM_CLIENT_PATH_MAX];
  const ShroomChatCacheKey key = Key("migration.example", 7777u, 8u);
  const ChatMessage message = Message(4u, kNow, "Player", "migrated message");
  ChatMessage loaded[1];

  TEST_ASSERT_NOT_NULL(getcwd(original_directory, sizeof(original_directory)));
  TEST_ASSERT_NOT_NULL(mkdtemp(test_directory));
  TEST_ASSERT_GREATER_THAN_INT(
      0, snprintf(cache_root, sizeof(cache_root), "%s/platform-cache", test_directory));
  TEST_ASSERT_EQUAL_INT(0, chdir(test_directory));
  ShroomClientPathsSetTestCacheRoot(cache_root);

  TEST_ASSERT_TRUE(
      ShroomChatCacheStoreMessage(SHROOM_CHAT_CACHE_LEGACY_PATH, &key, &message, kNow));
  TEST_ASSERT_TRUE(ShroomChatCachePrepareDefaultPath(destination, sizeof(destination), kNow));
  TEST_ASSERT_EQUAL_INT(-1, access(SHROOM_CHAT_CACHE_LEGACY_PATH, F_OK));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomChatCacheLoadContext(destination, &key, kNow, loaded, 1u));
  TEST_ASSERT_EQUAL_STRING("migrated message", loaded[0].message);

  TEST_ASSERT_EQUAL_INT(0, chdir(original_directory));
  unlink(destination);
  {
    char* separator = strrchr(destination, '/');
    TEST_ASSERT_NOT_NULL(separator);
    *separator = '\0';
    rmdir(destination);
  }
  rmdir(cache_root);
  rmdir(test_directory);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_cache_bounds_history_to_newest_messages);
  RUN_TEST(test_cache_separates_servers_ports_and_lobbies);
  RUN_TEST(test_cache_separates_reused_lobby_ids_by_history_identity);
  RUN_TEST(test_cache_sanitizes_in_place_and_deduplicates_reconnect_overlap);
  RUN_TEST(test_identical_text_with_distinct_ids_is_retained);
  RUN_TEST(test_corrupt_and_oversized_cache_fail_closed);
  RUN_TEST(test_retention_discards_expired_messages_and_rejects_future_data);
  RUN_TEST(test_future_timestamp_validation_does_not_wrap);
  RUN_TEST(test_default_path_migrates_valid_legacy_cache_out_of_working_directory);
  return UNITY_END();
}
