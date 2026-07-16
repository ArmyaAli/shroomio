#include "unity.h"

#include "server/chat_history.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static sqlite3* g_db;

static ShroomServerChatHistoryMessage Message(uint64_t id, uint32_t timestamp, const char* text) {
  ShroomServerChatHistoryMessage message = {
      .sender_id = 7u, .message_id = id, .timestamp_sec = timestamp};
  snprintf(message.sender_name, sizeof(message.sender_name), "%s", "Player");
  snprintf(message.message, sizeof(message.message), "%s", text);
  return message;
}

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(SQLITE_OK, sqlite3_open(":memory:", &g_db));
  TEST_ASSERT_EQUAL_INT(
      SQLITE_OK, sqlite3_exec(g_db,
                              "CREATE TABLE chat_history (history_identity INTEGER NOT NULL, "
                              "message_id INTEGER NOT NULL, sender_id INTEGER NOT NULL, "
                              "timestamp_sec INTEGER NOT NULL, sender_name TEXT NOT NULL, "
                              "message TEXT NOT NULL, PRIMARY KEY(history_identity, message_id))",
                              NULL, NULL, NULL));
}

void tearDown(void) {
  sqlite3_close(g_db);
  g_db = NULL;
}

void test_append_is_bounded_and_deduplicated(void) {
  ShroomServerChatHistory history;
  ShroomServerChatHistoryInit(&history, 42u);
  for (uint64_t id = 1u; id <= SHROOM_SERVER_CHAT_HISTORY_MAX_MESSAGES + 2u; ++id) {
    ShroomServerChatHistoryMessage message = Message(id, 1000u + (uint32_t)id, "hello");
    TEST_ASSERT_TRUE(ShroomServerChatHistoryAppend(&history, &message, 2000u));
  }
  TEST_ASSERT_EQUAL_size_t(SHROOM_SERVER_CHAT_HISTORY_MAX_MESSAGES, history.message_count);
  TEST_ASSERT_EQUAL_UINT64(3u, history.messages[0].message_id);
  TEST_ASSERT_TRUE(ShroomServerChatHistoryAppend(&history, &history.messages[0], 2000u));
  TEST_ASSERT_EQUAL_size_t(SHROOM_SERVER_CHAT_HISTORY_MAX_MESSAGES, history.message_count);
}

void test_flush_and_restore_preserve_only_retained_history(void) {
  ShroomServerChatHistory history;
  ShroomServerChatHistory restored;
  const ShroomServerChatHistoryMessage retained = Message(10u, 1990u, "retained");
  const ShroomServerChatHistoryMessage expired = Message(
      11u, 2000u - SHROOM_SERVER_CHAT_HISTORY_RETENTION_SECONDS - 1u, "expired");

  ShroomServerChatHistoryInit(&history, 42u);
  TEST_ASSERT_TRUE(ShroomServerChatHistoryAppend(&history, &retained, 2000u));
  TEST_ASSERT_FALSE(ShroomServerChatHistoryAppend(&history, &expired, 2000u));
  TEST_ASSERT_TRUE(ShroomServerChatHistoryFlush(g_db, &history, 1000u));
  ShroomServerChatHistoryInit(&restored, 42u);
  TEST_ASSERT_TRUE(ShroomServerChatHistoryLoad(g_db, &restored, 2000u));
  TEST_ASSERT_EQUAL_size_t(1u, restored.message_count);
  TEST_ASSERT_EQUAL_STRING("retained", restored.messages[0].message);
}

void test_history_identities_isolate_rows(void) {
  ShroomServerChatHistory first;
  ShroomServerChatHistory other;
  const ShroomServerChatHistoryMessage message = Message(1u, 1990u, "private");

  ShroomServerChatHistoryInit(&first, 1u);
  ShroomServerChatHistoryInit(&other, 2u);
  TEST_ASSERT_TRUE(ShroomServerChatHistoryAppend(&first, &message, 2000u));
  TEST_ASSERT_TRUE(ShroomServerChatHistoryFlush(g_db, &first, 1000u));
  TEST_ASSERT_TRUE(ShroomServerChatHistoryLoad(g_db, &other, 2000u));
  TEST_ASSERT_EQUAL_size_t(0u, other.message_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_append_is_bounded_and_deduplicated);
  RUN_TEST(test_flush_and_restore_preserve_only_retained_history);
  RUN_TEST(test_history_identities_isolate_rows);
  return UNITY_END();
}
