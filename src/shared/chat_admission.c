#include "chat_admission.h"

static bool IsAsciiWhitespace(unsigned char character) {
  return (character == ' ') || (character == '\t') || (character == '\n') ||
         (character == '\r') || (character == '\f') || (character == '\v');
}

bool ShroomChatCanonicalizeMessage(const char* input, size_t input_capacity, char* output,
                                   size_t output_capacity) {
  size_t input_length = 0u;
  size_t output_length = 0u;
  bool pending_space = false;

  if ((input == NULL) || (output == NULL) || (output_capacity == 0u)) {
    return false;
  }
  while ((input_length < input_capacity) && (input[input_length] != '\0')) {
    input_length += 1u;
  }
  if ((input_length == input_capacity) || (input_length > SHROOM_CHAT_MAX_MESSAGE_LENGTH)) {
    output[0] = '\0';
    return false;
  }

  for (size_t index = 0u; index < input_length; ++index) {
    const unsigned char character = (unsigned char)input[index];
    if (character > 0x7eu) {
      continue;
    }
    if (IsAsciiWhitespace(character)) {
      if (output_length > 0u) {
        pending_space = true;
      }
      continue;
    }
    if (pending_space) {
      if (output_length + 1u >= output_capacity) {
        output[0] = '\0';
        return false;
      }
      output[output_length++] = ' ';
      pending_space = false;
    }
    if (output_length + 1u >= output_capacity) {
      output[0] = '\0';
      return false;
    }
    output[output_length++] = (char)character;
  }
  output[output_length] = '\0';
  return output_length > 0u;
}

bool ShroomChatIsCanonicalMessage(const char* message, size_t capacity) {
  char canonical[SHROOM_CHAT_MAX_MESSAGE_LENGTH + 1u];

  if (!ShroomChatCanonicalizeMessage(message, capacity, canonical, sizeof(canonical))) {
    return false;
  }
  for (size_t index = 0u; canonical[index] != '\0'; ++index) {
    if (canonical[index] != message[index]) {
      return false;
    }
  }
  return canonical[0] != '\0';
}
