#include "voice.h"

#include "voice_jitter.h"
#include "voice_mixer.h"
#include "voice_thread.h"

#include <math.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shared/config.h"

#define SHROOM_VOICE_CAPTURE_QUEUE_CAPACITY 32u
#define SHROOM_VOICE_PLAYBACK_QUEUE_CAPACITY 32u
#define SHROOM_VOICE_INBOUND_QUEUE_CAPACITY 128u
#define SHROOM_VOICE_OUTBOUND_QUEUE_CAPACITY 32u
#define SHROOM_VOICE_RECOVERY_DELAY_MS 1000u
#define SHROOM_VOICE_WORKER_START_TIMEOUT_MS 250u

typedef struct ShroomVoicePcmFrame {
  float samples[SHROOM_VOICE_FRAME_SAMPLES];
} ShroomVoicePcmFrame;

typedef struct ShroomVoicePcmQueue {
  ShroomVoicePcmFrame frames[SHROOM_VOICE_CAPTURE_QUEUE_CAPACITY];
  atomic_uint write_index;
  atomic_uint read_index;
} ShroomVoicePcmQueue;

typedef struct ShroomVoiceWireFrame {
  alignas(max_align_t) uint8_t bytes[SHROOM_VOICE_FRAME_MAX_SIZE];
  size_t wire_size;
} ShroomVoiceWireFrame;

typedef struct ShroomVoiceWireQueue {
  ShroomVoiceWireFrame frames[SHROOM_VOICE_OUTBOUND_QUEUE_CAPACITY];
  atomic_uint write_index;
  atomic_uint read_index;
} ShroomVoiceWireQueue;

typedef enum ShroomVoiceInboundKind {
  SHROOM_VOICE_INBOUND_FRAME = 0,
  SHROOM_VOICE_INBOUND_MUTE,
  SHROOM_VOICE_INBOUND_REMOVE,
  SHROOM_VOICE_INBOUND_RESET,
} ShroomVoiceInboundKind;

typedef struct ShroomVoiceInbound {
  ShroomVoiceInboundKind kind;
  uint32_t player_id;
  bool muted;
  ShroomVoiceWireFrame frame;
} ShroomVoiceInbound;

typedef struct ShroomVoiceInboundQueue {
  ShroomVoiceInbound items[SHROOM_VOICE_INBOUND_QUEUE_CAPACITY];
  atomic_uint write_index;
  atomic_uint read_index;
} ShroomVoiceInboundQueue;

typedef struct ShroomVoiceRemote {
  uint32_t player_id;
  bool muted;
  bool decoder_initialized;
  float volume;
  ShroomVoiceDecoder decoder;
  ShroomVoiceJitterBuffer jitter;
} ShroomVoiceRemote;

typedef struct ShroomVoiceAtomicStats {
  atomic_uint_fast64_t captured_frames;
  atomic_uint_fast64_t encoded_frames;
  atomic_uint_fast64_t sent_frames;
  atomic_uint_fast64_t received_frames;
  atomic_uint_fast64_t decoded_frames;
  atomic_uint_fast64_t fec_frames;
  atomic_uint_fast64_t plc_frames;
  atomic_uint_fast64_t capture_overflows;
  atomic_uint_fast64_t playback_overflows;
  atomic_uint_fast64_t inbound_overflows;
  atomic_uint_fast64_t outbound_overflows;
  atomic_uint_fast64_t late_drops;
  atomic_uint_fast64_t duplicate_drops;
  atomic_uint_fast64_t invalid_drops;
  atomic_uint_fast64_t transport_drops;
  atomic_uint active_decoders;
  atomic_uint restart_count;
} ShroomVoiceAtomicStats;

