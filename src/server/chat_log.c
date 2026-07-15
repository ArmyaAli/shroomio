#include "chat_log.h"

#include <inttypes.h>
#include <stdio.h>

const char* ShroomChatLogOutcomeLabel(ShroomChatLogOutcome outcome) {
  switch (outcome) {
  case SHROOM_CHAT_LOG_ACCEPTED:
    return "accepted";
  case SHROOM_CHAT_LOG_REJECTED_INVALID:
    return "rejected_invalid";
  case SHROOM_CHAT_LOG_REJECTED_RATE_LIMITED:
    return "rejected_rate_limited";
  default:
    return "rejected_unknown";
  }
}

bool ShroomChatLogEmit(const ShroomChatLogEvent* event, ShroomChatLogSink sink, void* context) {
  char line[SHROOM_CHAT_LOG_LINE_LENGTH];
  int written;

  if ((event == NULL) || (sink == NULL)) {
    return false;
  }
  written = snprintf(line, sizeof(line),
                     "chat lobby_id=%" PRIu32 " message_id=%" PRIu64 " bytes=%zu outcome=%s",
                     event->lobby_id, event->message_id, event->byte_length,
                     ShroomChatLogOutcomeLabel(event->outcome));
  if ((written < 0) || ((size_t)written >= sizeof(line))) {
    return false;
  }
  sink(context, event->outcome, line);
  return true;
}
