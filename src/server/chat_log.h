#ifndef SHROOM_CHAT_LOG_H
#define SHROOM_CHAT_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SHROOM_CHAT_LOG_LINE_LENGTH 160u

typedef enum ShroomChatLogOutcome {
  SHROOM_CHAT_LOG_ACCEPTED = 0,
  SHROOM_CHAT_LOG_REJECTED_INVALID,
  SHROOM_CHAT_LOG_REJECTED_RATE_LIMITED,
} ShroomChatLogOutcome;

typedef struct ShroomChatLogEvent {
  uint32_t lobby_id;
  uint64_t message_id;
  size_t byte_length;
  ShroomChatLogOutcome outcome;
} ShroomChatLogEvent;

typedef void (*ShroomChatLogSink)(void* context, ShroomChatLogOutcome outcome, const char* line);

const char* ShroomChatLogOutcomeLabel(ShroomChatLogOutcome outcome);
bool ShroomChatLogEmit(const ShroomChatLogEvent* event, ShroomChatLogSink sink, void* context);

#endif