typedef struct ShroomVoiceRuntime {
  ShroomVoiceBackend backend;
  ShroomVoiceThread worker;
  ShroomVoicePcmQueue capture_queue;
  ShroomVoicePcmQueue playback_queue;
  ShroomVoiceInboundQueue inbound_queue;
  ShroomVoiceWireQueue outbound_queue;
  ShroomVoiceRemote remotes[SHROOM_MAX_PARTICIPANTS];
  ShroomVoiceAtomicStats stats;
  float capture_partial[SHROOM_VOICE_FRAME_SAMPLES];
  uint32_t capture_partial_count;
  ShroomVoicePcmFrame playback_partial;
  uint32_t playback_partial_offset;
  bool playback_partial_valid;
  atomic_bool configured;
  atomic_bool session_active;
  atomic_bool transmitting;
  atomic_bool device_lost;
  atomic_bool worker_should_run;
  atomic_bool worker_ready;
  atomic_bool worker_failed;
  atomic_bool running;
  atomic_int status;
  uint64_t retry_at_ms;
} ShroomVoiceRuntime;

static ShroomVoiceRuntime g_voice;

static uint64_t DefaultNowMs(void) {
  struct timespec now;
  if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
    return 0u;
  }
  return ((uint64_t)now.tv_sec * 1000u) + (uint64_t)(now.tv_nsec / 1000000L);
}

static uint64_t VoiceNowMs(void) {
  return g_voice.backend.now_ms != NULL ? g_voice.backend.now_ms(g_voice.backend.context)
                                        : DefaultNowMs();
}

static void ClearQueues(void) {
  atomic_store(&g_voice.capture_queue.write_index, 0u);
  atomic_store(&g_voice.capture_queue.read_index, 0u);
  atomic_store(&g_voice.playback_queue.write_index, 0u);
  atomic_store(&g_voice.playback_queue.read_index, 0u);
  atomic_store(&g_voice.inbound_queue.write_index, 0u);
  atomic_store(&g_voice.inbound_queue.read_index, 0u);
  atomic_store(&g_voice.outbound_queue.write_index, 0u);
  atomic_store(&g_voice.outbound_queue.read_index, 0u);
  g_voice.capture_partial_count = 0u;
  g_voice.playback_partial_offset = 0u;
  g_voice.playback_partial_valid = false;
}

static bool PcmQueuePush(ShroomVoicePcmQueue* queue, const ShroomVoicePcmFrame* frame) {
  const unsigned write_index = atomic_load_explicit(&queue->write_index, memory_order_relaxed);
  const unsigned read_index = atomic_load_explicit(&queue->read_index, memory_order_acquire);

  if ((write_index - read_index) >= SHROOM_VOICE_CAPTURE_QUEUE_CAPACITY) {
    return false;
  }
  queue->frames[write_index % SHROOM_VOICE_CAPTURE_QUEUE_CAPACITY] = *frame;
  atomic_store_explicit(&queue->write_index, write_index + 1u, memory_order_release);
  return true;
}

static bool PcmQueuePop(ShroomVoicePcmQueue* queue, ShroomVoicePcmFrame* frame) {
  const unsigned read_index = atomic_load_explicit(&queue->read_index, memory_order_relaxed);
  const unsigned write_index = atomic_load_explicit(&queue->write_index, memory_order_acquire);

  if (read_index == write_index) {
    return false;
  }
  *frame = queue->frames[read_index % SHROOM_VOICE_CAPTURE_QUEUE_CAPACITY];
  atomic_store_explicit(&queue->read_index, read_index + 1u, memory_order_release);
  return true;
}

static bool WireQueuePush(ShroomVoiceWireQueue* queue, const ShroomVoiceWireFrame* frame) {
  const unsigned write_index = atomic_load_explicit(&queue->write_index, memory_order_relaxed);
  const unsigned read_index = atomic_load_explicit(&queue->read_index, memory_order_acquire);

  if ((write_index - read_index) >= SHROOM_VOICE_OUTBOUND_QUEUE_CAPACITY) {
    return false;
  }
  queue->frames[write_index % SHROOM_VOICE_OUTBOUND_QUEUE_CAPACITY] = *frame;
  atomic_store_explicit(&queue->write_index, write_index + 1u, memory_order_release);
  return true;
}

