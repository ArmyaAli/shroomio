#include "settings_session.h"

#include <stddef.h>

void ShroomSettingsSessionInit(ShroomSettingsSession* session, const ClientSettings* current) {
  if ((session == NULL) || (current == NULL)) {
    return;
  }
  session->original = *current;
  session->pending = *current;
  session->dirty = false;
}

void ShroomSettingsSessionMarkDirty(ShroomSettingsSession* session) {
  if (session != NULL) {
    session->dirty = true;
  }
}

void ShroomSettingsSessionRestoreDefaults(ShroomSettingsSession* session) {
  if (session == NULL) {
    return;
  }
  ClientSettingsSetDefaults(&session->pending);
  session->dirty = true;
}

void ShroomSettingsSessionCommit(ShroomSettingsSession* session) {
  if (session == NULL) {
    return;
  }
  session->original = session->pending;
  session->dirty = false;
}

void ShroomSettingsSessionDiscard(ShroomSettingsSession* session) {
  if (session == NULL) {
    return;
  }
  session->pending = session->original;
  session->dirty = false;
}
