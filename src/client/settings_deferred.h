#ifndef SHROOM_SETTINGS_DEFERRED_H
#define SHROOM_SETTINGS_DEFERRED_H

#include <stdbool.h>

#include "client_settings.h"

#define SHROOM_SETTINGS_DEBOUNCE_SECONDS 0.5
#define SHROOM_SETTINGS_RETRY_INITIAL_SECONDS 1.0
#define SHROOM_SETTINGS_RETRY_MAX_SECONDS 30.0

typedef bool (*ShroomSettingsDeferredWriter)(const ClientSettings* settings, void* context);

typedef struct ShroomSettingsDeferred {
  ClientSettings pending;
  ShroomSettingsDeferredWriter writer;
  void* context;
  double due_time;
  double retry_delay;
  bool dirty;
  bool warning_pending;
} ShroomSettingsDeferred;

void ShroomSettingsDeferredInit(ShroomSettingsDeferred* state,
                                ShroomSettingsDeferredWriter writer, void* context);
void ShroomSettingsDeferredMarkDirty(ShroomSettingsDeferred* state, const ClientSettings* settings,
                                     double now);
void ShroomSettingsDeferredUpdate(ShroomSettingsDeferred* state, double now);
bool ShroomSettingsDeferredFlush(ShroomSettingsDeferred* state, double now);
bool ShroomSettingsDeferredConsumeWarning(ShroomSettingsDeferred* state);

#endif