static bool WireQueuePop(ShroomVoiceWireQueue* queue, ShroomVoiceWireFrame* frame) {
  const unsigned read_index = atomic_load_explicit(&queue->read_index, memory_order_relaxed);
  const unsigned write_index = atomic_load_explicit(&queue->write_index, memory_order_acquire);

  if (read_index == write_index) {
    return false;
  }
  *frame = queue->frames[read_index % SHROOM_VOICE_OUTBOUND_QUEUE_CAPACITY];
  atomic_store_explicit(&queue->read_index, read_index + 1u, memory_order_release);
  return true;
}

static bool InboundQueuePush(const ShroomVoiceInbound* item) {
  ShroomVoiceInboundQueue* queue = &g_voice.inbound_queue;
  const unsigned write_index = atomic_load_explicit(&queue->write_index, memory_order_relaxed);
  const unsigned read_index = atomic_load_explicit(&queue->read_index, memory_order_acquire);

  if ((write_index - read_index) >= SHROOM_VOICE_INBOUND_QUEUE_CAPACITY) {
    atomic_fetch_add(&g_voice.stats.inbound_overflows, 1u);
    return false;
  }
  queue->items[write_index % SHROOM_VOICE_INBOUND_QUEUE_CAPACITY] = *item;
  atomic_store_explicit(&queue->write_index, write_index + 1u, memory_order_release);
  return true;
}

static bool InboundQueuePop(ShroomVoiceInbound* item) {
  ShroomVoiceInboundQueue* queue = &g_voice.inbound_queue;
  const unsigned read_index = atomic_load_explicit(&queue->read_index, memory_order_relaxed);
  const unsigned write_index = atomic_load_explicit(&queue->write_index, memory_order_acquire);

  if (read_index == write_index) {
    return false;
  }
  *item = queue->items[read_index % SHROOM_VOICE_INBOUND_QUEUE_CAPACITY];
  atomic_store_explicit(&queue->read_index, read_index + 1u, memory_order_release);
  return true;
}

static void ProcessAudio(void* context, float* output, const float* input, uint32_t frame_count) {
  ShroomVoiceRuntime* voice = context;
  uint32_t offset = 0u;

  if ((voice == NULL) || !atomic_load_explicit(&voice->running, memory_order_acquire)) {
    if (output != NULL) {
      memset(output, 0, (size_t)frame_count * sizeof(*output));
    }
    return;
  }

  if (input != NULL) {
    while (offset < frame_count) {
      const uint32_t available = SHROOM_VOICE_FRAME_SAMPLES - voice->capture_partial_count;
      const uint32_t copy_count =
          (frame_count - offset) < available ? (frame_count - offset) : available;
      memcpy(&voice->capture_partial[voice->capture_partial_count], &input[offset],
             (size_t)copy_count * sizeof(*input));
      voice->capture_partial_count += copy_count;
      offset += copy_count;
      if (voice->capture_partial_count == SHROOM_VOICE_FRAME_SAMPLES) {
        ShroomVoicePcmFrame frame;
        memcpy(frame.samples, voice->capture_partial, sizeof(frame.samples));
        if (PcmQueuePush(&voice->capture_queue, &frame)) {
          atomic_fetch_add(&voice->stats.captured_frames, 1u);
        } else {
          atomic_fetch_add(&voice->stats.capture_overflows, 1u);
        }
        voice->capture_partial_count = 0u;
      }
    }
  }

  offset = 0u;
  while ((output != NULL) && (offset < frame_count)) {
    if (!voice->playback_partial_valid) {
      if (!PcmQueuePop(&voice->playback_queue, &voice->playback_partial)) {
        memset(&output[offset], 0, (size_t)(frame_count - offset) * sizeof(*output));
        break;
      }
      voice->playback_partial_offset = 0u;
      voice->playback_partial_valid = true;
    }
    {
      const uint32_t available = SHROOM_VOICE_FRAME_SAMPLES - voice->playback_partial_offset;
      const uint32_t copy_count =
          (frame_count - offset) < available ? (frame_count - offset) : available;
      memcpy(&output[offset], &voice->playback_partial.samples[voice->playback_partial_offset],
             (size_t)copy_count * sizeof(*output));
      voice->playback_partial_offset += copy_count;
      offset += copy_count;
      if (voice->playback_partial_offset == SHROOM_VOICE_FRAME_SAMPLES) {
        voice->playback_partial_valid = false;
      }
    }
  }
}

