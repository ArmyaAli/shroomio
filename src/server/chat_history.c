#include "chat_history.h"

#include <stdio.h>
#include <string.h>

static size_t MessageBytes(const ShroomServerChatHistoryMessage* message) {
  return strlen(message->sender_name) + strlen(message->message);
}

static bool Retained(uint32_t timestamp_sec, uint32_t now_sec) {
  return timestamp_sec != 0u && timestamp_sec <= now_sec &&
         now_sec - timestamp_sec <= SHROOM_SERVER_CHAT_HISTORY_RETENTION_SECONDS;
}

static void RemoveOldest(ShroomServerChatHistory* history) {
  if (history->message_count == 0u) {
    return;
  }
  history->byte_count -= MessageBytes(&history->messages[0]);
  memmove(&history->messages[0], &history->messages[1],
          (history->message_count - 1u) * sizeof(history->messages[0]));
  --history->message_count;
}

void ShroomServerChatHistoryInit(ShroomServerChatHistory* history, uint64_t identity) {
  if (history != NULL) {
    *history = (ShroomServerChatHistory){.identity = identity};
  }
}

bool ShroomServerChatHistoryLoad(sqlite3* db, ShroomServerChatHistory* history, uint32_t now_sec) {
  sqlite3_stmt* statement = NULL;
  bool success = false;
  int result;

  if ((db == NULL) || (history == NULL) || (history->identity == 0u) ||
      sqlite3_prepare_v2(db,
                         "SELECT sender_id, message_id, timestamp_sec, sender_name, message "
                         "FROM chat_history WHERE history_identity=?1 ORDER BY message_id ASC",
                         -1, &statement, NULL) != SQLITE_OK) {
    return false;
  }
  sqlite3_bind_int64(statement, 1, (sqlite3_int64)history->identity);
  while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
    ShroomServerChatHistoryMessage message = {0};
    const unsigned char* sender_name = sqlite3_column_text(statement, 3);
    const unsigned char* text = sqlite3_column_text(statement, 4);
    message.sender_id = (uint32_t)sqlite3_column_int64(statement, 0);
    message.message_id = (uint64_t)sqlite3_column_int64(statement, 1);
    message.timestamp_sec = (uint32_t)sqlite3_column_int64(statement, 2);
    snprintf(message.sender_name, sizeof(message.sender_name), "%s",
             sender_name != NULL ? (const char*)sender_name : "");
    snprintf(message.message, sizeof(message.message), "%s", text != NULL ? (const char*)text : "");
    if (Retained(message.timestamp_sec, now_sec) && message.message_id != 0u &&
        message.sender_name[0] != '\0' && message.message[0] != '\0') {
      (void)ShroomServerChatHistoryAppend(history, &message, now_sec);
    }
  }
  success = result == SQLITE_DONE;
  sqlite3_finalize(statement);
  history->dirty = false;
  return success;
}

bool ShroomServerChatHistoryAppend(ShroomServerChatHistory* history,
                                   const ShroomServerChatHistoryMessage* message, uint32_t now_sec) {
  size_t bytes;

  if ((history == NULL) || (message == NULL) || (history->identity == 0u) ||
      !Retained(message->timestamp_sec, now_sec) || message->message_id == 0u ||
      message->sender_name[0] == '\0' || message->message[0] == '\0') {
    return false;
  }
  for (size_t index = 0u; index < history->message_count; ++index) {
    if (history->messages[index].message_id == message->message_id) {
      return true;
    }
  }
  bytes = MessageBytes(message);
  while ((history->message_count >= SHROOM_SERVER_CHAT_HISTORY_MAX_MESSAGES) ||
         (history->byte_count + bytes > SHROOM_SERVER_CHAT_HISTORY_MAX_BYTES)) {
    RemoveOldest(history);
  }
  history->messages[history->message_count++] = *message;
  history->byte_count += bytes;
  history->dirty = true;
  return true;
}

bool ShroomServerChatHistoryFlush(sqlite3* db, ShroomServerChatHistory* history, uint64_t now_ms) {
  sqlite3_stmt* statement = NULL;

  if ((db == NULL) || (history == NULL) || !history->dirty) {
    return true;
  }
  if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_prepare_v2(db, "DELETE FROM chat_history WHERE history_identity=?1", -1,
                         &statement, NULL) != SQLITE_OK) {
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    return false;
  }
  sqlite3_bind_int64(statement, 1, (sqlite3_int64)history->identity);
  if (sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement);
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    return false;
  }
  sqlite3_finalize(statement);
  for (size_t index = 0u; index < history->message_count; ++index) {
    const ShroomServerChatHistoryMessage* message = &history->messages[index];
    if (sqlite3_prepare_v2(db,
                           "INSERT INTO chat_history(history_identity, message_id, sender_id, "
                           "timestamp_sec, sender_name, message) VALUES(?1,?2,?3,?4,?5,?6)",
                           -1, &statement, NULL) != SQLITE_OK) {
      sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
      return false;
    }
    sqlite3_bind_int64(statement, 1, (sqlite3_int64)history->identity);
    sqlite3_bind_int64(statement, 2, (sqlite3_int64)message->message_id);
    sqlite3_bind_int64(statement, 3, message->sender_id);
    sqlite3_bind_int64(statement, 4, message->timestamp_sec);
    sqlite3_bind_text(statement, 5, message->sender_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, message->message, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) != SQLITE_DONE) {
      sqlite3_finalize(statement);
      sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
      return false;
    }
    sqlite3_finalize(statement);
  }
  if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    return false;
  }
  history->dirty = false;
  history->last_flush_ms = now_ms;
  return true;
}

bool ShroomServerChatHistoryFlushIfDue(sqlite3* db, ShroomServerChatHistory* history,
                                       uint64_t now_ms) {
  if ((history == NULL) || !history->dirty ||
      (now_ms - history->last_flush_ms < SHROOM_SERVER_CHAT_HISTORY_FLUSH_INTERVAL_MS)) {
    return true;
  }
  return ShroomServerChatHistoryFlush(db, history, now_ms);
}
