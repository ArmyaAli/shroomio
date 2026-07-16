#ifndef SHROOM_SERVER_CHAT_HISTORY_H
#define SHROOM_SERVER_CHAT_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sqlite3.h>

#include "shared/protocol.h"

#define SHROOM_SERVER_CHAT_HISTORY_MAX_MESSAGES 50u
#define SHROOM_SERVER_CHAT_HISTORY_MAX_BYTES (16u * 1024u)
#define SHROOM_SERVER_CHAT_HISTORY_RETENTION_SECONDS (7u * 24u * 60u * 60u)
#define SHROOM_SERVER_CHAT_HISTORY_FLUSH_INTERVAL_MS 1000u

typedef struct ShroomServerChatHistoryMessage {
  uint32_t sender_id;
  uint64_t message_id;
  uint32_t timestamp_sec;
  char sender_name[SHROOM_MAX_NAME_LENGTH];
  char message[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
} ShroomServerChatHistoryMessage;

typedef struct ShroomServerChatHistory {
  uint64_t identity;
  size_t message_count;
  size_t byte_count;
  ShroomServerChatHistoryMessage messages[SHROOM_SERVER_CHAT_HISTORY_MAX_MESSAGES];
  bool dirty;
  uint64_t last_flush_ms;
} ShroomServerChatHistory;

void ShroomServerChatHistoryInit(ShroomServerChatHistory* history, uint64_t identity);
bool ShroomServerChatHistoryLoad(sqlite3* db, ShroomServerChatHistory* history, uint32_t now_sec);
bool ShroomServerChatHistoryAppend(ShroomServerChatHistory* history,
                                   const ShroomServerChatHistoryMessage* message, uint32_t now_sec);
bool ShroomServerChatHistoryFlush(sqlite3* db, ShroomServerChatHistory* history, uint64_t now_ms);
bool ShroomServerChatHistoryFlushIfDue(sqlite3* db, ShroomServerChatHistory* history,
                                       uint64_t now_ms);

#endif