static void MarkDeviceLost(void* context) {
  ShroomVoiceRuntime* voice = context;
  if (voice != NULL) {
    atomic_store_explicit(&voice->device_lost, true, memory_order_release);
  }
}

static ShroomVoiceRemote* FindRemote(uint32_t player_id, bool create) {
  ShroomVoiceRemote* empty = NULL;

  for (size_t index = 0u; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    ShroomVoiceRemote* remote = &g_voice.remotes[index];
    if (remote->player_id == player_id) {
      return remote;
    }
    if ((empty == NULL) && (remote->player_id == 0u)) {
      empty = remote;
    }
  }
  if (create && (empty != NULL)) {
    *empty = (ShroomVoiceRemote){.player_id = player_id, .volume = 1.0f};
    ShroomVoiceJitterInit(&empty->jitter);
  }
  return create ? empty : NULL;
}

static void DestroyRemote(ShroomVoiceRemote* remote) {
  if (remote == NULL) {
    return;
  }
  if (remote->decoder_initialized) {
    ShroomVoiceDecoderDestroy(&remote->decoder);
    atomic_fetch_sub(&g_voice.stats.active_decoders, 1u);
  }
  *remote = (ShroomVoiceRemote){0};
}

static void DestroyAllRemotes(void) {
  for (size_t index = 0u; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    DestroyRemote(&g_voice.remotes[index]);
  }
}

static void HandleInboundFrame(const ShroomVoiceWireFrame* wire) {
  const ShroomVoiceFramePacket* packet = (const ShroomVoiceFramePacket*)wire->bytes;
  ShroomVoiceRemote* remote;
  ShroomVoiceJitterPushResult result;

  if (!ShroomVoiceFramePacketIsValid(packet, wire->wire_size) || (packet->sender_id == 0u)) {
    atomic_fetch_add(&g_voice.stats.invalid_drops, 1u);
    return;
  }
  remote = FindRemote(packet->sender_id, (packet->flags & SHROOM_VOICE_FLAG_START) != 0u);
  if (remote == NULL) {
    atomic_fetch_add(&g_voice.stats.invalid_drops, 1u);
    return;
  }
  if (!remote->decoder_initialized) {
    if (!ShroomVoiceDecoderInit(&remote->decoder)) {
      atomic_fetch_add(&g_voice.stats.invalid_drops, 1u);
      return;
    }
    remote->decoder_initialized = true;
    atomic_fetch_add(&g_voice.stats.active_decoders, 1u);
  }
  if ((packet->flags & SHROOM_VOICE_FLAG_START) != 0u) {
    ShroomVoiceDecoderReset(&remote->decoder);
  }
  result = ShroomVoiceJitterPush(&remote->jitter, packet, wire->wire_size, DefaultNowMs());
  if (result == SHROOM_VOICE_JITTER_PUSH_ACCEPTED) {
    atomic_fetch_add(&g_voice.stats.received_frames, 1u);
  } else if (result == SHROOM_VOICE_JITTER_PUSH_LATE) {
    atomic_fetch_add(&g_voice.stats.late_drops, 1u);
  } else if (result == SHROOM_VOICE_JITTER_PUSH_DUPLICATE) {
    atomic_fetch_add(&g_voice.stats.duplicate_drops, 1u);
  } else {
    atomic_fetch_add(&g_voice.stats.invalid_drops, 1u);
  }
}

static void ProcessInbound(void) {
  ShroomVoiceInbound item;

  while (InboundQueuePop(&item)) {
    switch (item.kind) {
    case SHROOM_VOICE_INBOUND_FRAME:
      HandleInboundFrame(&item.frame);
      break;
    case SHROOM_VOICE_INBOUND_MUTE: {
      ShroomVoiceRemote* remote = FindRemote(item.player_id, true);
      if (remote != NULL) {
        remote->muted = item.muted;
      }
    } break;
    case SHROOM_VOICE_INBOUND_REMOVE:
      DestroyRemote(FindRemote(item.player_id, false));
      break;
    case SHROOM_VOICE_INBOUND_RESET:
      DestroyAllRemotes();
      break;
    default:
      break;
    }
  }
}

