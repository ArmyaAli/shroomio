#ifndef SHROOM_CHAT_ADMISSION_H
#define SHROOM_CHAT_ADMISSION_H

#include <stdbool.h>
#include <stddef.h>

#include "protocol.h"

bool ShroomChatCanonicalizeMessage(const char* input, size_t input_capacity, char* output,
                                   size_t output_capacity);
bool ShroomChatIsCanonicalMessage(const char* message, size_t capacity);

#endif
