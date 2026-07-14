#ifndef SHROOM_CLIENT_VOICE_H
#define SHROOM_CLIENT_VOICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "voice_codec.h"

typedef void (*ShroomVoiceAudioProcessFn)(void* context, float* output, const float* input,
                                          uint32_t frame_count);
typedef void (*ShroomVoiceDeviceLostFn)(void* context);

typedef struct ShroomVoiceBackend {
  void* context;
  bool (*start)(void* context, ShroomVoiceAudioProcessFn process,
                ShroomVoiceDeviceLostFn device_lost, void* callback_context);
  void (*stop)(void* context);
  bool (*healthy)(void* context);
  uint64_t (*now_ms)(void* context);
} ShroomVoiceBackend;

typedef bool (*ShroomVoiceSendFn)(void* context, const void* data, size_t wire_size);

typedef enum ShroomVoiceStatus {
  SHROOM_VOICE_STATUS_UNCONFIGURED = 0,
  SHROOM_VOICE_STATUS_IDLE,
  SHROOM_VOICE_STATUS_STARTING,
  SHROOM_VOICE_STATUS_RUNNING,
  SHROOM_VOICE_STATUS_RECOVERING,
  SHROOM_VOICE_STATUS_ERROR,
} ShroomVoiceStatus;

typedef struct ShroomVoiceStats {
  uint64_t captured_frames;
  uint64_t encoded_frames;
  uint64_t sent_frames;
  uint64_t received_frames;
  uint64_t decoded_frames;
  uint64_t fec_frames;
  uint64_t plc_frames;
  uint64_t capture_overflows;
  uint64_t playback_overflows;
  uint64_t inbound_overflows;
  uint64_t outbound_overflows;
  uint64_t late_drops;
  uint64_t duplicate_drops;
  uint64_t invalid_drops;
  uint64_t transport_drops;
  uint32_t active_decoders;
  uint32_t restart_count;
} ShroomVoiceStats;

bool ShroomVoiceConfigure(const ShroomVoiceBackend* backend);
void ShroomVoiceSetSessionActive(bool active);
void ShroomVoiceUpdate(bool push_to_talk, ShroomVoiceSendFn send_fn, void* send_context);
bool ShroomVoiceSubmitFrame(const void* data, size_t wire_size);
bool ShroomVoiceSetPlayerMuted(uint32_t player_id, bool muted);
bool ShroomVoiceRemovePlayer(uint32_t player_id);
bool ShroomVoiceResetSession(void);
bool ShroomVoiceRestart(void);
void ShroomVoiceShutdown(void);
ShroomVoiceStatus ShroomVoiceGetStatus(void);
const char* ShroomVoiceGetStatusText(void);
bool ShroomVoiceIsRunning(void);
bool ShroomVoiceIsTransmitting(void);
ShroomVoiceStats ShroomVoiceGetStats(void);

#ifdef TEST_MODE
void ShroomVoiceTestReset(void);
#endif

#endif