static void ProcessPlayback(uint64_t now_ms) {
  ShroomVoicePcmFrame mixed = {0};
  bool decoded_any = false;

  for (size_t index = 0u; index < SHROOM_MAX_PARTICIPANTS; ++index) {
    ShroomVoiceRemote* remote = &g_voice.remotes[index];
    ShroomVoiceJitterPop popped;
    float decoded[SHROOM_VOICE_FRAME_SAMPLES];
    int decoded_samples = 0;

    if ((remote->player_id == 0u) || !remote->decoder_initialized) {
      continue;
    }
    popped = ShroomVoiceJitterPopNext(&remote->jitter, now_ms);
    if (popped.kind == SHROOM_VOICE_JITTER_POP_WAIT) {
      continue;
    }
    if (popped.kind == SHROOM_VOICE_JITTER_POP_FINISHED) {
      DestroyRemote(remote);
      continue;
    }
    if (popped.kind == SHROOM_VOICE_JITTER_POP_FRAME) {
      decoded_samples = ShroomVoiceDecode(&remote->decoder, popped.frame.payload,
                                          popped.frame.payload_size, false, decoded);
    } else if (popped.kind == SHROOM_VOICE_JITTER_POP_FEC) {
      decoded_samples = ShroomVoiceDecode(&remote->decoder, popped.frame.payload,
                                          popped.frame.payload_size, true, decoded);
      if (decoded_samples > 0) {
        atomic_fetch_add(&g_voice.stats.fec_frames, 1u);
      }
    }
    if ((popped.kind == SHROOM_VOICE_JITTER_POP_PLC) || (decoded_samples <= 0)) {
      decoded_samples = ShroomVoiceDecode(&remote->decoder, NULL, 0u, false, decoded);
      if (decoded_samples > 0) {
        atomic_fetch_add(&g_voice.stats.plc_frames, 1u);
      }
    }
    if (decoded_samples > 0) {
      if (!remote->muted) {
        ShroomVoiceMixAdd(mixed.samples, decoded, (size_t)decoded_samples, remote->volume);
      }
      decoded_any = true;
      atomic_fetch_add(&g_voice.stats.decoded_frames, 1u);
    }
    if (popped.stream_ended) {
      DestroyRemote(remote);
    }
  }

  if (decoded_any) {
    ShroomVoiceMixClamp(mixed.samples, SHROOM_VOICE_FRAME_SAMPLES);
    if (!PcmQueuePush(&g_voice.playback_queue, &mixed)) {
      atomic_fetch_add(&g_voice.stats.playback_overflows, 1u);
    }
  }
}

static bool QueueEncoded(ShroomVoiceEncoder* encoder, const ShroomVoicePcmFrame* pcm,
                         uint32_t stream_id, uint16_t sequence, uint32_t timestamp, uint8_t flags,
                         ShroomVoiceWireFrame* pending, bool* pending_valid) {
  ShroomVoiceWireFrame wire = {0};
  ShroomVoiceFramePacket* packet = (ShroomVoiceFramePacket*)wire.bytes;
  int payload_size;

  payload_size = ShroomVoiceEncode(encoder, pcm->samples, ShroomVoiceFramePayload(packet));
  if (payload_size <= 0) {
    return false;
  }
  wire.wire_size = ShroomVoiceFramePacketSize((uint16_t)payload_size);
  ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_VOICE_FRAME, (uint16_t)wire.wire_size);
  packet->stream_id = stream_id;
  packet->timestamp = timestamp;
  packet->sequence = sequence;
  packet->payload_size = (uint16_t)payload_size;
  packet->flags = flags;
  if (!WireQueuePush(&g_voice.outbound_queue, &wire)) {
    *pending = wire;
    *pending_valid = true;
    atomic_fetch_add(&g_voice.stats.outbound_overflows, 1u);
  }
  atomic_fetch_add(&g_voice.stats.encoded_frames, 1u);
  return true;
}

