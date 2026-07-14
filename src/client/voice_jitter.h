#ifndef SHROOM_CLIENT_VOICE_JITTER_H
#define SHROOM_CLIENT_VOICE_JITTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_VOICE_JITTER_CAPACITY 12u
#define SHROOM_VOICE_JITTER_TARGET_FRAMES 3u
#define SHROOM_VOICE_JITTER_MAX_START_DELAY_MS 60u
#define SHROOM_VOICE_JITTER_MAX_CONSECUTIVE_LOSS 5u

typedef struct ShroomVoiceJitterFrame {
  uint32_t timestamp;
  uint64_t arrival_ms;
  uint16_t sequence;
  uint16_t payload_size;
  uint8_t flags;
  uint8_t payload[SHROOM_VOICE_MAX_PAYLOAD_SIZE];
} ShroomVoiceJitterFrame;

typedef struct ShroomVoiceJitterBuffer {
  ShroomVoiceJitterFrame frames[SHROOM_VOICE_JITTER_CAPACITY];
  uint32_t stream_id;
  uint64_t first_arrival_ms;
  uint16_t expected_sequence;
  uint8_t count;
  uint8_t consecutive_loss;
  bool initialized;
  bool started;
  bool end_received;
} ShroomVoiceJitterBuffer;

typedef enum ShroomVoiceJitterPushResult {
  SHROOM_VOICE_JITTER_PUSH_ACCEPTED = 0,
  SHROOM_VOICE_JITTER_PUSH_STREAM_NOT_STARTED,
  SHROOM_VOICE_JITTER_PUSH_LATE,
  SHROOM_VOICE_JITTER_PUSH_DUPLICATE,
  SHROOM_VOICE_JITTER_PUSH_OVERFLOW,
  SHROOM_VOICE_JITTER_PUSH_INVALID,
} ShroomVoiceJitterPushResult;

typedef enum ShroomVoiceJitterPopKind {
  SHROOM_VOICE_JITTER_POP_WAIT = 0,
  SHROOM_VOICE_JITTER_POP_FRAME,
  SHROOM_VOICE_JITTER_POP_FEC,
  SHROOM_VOICE_JITTER_POP_PLC,
  SHROOM_VOICE_JITTER_POP_FINISHED,
} ShroomVoiceJitterPopKind;

typedef struct ShroomVoiceJitterPop {
  ShroomVoiceJitterPopKind kind;
  ShroomVoiceJitterFrame frame;
  uint16_t missing_sequence;
  bool stream_ended;
} ShroomVoiceJitterPop;

void ShroomVoiceJitterInit(ShroomVoiceJitterBuffer* jitter);
void ShroomVoiceJitterReset(ShroomVoiceJitterBuffer* jitter);
ShroomVoiceJitterPushResult ShroomVoiceJitterPush(ShroomVoiceJitterBuffer* jitter,
                                                  const ShroomVoiceFramePacket* packet,
                                                  size_t wire_size, uint64_t arrival_ms);
ShroomVoiceJitterPop ShroomVoiceJitterPopNext(ShroomVoiceJitterBuffer* jitter, uint64_t now_ms);

#endif
