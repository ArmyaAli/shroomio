#include "chat_cache.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#define SHROOM_CHAT_CACHE_HEADER "SHROOM_CHAT_CACHE_V1"
#define SHROOM_CHAT_CACHE_LINE_BYTES 768u

typedef struct ShroomChatCacheContext {
  ShroomChatCacheKey key;
  size_t message_count;
  ChatMessage messages[SHROOM_CHAT_CACHE_MAX_MESSAGES];
} ShroomChatCacheContext;

typedef struct ShroomChatCacheData {
  size_t context_count;
  ShroomChatCacheContext contexts[SHROOM_CHAT_CACHE_MAX_CONTEXTS];
} ShroomChatCacheData;

static bool IsTimestampRetained(uint32_t timestamp, uint32_t now_sec) {
  if ((timestamp == 0u) || ((timestamp > now_sec) && (timestamp - now_sec > 300u))) {
    return false;
  }
  return now_sec <= timestamp || (now_sec - timestamp) <= SHROOM_CHAT_CACHE_RETENTION_SECONDS;
}

void ShroomChatCacheSanitizeText(char* destination, size_t destination_size, const char* source) {
  char source_copy[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
  size_t written = 0u;
  bool pending_space = false;

  if ((destination == NULL) || (destination_size == 0u)) {
    return;
  }
  if (source == NULL) {
    destination[0] = '\0';
    return;
  }
  snprintf(source_copy, sizeof(source_copy), "%s", source);
  source = source_copy;
  destination[0] = '\0';
  while ((*source != '\0') && (written + 1u < destination_size)) {
    const unsigned char character = (unsigned char)*source++;
    if (isspace(character) || (character < 0x20u) || (character == 0x7fu)) {
      pending_space = written > 0u;
      continue;
    }
    if (character > 0x7eu) {
      continue;
    }
    if (pending_space && (written + 1u < destination_size)) {
      destination[written++] = ' ';
    }
    pending_space = false;
    destination[written++] = (char)character;
  }
  destination[written] = '\0';
}

bool ShroomChatCacheMessagesDuplicate(const ChatMessage* left, const ChatMessage* right) {
  uint32_t difference;

  if ((left == NULL) || (right == NULL) || (left->sender_id != right->sender_id) ||
      (strcmp(left->sender_name, right->sender_name) != 0) ||
      (strcmp(left->message, right->message) != 0)) {
    return false;
  }
  difference = left->timestamp_sec > right->timestamp_sec
                   ? left->timestamp_sec - right->timestamp_sec
                   : right->timestamp_sec - left->timestamp_sec;
  return difference <= SHROOM_CHAT_CACHE_DUPLICATE_WINDOW_SECONDS;
}

static bool NormalizeKey(ShroomChatCacheKey* destination, const ShroomChatCacheKey* source) {
  size_t written = 0u;

  if ((destination == NULL) || (source == NULL) || (source->port == 0u) ||
      (source->lobby_id == 0u)) {
    return false;
  }
  *destination = (ShroomChatCacheKey){.port = source->port, .lobby_id = source->lobby_id};
  for (const char* cursor = source->host;
       (*cursor != '\0') && (written + 1u < sizeof(destination->host)); ++cursor) {
    const unsigned char character = (unsigned char)*cursor;
    if (!isalnum(character) && (character != '.') && (character != '-') && (character != ':')) {
      return false;
    }
    destination->host[written++] = (char)tolower(character);
  }
  destination->host[written] = '\0';
  return written > 0u;
}

static bool KeysEqual(const ShroomChatCacheKey* left, const ShroomChatCacheKey* right) {
  return (left->port == right->port) && (left->lobby_id == right->lobby_id) &&
         (strcmp(left->host, right->host) == 0);
}

static bool HexEncode(const char* source, char* destination, size_t destination_size) {
  static const char digits[] = "0123456789abcdef";
  size_t source_length = strlen(source);

  if ((source_length * 2u) + 1u > destination_size) {
    return false;
  }
  for (size_t index = 0u; index < source_length; ++index) {
    const unsigned char character = (unsigned char)source[index];
    destination[index * 2u] = digits[character >> 4u];
    destination[index * 2u + 1u] = digits[character & 0x0fu];
  }
  destination[source_length * 2u] = '\0';
  return true;
}

static int HexValue(char character) {
  if ((character >= '0') && (character <= '9')) {
    return character - '0';
  }
  if ((character >= 'a') && (character <= 'f')) {
    return character - 'a' + 10;
  }
  if ((character >= 'A') && (character <= 'F')) {
    return character - 'A' + 10;
  }
  return -1;
}

static bool HexDecode(const char* source, char* destination, size_t destination_size) {
  const size_t length = strlen(source);

  if (((length % 2u) != 0u) || ((length / 2u) + 1u > destination_size)) {
    return false;
  }
  for (size_t index = 0u; index < length; index += 2u) {
    const int high = HexValue(source[index]);
    const int low = HexValue(source[index + 1u]);
    if ((high < 0) || (low < 0)) {
      return false;
    }
    destination[index / 2u] = (char)((high << 4u) | low);
  }
  destination[length / 2u] = '\0';
  return true;
}

static ShroomChatCacheContext* FindContext(ShroomChatCacheData* cache,
                                           const ShroomChatCacheKey* key) {
  for (size_t index = 0u; index < cache->context_count; ++index) {
    if (KeysEqual(&cache->contexts[index].key, key)) {
      return &cache->contexts[index];
    }
  }
  return NULL;
}

static void AppendMessage(ShroomChatCacheContext* context, const ChatMessage* message) {
  if (context->message_count == SHROOM_CHAT_CACHE_MAX_MESSAGES) {
    memmove(&context->messages[0], &context->messages[1],
            (SHROOM_CHAT_CACHE_MAX_MESSAGES - 1u) * sizeof(context->messages[0]));
    --context->message_count;
  }
  context->messages[context->message_count++] = *message;
}

static bool ParseUnsigned(const char* text, unsigned long maximum, unsigned long* value) {
  char* end = NULL;
  unsigned long parsed;

  if ((text == NULL) || (*text == '\0')) {
    return false;
  }
  parsed = strtoul(text, &end, 10);
  if ((end == text) || (*end != '\0') || (parsed > maximum)) {
    return false;
  }
  *value = parsed;
  return true;
}

static bool ParseRecord(char* line, ShroomChatCacheKey* key, ChatMessage* message) {
  char* fields[7];
  char* cursor = line;
  unsigned long value;

  for (size_t index = 0u; index < 7u; ++index) {
    fields[index] = cursor;
    if (index == 6u) {
      break;
    }
    cursor = strchr(cursor, '|');
    if (cursor == NULL) {
      return false;
    }
    *cursor++ = '\0';
  }
  if (strchr(fields[6], '|') != NULL || !HexDecode(fields[0], key->host, sizeof(key->host)) ||
      !ParseUnsigned(fields[1], UINT16_MAX, &value)) {
    return false;
  }
  key->port = (uint16_t)value;
  if (!ParseUnsigned(fields[2], UINT32_MAX, &value)) {
    return false;
  }
  key->lobby_id = (uint32_t)value;
  if (!ParseUnsigned(fields[3], UINT32_MAX, &value)) {
    return false;
  }
  message->sender_id = (uint32_t)value;
  if (!ParseUnsigned(fields[4], UINT32_MAX, &value)) {
    return false;
  }
  message->timestamp_sec = (uint32_t)value;
  return HexDecode(fields[5], message->sender_name, sizeof(message->sender_name)) &&
         HexDecode(fields[6], message->message, sizeof(message->message));
}

static bool LoadCache(const char* path, uint32_t now_sec, ShroomChatCacheData* cache) {
#ifdef _WIN32
  struct _stat file_status;
#else
  struct stat file_status;
#endif
  int descriptor;
  FILE* file;
  char line[SHROOM_CHAT_CACHE_LINE_BYTES];

  *cache = (ShroomChatCacheData){0};
  if (path == NULL) {
    return false;
  }
#ifdef _WIN32
  descriptor = _open(path, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
#else
  descriptor = open(path, O_RDONLY);
#endif
  if (descriptor < 0) {
    return errno == ENOENT;
  }
#ifdef _WIN32
  if (_fstat(descriptor, &file_status) != 0) {
    _close(descriptor);
#else
  if (fstat(descriptor, &file_status) != 0) {
    close(descriptor);
#endif
    return false;
  }
  if ((file_status.st_size <= 0) ||
      ((size_t)file_status.st_size > SHROOM_CHAT_CACHE_MAX_FILE_BYTES)) {
#ifdef _WIN32
    _close(descriptor);
#else
    close(descriptor);
#endif
    return false;
  }
#ifdef _WIN32
  file = _fdopen(descriptor, "r");
#else
  file = fdopen(descriptor, "r");
#endif
  if (file == NULL) {
#ifdef _WIN32
    _close(descriptor);
#else
    close(descriptor);
#endif
    return false;
  }
  if ((fgets(line, sizeof(line), file) == NULL) ||
      (strcmp(line, SHROOM_CHAT_CACHE_HEADER "\n") != 0)) {
    fclose(file);
    return false;
  }
  while (fgets(line, sizeof(line), file) != NULL) {
    ShroomChatCacheKey raw_key;
    ShroomChatCacheKey key;
    ChatMessage message = {0};
    ShroomChatCacheContext* context;
    const size_t line_length = strlen(line);

    if ((line_length == 0u) || (line[line_length - 1u] != '\n')) {
      fclose(file);
      *cache = (ShroomChatCacheData){0};
      return false;
    }
    line[line_length - 1u] = '\0';
    if (!ParseRecord(line, &raw_key, &message) || !NormalizeKey(&key, &raw_key) ||
        (message.timestamp_sec == 0u) ||
        ((message.timestamp_sec > now_sec) && (message.timestamp_sec - now_sec > 300u))) {
      fclose(file);
      *cache = (ShroomChatCacheData){0};
      return false;
    }
    if (!IsTimestampRetained(message.timestamp_sec, now_sec)) {
      continue;
    }
    ShroomChatCacheSanitizeText(message.sender_name, sizeof(message.sender_name),
                                message.sender_name);
    ShroomChatCacheSanitizeText(message.message, sizeof(message.message), message.message);
    if ((message.sender_name[0] == '\0') || (message.message[0] == '\0')) {
      fclose(file);
      *cache = (ShroomChatCacheData){0};
      return false;
    }
    context = FindContext(cache, &key);
    if (context == NULL) {
      if (cache->context_count == SHROOM_CHAT_CACHE_MAX_CONTEXTS) {
        continue;
      }
      context = &cache->contexts[cache->context_count++];
      *context = (ShroomChatCacheContext){.key = key};
    }
    if ((context->message_count == 0u) ||
        !ShroomChatCacheMessagesDuplicate(&context->messages[context->message_count - 1u],
                                          &message)) {
      AppendMessage(context, &message);
    }
  }
  fclose(file);
  return true;
}

static bool SaveCache(const char* path, const ShroomChatCacheData* cache) {
  char temporary_path[512];
  int path_length;
  int descriptor;
  FILE* file;
  bool write_failed = false;

  if (path == NULL) {
    return false;
  }
  path_length = snprintf(temporary_path, sizeof(temporary_path), "%s.tmp", path);
  if ((path_length < 0) || ((size_t)path_length >= sizeof(temporary_path))) {
    return false;
  }
#ifdef _WIN32
  descriptor = _open(temporary_path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY | _O_NOINHERIT,
                     _S_IREAD | _S_IWRITE);
#else
  descriptor = open(temporary_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
#endif
  if (descriptor < 0) {
    return false;
  }
  file =
#ifdef _WIN32
      _fdopen(descriptor, "w");
#else
      fdopen(descriptor, "w");
#endif
  if (file == NULL) {
#ifdef _WIN32
    _close(descriptor);
    _unlink(temporary_path);
#else
    close(descriptor);
    unlink(temporary_path);
#endif
    return false;
  }
  fprintf(file, "%s\n", SHROOM_CHAT_CACHE_HEADER);
  for (size_t context_index = 0u; context_index < cache->context_count; ++context_index) {
    const ShroomChatCacheContext* context = &cache->contexts[context_index];
    char host_hex[sizeof(context->key.host) * 2u];
    if (!HexEncode(context->key.host, host_hex, sizeof(host_hex))) {
      fclose(file);
      remove(temporary_path);
      return false;
    }
    for (size_t message_index = 0u; message_index < context->message_count; ++message_index) {
      const ChatMessage* message = &context->messages[message_index];
      char name_hex[sizeof(message->sender_name) * 2u];
      char message_hex[sizeof(message->message) * 2u];
      if (!HexEncode(message->sender_name, name_hex, sizeof(name_hex)) ||
          !HexEncode(message->message, message_hex, sizeof(message_hex)) ||
          fprintf(file, "%s|%u|%u|%u|%u|%s|%s\n", host_hex, context->key.port,
                  context->key.lobby_id, message->sender_id, message->timestamp_sec, name_hex,
                  message_hex) < 0) {
        fclose(file);
        remove(temporary_path);
        return false;
      }
    }
  }
  write_failed = fflush(file) != 0;
  if (!write_failed) {
#ifdef _WIN32
    write_failed = _commit(descriptor) != 0;
#else
    write_failed = fsync(descriptor) != 0;
#endif
  }
  if (fclose(file) != 0) {
    write_failed = true;
  }
  if (write_failed) {
    remove(temporary_path);
    return false;
  }
#ifdef _WIN32
  if (!MoveFileExA(temporary_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
  if (rename(temporary_path, path) != 0) {
#endif
    remove(temporary_path);
    return false;
  }
  return true;
}

size_t ShroomChatCacheLoadContext(const char* path, const ShroomChatCacheKey* raw_key,
                                  uint32_t now_sec, ChatMessage* messages, size_t capacity) {
  ShroomChatCacheData cache;
  ShroomChatCacheKey key;
  ShroomChatCacheContext* context;
  size_t count;

  if ((messages == NULL) || (capacity == 0u) || !NormalizeKey(&key, raw_key) ||
      !LoadCache(path, now_sec, &cache)) {
    return 0u;
  }
  context = FindContext(&cache, &key);
  if (context == NULL) {
    return 0u;
  }
  count = context->message_count < capacity ? context->message_count : capacity;
  memcpy(messages, &context->messages[context->message_count - count], count * sizeof(messages[0]));
  return count;
}

bool ShroomChatCacheStoreMessage(const char* path, const ShroomChatCacheKey* raw_key,
                                 const ChatMessage* raw_message, uint32_t now_sec) {
  ShroomChatCacheData cache;
  ShroomChatCacheKey key;
  ShroomChatCacheContext* context;
  ChatMessage message;

  if ((raw_message == NULL) || !NormalizeKey(&key, raw_key) ||
      !IsTimestampRetained(raw_message->timestamp_sec, now_sec)) {
    return false;
  }
  message = *raw_message;
  ShroomChatCacheSanitizeText(message.sender_name, sizeof(message.sender_name),
                              raw_message->sender_name);
  ShroomChatCacheSanitizeText(message.message, sizeof(message.message), raw_message->message);
  if ((message.sender_name[0] == '\0') || (message.message[0] == '\0')) {
    return false;
  }
  if (!LoadCache(path, now_sec, &cache)) {
    cache = (ShroomChatCacheData){0};
  }
  context = FindContext(&cache, &key);
  if (context == NULL) {
    if (cache.context_count == SHROOM_CHAT_CACHE_MAX_CONTEXTS) {
      memmove(&cache.contexts[0], &cache.contexts[1],
              (SHROOM_CHAT_CACHE_MAX_CONTEXTS - 1u) * sizeof(cache.contexts[0]));
      --cache.context_count;
    }
    context = &cache.contexts[cache.context_count++];
    *context = (ShroomChatCacheContext){.key = key};
  }
  for (size_t index = 0u; index < context->message_count; ++index) {
    if (ShroomChatCacheMessagesDuplicate(&context->messages[index], &message)) {
      return true;
    }
  }
  AppendMessage(context, &message);
  return SaveCache(path, &cache);
}