static int WorkerMain(void* context) {
  ShroomVoiceRuntime* voice = context;
  ShroomVoiceEncoder encoder = {0};
  ShroomVoiceWireFrame pending = {0};
  bool pending_valid = false;
  bool stream_active = false;
  uint32_t stream_id = 0u;
  uint32_t timestamp = 0u;
  uint16_t sequence = 0u;
  uint64_t next_playback_ms = 0u;

  if (!ShroomVoiceEncoderInit(&encoder)) {
    atomic_store(&voice->worker_failed, true);
    return 1;
  }
  atomic_store(&voice->worker_ready, true);

  while (atomic_load_explicit(&voice->worker_should_run, memory_order_acquire)) {
    ShroomVoicePcmFrame captured;
    const bool transmit = atomic_load_explicit(&voice->transmitting, memory_order_acquire);
    const uint64_t now_ms = DefaultNowMs();
    bool did_work = false;

    if (pending_valid && WireQueuePush(&voice->outbound_queue, &pending)) {
      pending_valid = false;
      did_work = true;
    }
    ProcessInbound();

    if (!pending_valid && transmit && PcmQueuePop(&voice->capture_queue, &captured)) {
      uint8_t flags = 0u;
      if (!stream_active) {
        ++stream_id;
        if (stream_id == 0u) {
          ++stream_id;
        }
        sequence = 0u;
        flags = SHROOM_VOICE_FLAG_START;
        stream_active = true;
      }
      if (QueueEncoded(&encoder, &captured, stream_id, sequence++, timestamp, flags, &pending,
                       &pending_valid)) {
        timestamp += SHROOM_VOICE_FRAME_SAMPLES;
      }
      did_work = true;
    } else if (!transmit) {
      while (PcmQueuePop(&voice->capture_queue, &captured)) {
        did_work = true;
      }
      if (!pending_valid && stream_active) {
        const ShroomVoicePcmFrame silence = {0};
        if (QueueEncoded(&encoder, &silence, stream_id, sequence++, timestamp,
                         SHROOM_VOICE_FLAG_END, &pending, &pending_valid)) {
          timestamp += SHROOM_VOICE_FRAME_SAMPLES;
        }
        stream_active = false;
        did_work = true;
      }
    }

    if (next_playback_ms == 0u) {
      next_playback_ms = now_ms + SHROOM_VOICE_FRAME_DURATION_MS;
    }
    for (uint8_t catch_up = 0u; (now_ms >= next_playback_ms) && (catch_up < 4u); ++catch_up) {
      ProcessPlayback(now_ms);
      next_playback_ms += SHROOM_VOICE_FRAME_DURATION_MS;
      did_work = true;
    }
    if (!did_work) {
      ShroomVoiceThreadSleepMs(1u);
    }
  }

  DestroyAllRemotes();
  ShroomVoiceEncoderDestroy(&encoder);
  atomic_store(&voice->worker_ready, false);
  return 0;
}

static void StopRuntime(void) {
  if (atomic_load(&g_voice.running) && (g_voice.backend.stop != NULL)) {
    g_voice.backend.stop(g_voice.backend.context);
  }
  atomic_store(&g_voice.running, false);
  atomic_store(&g_voice.transmitting, false);
  atomic_store(&g_voice.worker_should_run, false);
  ShroomVoiceThreadJoin(&g_voice.worker);
  ClearQueues();
}

