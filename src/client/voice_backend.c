#include "voice_backend.h"

#include <miniaudio.h>

#include <string.h>

typedef struct ShroomVoiceMiniaudioBackend {
  ma_device device;
  ShroomVoiceAudioProcessFn process;
  ShroomVoiceDeviceLostFn device_lost;
  void* callback_context;
  bool initialized;
  bool started;
} ShroomVoiceMiniaudioBackend;

static ShroomVoiceMiniaudioBackend g_voice_backend;

static void ProcessDevice(ma_device* device, void* output, const void* input,
                          ma_uint32 frame_count) {
  ShroomVoiceMiniaudioBackend* backend = device != NULL ? device->pUserData : NULL;

  if ((backend != NULL) && (backend->process != NULL)) {
    backend->process(backend->callback_context, output, input, frame_count);
  } else if (output != NULL) {
    memset(output, 0, (size_t)frame_count * sizeof(float));
  }
}

static void HandleNotification(const ma_device_notification* notification) {
  ShroomVoiceMiniaudioBackend* backend;

  if ((notification == NULL) || (notification->pDevice == NULL)) {
    return;
  }
  backend = notification->pDevice->pUserData;
  if ((backend != NULL) &&
      ((notification->type == ma_device_notification_type_rerouted) ||
       (notification->type == ma_device_notification_type_interruption_began) ||
       (notification->type == ma_device_notification_type_unlocked))) {
    if (backend->device_lost != NULL) {
      backend->device_lost(backend->callback_context);
    }
  }
}

static bool Start(void* context, ShroomVoiceAudioProcessFn process,
                  ShroomVoiceDeviceLostFn device_lost, void* callback_context) {
  ShroomVoiceMiniaudioBackend* backend = context;
  ma_device_config config;

  if ((backend == NULL) || (process == NULL)) {
    return false;
  }
  if (backend->initialized) {
    return backend->started;
  }
  *backend = (ShroomVoiceMiniaudioBackend){
      .process = process,
      .device_lost = device_lost,
      .callback_context = callback_context,
  };
  config = ma_device_config_init(ma_device_type_duplex);
  config.capture.format = ma_format_f32;
  config.capture.channels = SHROOM_VOICE_CHANNEL_COUNT;
  config.playback.format = ma_format_f32;
  config.playback.channels = SHROOM_VOICE_CHANNEL_COUNT;
  config.sampleRate = SHROOM_VOICE_SAMPLE_RATE;
  config.periodSizeInFrames = SHROOM_VOICE_FRAME_SAMPLES;
  config.periods = 3u;
  config.dataCallback = ProcessDevice;
  config.notificationCallback = HandleNotification;
  config.pUserData = backend;

  if (ma_device_init(NULL, &config, &backend->device) != MA_SUCCESS) {
    *backend = (ShroomVoiceMiniaudioBackend){0};
    return false;
  }
  backend->initialized = true;
  if (ma_device_start(&backend->device) != MA_SUCCESS) {
    ma_device_uninit(&backend->device);
    *backend = (ShroomVoiceMiniaudioBackend){0};
    return false;
  }
  backend->started = true;
  return true;
}

static void Stop(void* context) {
  ShroomVoiceMiniaudioBackend* backend = context;

  if ((backend == NULL) || !backend->initialized) {
    return;
  }
  if (backend->started) {
    (void)ma_device_stop(&backend->device);
  }
  ma_device_uninit(&backend->device);
  *backend = (ShroomVoiceMiniaudioBackend){0};
}

static bool Healthy(void* context) {
  const ShroomVoiceMiniaudioBackend* backend = context;
  return (backend != NULL) && backend->initialized && backend->started &&
         ma_device_is_started((ma_device*)&backend->device);
}

const ShroomVoiceBackend* ShroomVoiceProductionBackend(void) {
  static const ShroomVoiceBackend backend = {
      .context = &g_voice_backend,
      .start = Start,
      .stop = Stop,
      .healthy = Healthy,
      .now_ms = NULL,
  };
  return &backend;
}
