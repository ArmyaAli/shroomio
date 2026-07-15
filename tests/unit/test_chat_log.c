#include "server/chat_log.h"

#include "unity.h"

#include <stdio.h>
#include <string.h>

typedef struct CapturedLog {
  ShroomChatLogOutcome outcome;
  char line[SHROOM_CHAT_LOG_LINE_LENGTH];
  unsigned int calls;
} CapturedLog;

static void Capture(void* context, ShroomChatLogOutcome outcome, const char* line) {
  CapturedLog* captured = (CapturedLog*)context;

  captured->outcome = outcome;
  snprintf(captured->line, sizeof(captured->line), "%s", line);
  captured->calls += 1u;
}

void setUp(void) {}
void tearDown(void) {}

void test_accepted_chat_log_contains_only_operational_metadata(void) {
  const char* sender_secret = "PRIVATE_SENDER_560";
  const char* message_secret = "PRIVATE_MESSAGE_560";
  const ShroomChatLogEvent event = {
      .lobby_id = 42u,
      .message_id = 9001u,
      .byte_length = strlen(message_secret),
      .outcome = SHROOM_CHAT_LOG_ACCEPTED,
  };
  CapturedLog captured = {0};

  TEST_ASSERT_TRUE(ShroomChatLogEmit(&event, Capture, &captured));
  TEST_ASSERT_EQUAL_UINT(1u, captured.calls);
  TEST_ASSERT_EQUAL(SHROOM_CHAT_LOG_ACCEPTED, captured.outcome);
  TEST_ASSERT_EQUAL_STRING("chat lobby_id=42 message_id=9001 bytes=19 outcome=accepted",
                           captured.line);
  TEST_ASSERT_NULL(strstr(captured.line, sender_secret));
  TEST_ASSERT_NULL(strstr(captured.line, message_secret));
}

void test_rejected_chat_logs_preserve_outcome_without_private_fields(void) {
  const char* sender_secret = "REJECTED_SENDER_560";
  const char* message_secret = "REJECTED_MESSAGE_560";
  const ShroomChatLogOutcome outcomes[] = {
      SHROOM_CHAT_LOG_REJECTED_INVALID,
      SHROOM_CHAT_LOG_REJECTED_RATE_LIMITED,
  };

  for (size_t index = 0u; index < sizeof(outcomes) / sizeof(outcomes[0]); ++index) {
    const ShroomChatLogEvent event = {
        .lobby_id = 7u,
        .message_id = 100u + index,
        .byte_length = strlen(message_secret),
        .outcome = outcomes[index],
    };
    CapturedLog captured = {0};

    TEST_ASSERT_TRUE(ShroomChatLogEmit(&event, Capture, &captured));
    TEST_ASSERT_EQUAL(outcomes[index], captured.outcome);
    TEST_ASSERT_NOT_NULL(strstr(captured.line, "lobby_id=7"));
    TEST_ASSERT_NOT_NULL(strstr(captured.line, "message_id="));
    TEST_ASSERT_NOT_NULL(strstr(captured.line, "bytes=20"));
    TEST_ASSERT_NOT_NULL(strstr(captured.line, "outcome=rejected_"));
    TEST_ASSERT_NULL(strstr(captured.line, sender_secret));
    TEST_ASSERT_NULL(strstr(captured.line, message_secret));
    TEST_ASSERT_NULL(strstr(captured.line, "name="));
    TEST_ASSERT_NULL(strstr(captured.line, "msg="));
  }
}

void test_chat_log_rejects_missing_event_or_sink(void) {
  const ShroomChatLogEvent event = {0};
  CapturedLog captured = {0};

  TEST_ASSERT_FALSE(ShroomChatLogEmit(NULL, Capture, &captured));
  TEST_ASSERT_FALSE(ShroomChatLogEmit(&event, NULL, &captured));
  TEST_ASSERT_EQUAL_UINT(0u, captured.calls);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_accepted_chat_log_contains_only_operational_metadata);
  RUN_TEST(test_rejected_chat_logs_preserve_outcome_without_private_fields);
  RUN_TEST(test_chat_log_rejects_missing_event_or_sink);
  return UNITY_END();
}