static bool StartRuntime(void) {
  if (!atomic_load(&g_voice.configured) || (g_voice.backend.start == NULL)) {
    atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_UNCONFIGURED);
    return false;
  }
  if (atomic_load(&g_voice.running)) {
    return true;
  }

  ClearQueues();
  atomic_store(&g_voice.device_lost, false);
  atomic_store(&g_voice.worker_failed, false);
  atomic_store(&g_voice.worker_ready, false);
  atomic_store(&g_voice.worker_should_run, true);
  atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_STARTING);
  if (!ShroomVoiceThreadStart(&g_voice.worker, WorkerMain, &g_voice)) {
    atomic_store(&g_voice.worker_should_run, false);
    atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_ERROR);
    return false;
  }
  for (uint32_t waited = 0u; waited < SHROOM_VOICE_WORKER_START_TIMEOUT_MS; ++waited) {
    if (atomic_load(&g_voice.worker_ready) || atomic_load(&g_voice.worker_failed)) {
      break;
    }
    ShroomVoiceThreadSleepMs(1u);
  }
  if (!atomic_load(&g_voice.worker_ready) || atomic_load(&g_voice.worker_failed) ||
      !g_voice.backend.start(g_voice.backend.context, ProcessAudio, MarkDeviceLost, &g_voice)) {
    atomic_store(&g_voice.worker_should_run, false);
    ShroomVoiceThreadJoin(&g_voice.worker);
    atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_ERROR);
    return false;
  }
  atomic_store(&g_voice.running, true);
  atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_RUNNING);
  return true;
}

bool ShroomVoiceConfigure(const ShroomVoiceBackend* backend) {
  ShroomVoiceShutdown();
  if ((backend == NULL) || (backend->start == NULL) || (backend->stop == NULL)) {
    atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_UNCONFIGURED);
    return false;
  }
  g_voice.backend = *backend;
  atomic_store(&g_voice.configured, true);
  atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_IDLE);
  return true;
}

void ShroomVoiceSetSessionActive(bool active) {
  atomic_store(&g_voice.session_active, active);
  if (!active) {
    StopRuntime();
    atomic_store(&g_voice.status, atomic_load(&g_voice.configured)
                                      ? SHROOM_VOICE_STATUS_IDLE
                                      : SHROOM_VOICE_STATUS_UNCONFIGURED);
  }
}

void ShroomVoiceUpdate(bool push_to_talk, ShroomVoiceSendFn send_fn, void* send_context) {
  const bool session_active = atomic_load(&g_voice.session_active);
  const uint64_t now_ms = VoiceNowMs();
  ShroomVoiceWireFrame wire;

  if (atomic_load(&g_voice.running) &&
      (atomic_exchange(&g_voice.device_lost, false) ||
       ((g_voice.backend.healthy != NULL) && !g_voice.backend.healthy(g_voice.backend.context)))) {
    StopRuntime();
    g_voice.retry_at_ms = now_ms + SHROOM_VOICE_RECOVERY_DELAY_MS;
    atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_RECOVERING);
  }
  if (session_active && !atomic_load(&g_voice.running) && (now_ms >= g_voice.retry_at_ms)) {
    if (!StartRuntime()) {
      g_voice.retry_at_ms = now_ms + SHROOM_VOICE_RECOVERY_DELAY_MS;
      atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_RECOVERING);
    } else {
      atomic_fetch_add(&g_voice.stats.restart_count, 1u);
    }
  }

  atomic_store(&g_voice.transmitting,
               session_active && atomic_load(&g_voice.running) && push_to_talk);
  while (WireQueuePop(&g_voice.outbound_queue, &wire)) {
    if ((send_fn != NULL) && send_fn(send_context, wire.bytes, wire.wire_size)) {
      atomic_fetch_add(&g_voice.stats.sent_frames, 1u);
    } else {
      atomic_fetch_add(&g_voice.stats.transport_drops, 1u);
    }
  }
}

bool ShroomVoiceSubmitFrame(const void* data, size_t wire_size) {
  ShroomVoiceInbound item = {.kind = SHROOM_VOICE_INBOUND_FRAME};

  if (!atomic_load(&g_voice.running) || !ShroomVoiceFramePacketIsValid(data, wire_size) ||
      (wire_size > sizeof(item.frame.bytes))) {
    atomic_fetch_add(&g_voice.stats.invalid_drops, 1u);
    return false;
  }
  memcpy(item.frame.bytes, data, wire_size);
  item.frame.wire_size = wire_size;
  return InboundQueuePush(&item);
}

