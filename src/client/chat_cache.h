#ifndef SHROOM_CLIENT_CHAT_CACHE_H
#define SHROOM_CLIENT_CHAT_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_CHAT_CACHE_MAX_CONTEXTS 8u
#define SHROOM_CHAT_CACHE_MAX_MESSAGES 50u
#define SHROOM_CHAT_CACHE_MAX_FILE_BYTES (256u * 1024u)
#define SHROOM_CHAT_CACHE_RETENTION_SECONDS (7u * 24u * 60u * 60u)
#define SHROOM_CHAT_CACHE_DUPLICATE_WINDOW_SECONDS 5u
#define SHROOM_CHAT_CACHE_DEFAULT_PATH "chat_history_cache.txt"

typedef struct ChatMessage {
  uint32_t sender_id;
  uint32_t timestamp_sec;
  char sender_name[SHROOM_MAX_NAME_LENGTH];
  char message[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
} ChatMessage;

typedef struct ShroomChatCacheKey {
  char host[64];
  uint16_t port;
  uint32_t lobby_id;
} ShroomChatCacheKey;

void ShroomChatCacheSanitizeText(char* destination, size_t destination_size, const char* source);
bool ShroomChatCacheMessagesDuplicate(const ChatMessage* left, const ChatMessage* right);
size_t ShroomChatCacheLoadContext(const char* path, const ShroomChatCacheKey* key, uint32_t now_sec,
                                  ChatMessage* messages, size_t capacity);
bool ShroomChatCacheStoreMessage(const char* path, const ShroomChatCacheKey* key,
                                 const ChatMessage* message, uint32_t now_sec);

#endif
