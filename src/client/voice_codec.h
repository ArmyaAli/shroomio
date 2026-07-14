#ifndef SHROOM_CLIENT_VOICE_CODEC_H
#define SHROOM_CLIENT_VOICE_CODEC_H

#include <stdbool.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_VOICE_SAMPLE_RATE 48000
#define SHROOM_VOICE_FRAME_DURATION_MS 20u
#define SHROOM_VOICE_FRAME_SAMPLES 960
#define SHROOM_VOICE_CHANNEL_COUNT 1
#define SHROOM_VOICE_BITRATE 48000
#define SHROOM_VOICE_COMPLEXITY 6

typedef struct ShroomVoiceEncoder {
  void* handle;
} ShroomVoiceEncoder;

typedef struct ShroomVoiceDecoder {
  void* handle;
} ShroomVoiceDecoder;

bool ShroomVoiceEncoderInit(ShroomVoiceEncoder* encoder);
void ShroomVoiceEncoderDestroy(ShroomVoiceEncoder* encoder);
int ShroomVoiceEncode(ShroomVoiceEncoder* encoder, const float pcm[SHROOM_VOICE_FRAME_SAMPLES],
                      uint8_t payload[SHROOM_VOICE_MAX_PAYLOAD_SIZE]);
bool ShroomVoiceDecoderInit(ShroomVoiceDecoder* decoder);
void ShroomVoiceDecoderDestroy(ShroomVoiceDecoder* decoder);
void ShroomVoiceDecoderReset(ShroomVoiceDecoder* decoder);
int ShroomVoiceDecode(ShroomVoiceDecoder* decoder, const uint8_t* payload, uint16_t payload_size,
                      bool decode_fec, float pcm[SHROOM_VOICE_FRAME_SAMPLES]);

#endif