bool ShroomVoiceSetPlayerMuted(uint32_t player_id, bool muted) {
  const ShroomVoiceInbound item = {
      .kind = SHROOM_VOICE_INBOUND_MUTE,
      .player_id = player_id,
      .muted = muted,
  };
  return (player_id != 0u) && InboundQueuePush(&item);
}

bool ShroomVoiceRemovePlayer(uint32_t player_id) {
  const ShroomVoiceInbound item = {
      .kind = SHROOM_VOICE_INBOUND_REMOVE,
      .player_id = player_id,
  };
  return (player_id != 0u) && InboundQueuePush(&item);
}

bool ShroomVoiceResetSession(void) {
  const ShroomVoiceInbound item = {.kind = SHROOM_VOICE_INBOUND_RESET};
  return InboundQueuePush(&item);
}

bool ShroomVoiceRestart(void) {
  if (!atomic_load(&g_voice.session_active)) {
    return false;
  }
  StopRuntime();
  g_voice.retry_at_ms = 0u;
  return StartRuntime();
}

void ShroomVoiceShutdown(void) {
  StopRuntime();
  atomic_store(&g_voice.session_active, false);
  atomic_store(&g_voice.configured, false);
  atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_UNCONFIGURED);
  g_voice.retry_at_ms = 0u;
}

ShroomVoiceStatus ShroomVoiceGetStatus(void) {
  return (ShroomVoiceStatus)atomic_load(&g_voice.status);
}

const char* ShroomVoiceGetStatusText(void) {
  switch (ShroomVoiceGetStatus()) {
  case SHROOM_VOICE_STATUS_IDLE:
    return "Voice idle";
  case SHROOM_VOICE_STATUS_STARTING:
    return "Voice starting";
  case SHROOM_VOICE_STATUS_RUNNING:
    return "Voice ready";
  case SHROOM_VOICE_STATUS_RECOVERING:
    return "Voice device unavailable; retrying";
  case SHROOM_VOICE_STATUS_ERROR:
    return "Voice failed";
  case SHROOM_VOICE_STATUS_UNCONFIGURED:
  default:
    return "Voice unavailable";
  }
}

bool ShroomVoiceIsRunning(void) { return atomic_load(&g_voice.running); }

bool ShroomVoiceIsTransmitting(void) { return atomic_load(&g_voice.transmitting); }

ShroomVoiceStats ShroomVoiceGetStats(void) {
  return (ShroomVoiceStats){
      .captured_frames = atomic_load(&g_voice.stats.captured_frames),
      .encoded_frames = atomic_load(&g_voice.stats.encoded_frames),
      .sent_frames = atomic_load(&g_voice.stats.sent_frames),
      .received_frames = atomic_load(&g_voice.stats.received_frames),
      .decoded_frames = atomic_load(&g_voice.stats.decoded_frames),
      .fec_frames = atomic_load(&g_voice.stats.fec_frames),
      .plc_frames = atomic_load(&g_voice.stats.plc_frames),
      .capture_overflows = atomic_load(&g_voice.stats.capture_overflows),
      .playback_overflows = atomic_load(&g_voice.stats.playback_overflows),
      .inbound_overflows = atomic_load(&g_voice.stats.inbound_overflows),
      .outbound_overflows = atomic_load(&g_voice.stats.outbound_overflows),
      .late_drops = atomic_load(&g_voice.stats.late_drops),
      .duplicate_drops = atomic_load(&g_voice.stats.duplicate_drops),
      .invalid_drops = atomic_load(&g_voice.stats.invalid_drops),
      .transport_drops = atomic_load(&g_voice.stats.transport_drops),
      .active_decoders = atomic_load(&g_voice.stats.active_decoders),
      .restart_count = atomic_load(&g_voice.stats.restart_count),
  };
}

#ifdef TEST_MODE
void ShroomVoiceTestReset(void) {
  ShroomVoiceShutdown();
  memset(&g_voice, 0, sizeof(g_voice));
  atomic_store(&g_voice.status, SHROOM_VOICE_STATUS_UNCONFIGURED);
}
#endif
