#include "settings_deferred.h"

#include <string.h>

static bool WritePending(ShroomSettingsDeferred* state) {
  if ((state == NULL) || !state->dirty || (state->writer == NULL)) {
    return (state == NULL) || !state->dirty;
  }
  if (state->writer(&state->pending, state->context)) {
    state->dirty = false;
    state->retry_delay = SHROOM_SETTINGS_RETRY_INITIAL_SECONDS;
    return true;
  }
  state->warning_pending = true;
  state->due_time += state->retry_delay;
  if (state->retry_delay < SHROOM_SETTINGS_RETRY_MAX_SECONDS) {
    state->retry_delay *= 2.0;
    if (state->retry_delay > SHROOM_SETTINGS_RETRY_MAX_SECONDS) {
      state->retry_delay = SHROOM_SETTINGS_RETRY_MAX_SECONDS;
    }
  }
  return false;
}

void ShroomSettingsDeferredInit(ShroomSettingsDeferred* state,
                                ShroomSettingsDeferredWriter writer, void* context) {
  if (state == NULL) {
    return;
  }
  *state = (ShroomSettingsDeferred){
      .writer = writer, .context = context, .retry_delay = SHROOM_SETTINGS_RETRY_INITIAL_SECONDS};
}

void ShroomSettingsDeferredMarkDirty(ShroomSettingsDeferred* state, const ClientSettings* settings,
                                     double now) {
  if ((state == NULL) || (settings == NULL)) {
    return;
  }
  state->pending = *settings;
  ClientSettingsValidate(&state->pending);
  state->dirty = true;
  state->due_time = now + SHROOM_SETTINGS_DEBOUNCE_SECONDS;
  state->retry_delay = SHROOM_SETTINGS_RETRY_INITIAL_SECONDS;
}

void ShroomSettingsDeferredUpdate(ShroomSettingsDeferred* state, double now) {
  if ((state != NULL) && state->dirty && (now >= state->due_time)) {
    (void)WritePending(state);
  }
}

bool ShroomSettingsDeferredFlush(ShroomSettingsDeferred* state, double now) {
  if ((state == NULL) || !state->dirty) {
    return true;
  }
  state->due_time = now;
  return WritePending(state);
}

bool ShroomSettingsDeferredConsumeWarning(ShroomSettingsDeferred* state) {
  const bool pending = state != NULL && state->warning_pending;
  if (pending) {
    state->warning_pending = false;
  }
  return pending;
}
