#ifndef SHROOM_PLAYER_IDENTITY_H
#define SHROOM_PLAYER_IDENTITY_H

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

static inline void ShroomSanitizePlayerName(char destination[SHROOM_MAX_NAME_LENGTH],
                                            const char* source) {
  size_t written = 0u;
  bool pending_space = false;

  if (destination == NULL) {
    return;
  }
  destination[0] = '\0';
  if (source == NULL) {
    return;
  }
  while ((*source != '\0') && (written + 1u < SHROOM_MAX_NAME_LENGTH)) {
    const unsigned char character = (unsigned char)*source++;
    if (isspace(character)) {
      pending_space = written > 0u;
      continue;
    }
    if (!isalnum(character) && (character != '_') && (character != '-') && (character != '.')) {
      continue;
    }
    if (pending_space && (written + 1u < SHROOM_MAX_NAME_LENGTH)) {
      destination[written++] = ' ';
    }
    pending_space = false;
    destination[written++] = (char)character;
  }
  destination[written] = '\0';
}

#endif
