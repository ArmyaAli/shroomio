#include "voice_backend.h"

#include <miniaudio.h>

#include <stdio.h>
#include <string.h>

typedef struct ShroomVoiceMiniaudioBackend {
  ma_context context;
  ma_device device;
  ShroomVoiceAudioProcessFn process;
  ShroomVoiceDeviceLostFn device_lost;
  void* callback_context;
  bool initialized;
  bool started;
  bool context_initialized;
  char selected_capture_device[SHROOM_VOICE_DEVICE_NAME_LENGTH];
} ShroomVoiceMiniaudioBackend;

static ShroomVoiceMiniaudioBackend g_voice_backend;

static bool EnsureContext(ShroomVoiceMiniaudioBackend* backend) {
  if (backend->context_initialized) {
    return true;
  }
  if (ma_context_init(NULL, 0u, NULL, &backend->context) != MA_SUCCESS) {
    return false;
  }
  backend->context_initialized = true;
  return true;
}

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
  ma_device_info* capture_devices = NULL;
  ma_uint32 capture_device_count = 0u;

  if ((backend == NULL) || (process == NULL)) {
    return false;
  }
  if (backend->initialized) {
    return backend->started;
  }
  if (!EnsureContext(backend)) {
    return false;
  }
  backend->process = process;
  backend->device_lost = device_lost;
  backend->callback_context = callback_context;
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

  if ((backend->selected_capture_device[0] != '\0') &&
      (ma_context_get_devices(&backend->context, NULL, NULL, &capture_devices,
                              &capture_device_count) == MA_SUCCESS)) {
    for (ma_uint32 index = 0u; index < capture_device_count; ++index) {
      if (strcmp(capture_devices[index].name, backend->selected_capture_device) == 0) {
        config.capture.pDeviceID = &capture_devices[index].id;
        break;
      }
    }
  }

  if (ma_device_init(&backend->context, &config, &backend->device) != MA_SUCCESS) {
    return false;
  }
  backend->initialized = true;
  if (ma_device_start(&backend->device) != MA_SUCCESS) {
    ma_device_uninit(&backend->device);
    backend->initialized = false;
    return false;
  }
  backend->started = true;
  return true;
}

static void Stop(void* context) {
  ShroomVoiceMiniaudioBackend* backend = context;
  char selected_capture_device[SHROOM_VOICE_DEVICE_NAME_LENGTH];

  if (backend == NULL) {
    return;
  }
  snprintf(selected_capture_device, sizeof(selected_capture_device), "%s",
           backend->selected_capture_device);
  if (backend->initialized && backend->started) {
    (void)ma_device_stop(&backend->device);
  }
  if (backend->initialized) {
    ma_device_uninit(&backend->device);
  }
  if (backend->context_initialized) {
    ma_context_uninit(&backend->context);
  }
  *backend = (ShroomVoiceMiniaudioBackend){0};
  snprintf(backend->selected_capture_device, sizeof(backend->selected_capture_device), "%s",
           selected_capture_device);
}

static bool Healthy(void* context) {
  const ShroomVoiceMiniaudioBackend* backend = context;
  return (backend != NULL) && backend->initialized && backend->started &&
         ma_device_is_started((ma_device*)&backend->device);
}

static size_t CaptureDevices(void* context, char names[][SHROOM_VOICE_DEVICE_NAME_LENGTH],
                             size_t capacity) {
  ShroomVoiceMiniaudioBackend* backend = context;
  ma_device_info* capture_devices = NULL;
  ma_uint32 capture_device_count = 0u;
  size_t copied = 0u;

  if ((backend == NULL) || (names == NULL) || (capacity == 0u) || !EnsureContext(backend) ||
      (ma_context_get_devices(&backend->context, NULL, NULL, &capture_devices,
                              &capture_device_count) != MA_SUCCESS)) {
    return 0u;
  }
  for (ma_uint32 index = 0u; (index < capture_device_count) && (copied < capacity); ++index) {
    snprintf(names[copied], SHROOM_VOICE_DEVICE_NAME_LENGTH, "%.*s",
             (int)SHROOM_VOICE_DEVICE_NAME_LENGTH - 1, capture_devices[index].name);
    ++copied;
  }
  return copied;
}

static bool SelectCaptureDevice(void* context, const char* device_name) {
  ShroomVoiceMiniaudioBackend* backend = context;

  if ((backend == NULL) || (device_name == NULL) ||
      (strlen(device_name) >= sizeof(backend->selected_capture_device))) {
    return false;
  }
  snprintf(backend->selected_capture_device, sizeof(backend->selected_capture_device), "%s",
           device_name);
  return true;
}

const ShroomVoiceBackend* ShroomVoiceProductionBackend(void) {
  static const ShroomVoiceBackend backend = {
      .context = &g_voice_backend,
      .start = Start,
      .stop = Stop,
      .healthy = Healthy,
      .now_ms = NULL,
      .capture_devices = CaptureDevices,
      .select_capture_device = SelectCaptureDevice,
  };
  return &backend;
}
