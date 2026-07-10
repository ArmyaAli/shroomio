#ifndef SHROOM_SETTINGS_SESSION_H
#define SHROOM_SETTINGS_SESSION_H

#include "client_settings.h"

#include <stdbool.h>

typedef struct ShroomSettingsSession {
  ClientSettings original;
  ClientSettings pending;
  bool dirty;
} ShroomSettingsSession;

void ShroomSettingsSessionInit(ShroomSettingsSession* session, const ClientSettings* current);
void ShroomSettingsSessionMarkDirty(ShroomSettingsSession* session);
void ShroomSettingsSessionRestoreDefaults(ShroomSettingsSession* session);
void ShroomSettingsSessionCommit(ShroomSettingsSession* session);
void ShroomSettingsSessionDiscard(ShroomSettingsSession* session);

#endif
