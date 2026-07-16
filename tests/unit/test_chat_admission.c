#include "unity.h"

#include "shared/chat_admission.h"

void setUp(void) {}
void tearDown(void) {}

static void test_whitespace_is_collapsed_and_trimmed(void) {
  char output[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];

  TEST_ASSERT_TRUE(ShroomChatCanonicalizeMessage(" \tHello\n  world \r", 32u, output,
                                                 sizeof(output)));
  TEST_ASSERT_EQUAL_STRING("Hello world", output);
}

static void test_unsupported_bytes_are_removed(void) {
  char output[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
  const char input[] = "high\x80" "byte\x7f";

  TEST_ASSERT_TRUE(ShroomChatCanonicalizeMessage(input, sizeof(input), output, sizeof(output)));
  TEST_ASSERT_EQUAL_STRING("highbyte", output);
}

static void test_empty_or_unterminated_messages_are_rejected(void) {
  char output[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];
  const char unterminated[] = "unterminated";

  TEST_ASSERT_FALSE(ShroomChatCanonicalizeMessage(" \t\n", 4u, output, sizeof(output)));
  TEST_ASSERT_FALSE(ShroomChatCanonicalizeMessage(unterminated, sizeof(unterminated) - 1u, output,
                                                  sizeof(output)));
}

static void test_canonical_validation_rejects_rewritten_content(void) {
  TEST_ASSERT_TRUE(ShroomChatIsCanonicalMessage("Hello world", 12u));
  TEST_ASSERT_FALSE(ShroomChatIsCanonicalMessage(" Hello", 7u));
  TEST_ASSERT_FALSE(ShroomChatIsCanonicalMessage("Hello  world", 14u));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_whitespace_is_collapsed_and_trimmed);
  RUN_TEST(test_unsupported_bytes_are_removed);
  RUN_TEST(test_empty_or_unterminated_messages_are_rejected);
  RUN_TEST(test_canonical_validation_rejects_rewritten_content);
  return UNITY_END();
}
