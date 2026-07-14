#include "voice_codec.h"

#include <opus/opus.h>

bool ShroomVoiceEncoderInit(ShroomVoiceEncoder* encoder) {
  OpusEncoder* handle;
  int error = OPUS_OK;

  if (encoder == NULL) {
    return false;
  }
  *encoder = (ShroomVoiceEncoder){0};
  handle = opus_encoder_create(SHROOM_VOICE_SAMPLE_RATE, SHROOM_VOICE_CHANNEL_COUNT,
                               OPUS_APPLICATION_VOIP, &error);
  if ((handle == NULL) || (error != OPUS_OK) ||
      (opus_encoder_ctl(handle, OPUS_SET_BITRATE(SHROOM_VOICE_BITRATE)) != OPUS_OK) ||
      (opus_encoder_ctl(handle, OPUS_SET_COMPLEXITY(SHROOM_VOICE_COMPLEXITY)) != OPUS_OK) ||
      (opus_encoder_ctl(handle, OPUS_SET_INBAND_FEC(1)) != OPUS_OK) ||
      (opus_encoder_ctl(handle, OPUS_SET_PACKET_LOSS_PERC(10)) != OPUS_OK)) {
    if (handle != NULL) {
      opus_encoder_destroy(handle);
    }
    return false;
  }
  encoder->handle = handle;
  return true;
}

void ShroomVoiceEncoderDestroy(ShroomVoiceEncoder* encoder) {
  if ((encoder != NULL) && (encoder->handle != NULL)) {
    opus_encoder_destroy((OpusEncoder*)encoder->handle);
    encoder->handle = NULL;
  }
}

int ShroomVoiceEncode(ShroomVoiceEncoder* encoder, const float pcm[SHROOM_VOICE_FRAME_SAMPLES],
                      uint8_t payload[SHROOM_VOICE_MAX_PAYLOAD_SIZE]) {
  if ((encoder == NULL) || (encoder->handle == NULL) || (pcm == NULL) || (payload == NULL)) {
    return OPUS_BAD_ARG;
  }
  return opus_encode_float((OpusEncoder*)encoder->handle, pcm, SHROOM_VOICE_FRAME_SAMPLES, payload,
                           SHROOM_VOICE_MAX_PAYLOAD_SIZE);
}

bool ShroomVoiceDecoderInit(ShroomVoiceDecoder* decoder) {
  int error = OPUS_OK;

  if (decoder == NULL) {
    return false;
  }
  *decoder = (ShroomVoiceDecoder){0};
  decoder->handle =
      opus_decoder_create(SHROOM_VOICE_SAMPLE_RATE, SHROOM_VOICE_CHANNEL_COUNT, &error);
  if ((decoder->handle == NULL) || (error != OPUS_OK)) {
    ShroomVoiceDecoderDestroy(decoder);
    return false;
  }
  return true;
}

void ShroomVoiceDecoderDestroy(ShroomVoiceDecoder* decoder) {
  if ((decoder != NULL) && (decoder->handle != NULL)) {
    opus_decoder_destroy((OpusDecoder*)decoder->handle);
    decoder->handle = NULL;
  }
}

void ShroomVoiceDecoderReset(ShroomVoiceDecoder* decoder) {
  if ((decoder != NULL) && (decoder->handle != NULL)) {
    (void)opus_decoder_ctl((OpusDecoder*)decoder->handle, OPUS_RESET_STATE);
  }
}

int ShroomVoiceDecode(ShroomVoiceDecoder* decoder, const uint8_t* payload, uint16_t payload_size,
                      bool decode_fec, float pcm[SHROOM_VOICE_FRAME_SAMPLES]) {
  if ((decoder == NULL) || (decoder->handle == NULL) || (pcm == NULL) ||
      ((payload == NULL) && (payload_size != 0u))) {
    return OPUS_BAD_ARG;
  }
  return opus_decode_float((OpusDecoder*)decoder->handle, payload, payload_size, pcm,
                           SHROOM_VOICE_FRAME_SAMPLES, decode_fec ? 1 : 0);
}
