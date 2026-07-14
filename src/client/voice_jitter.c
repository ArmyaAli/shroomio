#include "voice_jitter.h"

#include <string.h>

static int16_t SequenceDelta(uint16_t value, uint16_t reference) {
  return (int16_t)(value - reference);
}

static void StartStream(ShroomVoiceJitterBuffer* jitter, const ShroomVoiceFramePacket* packet,
                        uint64_t arrival_ms) {
  ShroomVoiceJitterReset(jitter);
  jitter->stream_id = packet->stream_id;
  jitter->expected_sequence = packet->sequence;
  jitter->first_arrival_ms = arrival_ms;
  jitter->initialized = true;
}

static int FindSequence(const ShroomVoiceJitterBuffer* jitter, uint16_t sequence) {
  for (uint8_t index = 0u; index < jitter->count; ++index) {
    if (jitter->frames[index].sequence == sequence) {
      return (int)index;
    }
  }
  return -1;
}

static int FindNextSequence(const ShroomVoiceJitterBuffer* jitter) {
  for (uint8_t index = 0u; index < jitter->count; ++index) {
    const int16_t delta = SequenceDelta(jitter->frames[index].sequence, jitter->expected_sequence);
    if (delta == 1) {
      return (int)index;
    }
  }
  return -1;
}

static ShroomVoiceJitterFrame RemoveFrame(ShroomVoiceJitterBuffer* jitter, uint8_t index) {
  const ShroomVoiceJitterFrame frame = jitter->frames[index];

  if (index + 1u < jitter->count) {
    memmove(&jitter->frames[index], &jitter->frames[index + 1u],
            (size_t)(jitter->count - index - 1u) * sizeof(jitter->frames[0]));
  }
  --jitter->count;
  return frame;
}

void ShroomVoiceJitterInit(ShroomVoiceJitterBuffer* jitter) {
  if (jitter != NULL) {
    memset(jitter, 0, sizeof(*jitter));
  }
}

void ShroomVoiceJitterReset(ShroomVoiceJitterBuffer* jitter) { ShroomVoiceJitterInit(jitter); }

ShroomVoiceJitterPushResult ShroomVoiceJitterPush(ShroomVoiceJitterBuffer* jitter,
                                                  const ShroomVoiceFramePacket* packet,
                                                  size_t wire_size, uint64_t arrival_ms) {
  ShroomVoiceJitterFrame* destination;

  if ((jitter == NULL) || !ShroomVoiceFramePacketIsValid(packet, wire_size) ||
      (packet->sender_id == 0u)) {
    return SHROOM_VOICE_JITTER_PUSH_INVALID;
  }
  if ((packet->flags & SHROOM_VOICE_FLAG_START) != 0u) {
    StartStream(jitter, packet, arrival_ms);
  } else if (!jitter->initialized || (jitter->stream_id != packet->stream_id)) {
    return SHROOM_VOICE_JITTER_PUSH_STREAM_NOT_STARTED;
  }
  if (jitter->started && (SequenceDelta(packet->sequence, jitter->expected_sequence) < 0)) {
    return SHROOM_VOICE_JITTER_PUSH_LATE;
  }
  if (FindSequence(jitter, packet->sequence) >= 0) {
    return SHROOM_VOICE_JITTER_PUSH_DUPLICATE;
  }
  if (jitter->count >= SHROOM_VOICE_JITTER_CAPACITY) {
    return SHROOM_VOICE_JITTER_PUSH_OVERFLOW;
  }

  destination = &jitter->frames[jitter->count++];
  *destination = (ShroomVoiceJitterFrame){
      .timestamp = packet->timestamp,
      .arrival_ms = arrival_ms,
      .sequence = packet->sequence,
      .payload_size = packet->payload_size,
      .flags = packet->flags,
  };
  memcpy(destination->payload, ShroomVoiceFramePayloadConst(packet), packet->payload_size);
  jitter->end_received = jitter->end_received || ((packet->flags & SHROOM_VOICE_FLAG_END) != 0u);
  return SHROOM_VOICE_JITTER_PUSH_ACCEPTED;
}

ShroomVoiceJitterPop ShroomVoiceJitterPopNext(ShroomVoiceJitterBuffer* jitter, uint64_t now_ms) {
  ShroomVoiceJitterPop result = {.kind = SHROOM_VOICE_JITTER_POP_WAIT};
  int frame_index;

  if ((jitter == NULL) || !jitter->initialized) {
    result.kind = SHROOM_VOICE_JITTER_POP_FINISHED;
    return result;
  }
  if (!jitter->started) {
    const bool delay_elapsed =
        (now_ms < jitter->first_arrival_ms) ||
        ((now_ms - jitter->first_arrival_ms) >= SHROOM_VOICE_JITTER_MAX_START_DELAY_MS);
    if ((jitter->count < SHROOM_VOICE_JITTER_TARGET_FRAMES) && !jitter->end_received &&
        !delay_elapsed) {
      return result;
    }
    jitter->started = true;
  }

  frame_index = FindSequence(jitter, jitter->expected_sequence);
  if (frame_index >= 0) {
    result.kind = SHROOM_VOICE_JITTER_POP_FRAME;
    result.frame = RemoveFrame(jitter, (uint8_t)frame_index);
    result.stream_ended = (result.frame.flags & SHROOM_VOICE_FLAG_END) != 0u;
    ++jitter->expected_sequence;
    jitter->consecutive_loss = 0u;
    if (result.stream_ended) {
      ShroomVoiceJitterReset(jitter);
    }
    return result;
  }

  frame_index = FindNextSequence(jitter);
  if (frame_index >= 0) {
    result.kind = SHROOM_VOICE_JITTER_POP_FEC;
    result.frame = jitter->frames[frame_index];
  } else {
    result.kind = SHROOM_VOICE_JITTER_POP_PLC;
  }
  result.missing_sequence = jitter->expected_sequence++;
  ++jitter->consecutive_loss;
  if (jitter->consecutive_loss > SHROOM_VOICE_JITTER_MAX_CONSECUTIVE_LOSS) {
    ShroomVoiceJitterReset(jitter);
    result.kind = SHROOM_VOICE_JITTER_POP_FINISHED;
  }
  return result;
}
