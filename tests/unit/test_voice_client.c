#include "unity.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "client/voice.h"
#include "client/voice_jitter.h"
#include "client/voice_mixer.h"
#include "client/voice_thread.h"

typedef union TestVoiceWire {
  max_align_t alignment;
  uint8_t bytes[SHROOM_VOICE_FRAME_MAX_SIZE];
} TestVoiceWire;

typedef struct FakeVoiceBackend {
  ShroomVoiceAudioProcessFn process;
  ShroomVoiceDeviceLostFn device_lost;
  void* callback_context;
  uint64_t now_ms;
  uint32_t start_count;
  uint32_t stop_count;
  bool healthy;
  bool fail_start;
} FakeVoiceBackend;

typedef struct FakeTransport {
  TestVoiceWire frames[16];
  size_t sizes[16];
  size_t count;
} FakeTransport;

static FakeVoiceBackend fake_backend;

void setUp(void) {
  ShroomVoiceTestReset();
  fake_backend = (FakeVoiceBackend){.healthy = true};
}

void tearDown(void) { ShroomVoiceTestReset(); }

static ShroomVoiceFramePacket* BuildFrame(TestVoiceWire* wire, uint32_t sender_id,
                                          uint32_t stream_id, uint16_t sequence, uint8_t flags) {
  const uint16_t payload_size = 8u;
  const size_t wire_size = ShroomVoiceFramePacketSize(payload_size);
  ShroomVoiceFramePacket* packet;

  memset(wire, 0, sizeof(*wire));
  packet = (ShroomVoiceFramePacket*)wire->bytes;
  ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_VOICE_FRAME, (uint16_t)wire_size);
  packet->sender_id = sender_id;
  packet->stream_id = stream_id;
  packet->timestamp = (uint32_t)sequence * SHROOM_VOICE_FRAME_SAMPLES;
  packet->sequence = sequence;
  packet->payload_size = payload_size;
  packet->flags = flags;
  memset(ShroomVoiceFramePayload(packet), (int)(sequence & 0xffu), payload_size);
  return packet;
}

static void FillSine(float* samples, float amplitude, float phase) {
  for (size_t index = 0u; index < SHROOM_VOICE_FRAME_SAMPLES; ++index) {
    samples[index] = amplitude * sinf(phase + (float)index * 0.071f);
  }
}

static float SignalEnergy(const float* samples) {
  float energy = 0.0f;
  for (size_t index = 0u; index < SHROOM_VOICE_FRAME_SAMPLES; ++index) {
    energy += fabsf(samples[index]);
  }
  return energy;
}

static bool FakeStart(void* context, ShroomVoiceAudioProcessFn process,
                      ShroomVoiceDeviceLostFn device_lost, void* callback_context) {
  FakeVoiceBackend* fake = context;
  ++fake->start_count;
  if (fake->fail_start) {
    return false;
  }
  fake->process = process;
  fake->device_lost = device_lost;
  fake->callback_context = callback_context;
  fake->healthy = true;
  return true;
}

static void FakeStop(void* context) {
  FakeVoiceBackend* fake = context;
  ++fake->stop_count;
  fake->process = NULL;
  fake->device_lost = NULL;
  fake->callback_context = NULL;
}

static bool FakeHealthy(void* context) { return ((FakeVoiceBackend*)context)->healthy; }

static uint64_t FakeNowMs(void* context) { return ((FakeVoiceBackend*)context)->now_ms; }

static ShroomVoiceBackend MakeBackend(void) {
  return (ShroomVoiceBackend){
      .context = &fake_backend,
      .start = FakeStart,
      .stop = FakeStop,
      .healthy = FakeHealthy,
      .now_ms = FakeNowMs,
  };
}

static bool CaptureSend(void* context, const void* data, size_t wire_size) {
  FakeTransport* transport = context;
  if ((transport->count >= 16u) || (wire_size > sizeof(transport->frames[0].bytes))) {
    return false;
  }
  memcpy(transport->frames[transport->count].bytes, data, wire_size);
  transport->sizes[transport->count] = wire_size;
  ++transport->count;
  return true;
}

static bool WaitForEncoded(FakeTransport* transport, size_t count, bool push_to_talk) {
  for (uint32_t attempt = 0u; attempt < 300u; ++attempt) {
    ShroomVoiceUpdate(push_to_talk, CaptureSend, transport);
    if (transport->count >= count) {
      return true;
    }
    ShroomVoiceThreadSleepMs(1u);
  }
  return false;
}

void test_opus_round_trip_and_packet_loss_concealment_produce_bounded_audio(void) {
  ShroomVoiceEncoder encoder = {0};
  ShroomVoiceDecoder decoder = {0};
  float source[SHROOM_VOICE_FRAME_SAMPLES];
  float decoded[SHROOM_VOICE_FRAME_SAMPLES];
  uint8_t payload[SHROOM_VOICE_MAX_PAYLOAD_SIZE];
  int payload_size;

  FillSine(source, 0.4f, 0.0f);
  TEST_ASSERT_TRUE(ShroomVoiceEncoderInit(&encoder));
  TEST_ASSERT_TRUE(ShroomVoiceDecoderInit(&decoder));
  payload_size = ShroomVoiceEncode(&encoder, source, payload);
  TEST_ASSERT_GREATER_THAN_INT(0, payload_size);
  TEST_ASSERT_LESS_OR_EQUAL_INT(SHROOM_VOICE_MAX_PAYLOAD_SIZE, payload_size);
  TEST_ASSERT_EQUAL_INT(
      SHROOM_VOICE_FRAME_SAMPLES,
      ShroomVoiceDecode(&decoder, payload, (uint16_t)payload_size, false, decoded));
  TEST_ASSERT_GREATER_THAN_FLOAT(1.0f, SignalEnergy(decoded));
  TEST_ASSERT_EQUAL_INT(SHROOM_VOICE_FRAME_SAMPLES,
                        ShroomVoiceDecode(&decoder, NULL, 0u, false, decoded));
  for (size_t index = 0u; index < SHROOM_VOICE_FRAME_SAMPLES; ++index) {
    TEST_ASSERT_TRUE(isfinite(decoded[index]));
  }
  ShroomVoiceDecoderDestroy(&decoder);
  ShroomVoiceEncoderDestroy(&encoder);
}

void test_jitter_buffer_reorders_frames_and_wraps_sequence_numbers(void) {
  ShroomVoiceJitterBuffer jitter;
  TestVoiceWire frames[4];
  ShroomVoiceJitterPop popped;

  ShroomVoiceJitterInit(&jitter);
  TEST_ASSERT_EQUAL(
      SHROOM_VOICE_JITTER_PUSH_ACCEPTED,
      ShroomVoiceJitterPush(&jitter,
                            BuildFrame(&frames[0], 7u, 3u, UINT16_MAX, SHROOM_VOICE_FLAG_START),
                            ShroomVoiceFramePacketSize(8u), 100u));
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_PUSH_ACCEPTED,
                    ShroomVoiceJitterPush(&jitter, BuildFrame(&frames[1], 7u, 3u, 1u, 0u),
                                          ShroomVoiceFramePacketSize(8u), 101u));
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_PUSH_ACCEPTED,
                    ShroomVoiceJitterPush(&jitter, BuildFrame(&frames[2], 7u, 3u, 0u, 0u),
                                          ShroomVoiceFramePacketSize(8u), 102u));
  popped = ShroomVoiceJitterPopNext(&jitter, 103u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_POP_FRAME, popped.kind);
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, popped.frame.sequence);
  TEST_ASSERT_EQUAL_UINT16(0u, ShroomVoiceJitterPopNext(&jitter, 104u).frame.sequence);
  TEST_ASSERT_EQUAL_UINT16(1u, ShroomVoiceJitterPopNext(&jitter, 105u).frame.sequence);

  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_PUSH_LATE,
                    ShroomVoiceJitterPush(&jitter, BuildFrame(&frames[3], 7u, 3u, 0u, 0u),
                                          ShroomVoiceFramePacketSize(8u), 106u));
}

void test_jitter_buffer_uses_fec_only_for_immediately_following_packet(void) {
  ShroomVoiceJitterBuffer jitter;
  TestVoiceWire first;
  TestVoiceWire future;
  ShroomVoiceJitterPop popped;

  ShroomVoiceJitterInit(&jitter);
  (void)ShroomVoiceJitterPush(&jitter, BuildFrame(&first, 9u, 8u, 10u, SHROOM_VOICE_FLAG_START),
                              ShroomVoiceFramePacketSize(8u), 100u);
  (void)ShroomVoiceJitterPush(&jitter, BuildFrame(&future, 9u, 8u, 13u, 0u),
                              ShroomVoiceFramePacketSize(8u), 101u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_POP_FRAME, ShroomVoiceJitterPopNext(&jitter, 200u).kind);
  popped = ShroomVoiceJitterPopNext(&jitter, 220u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_POP_PLC, popped.kind);
  TEST_ASSERT_EQUAL_UINT16(11u, popped.missing_sequence);
  ShroomVoiceJitterInit(&jitter);
  (void)ShroomVoiceJitterPush(&jitter, BuildFrame(&first, 9u, 9u, 20u, SHROOM_VOICE_FLAG_START),
                              ShroomVoiceFramePacketSize(8u), 100u);
  (void)ShroomVoiceJitterPush(&jitter, BuildFrame(&future, 9u, 9u, 22u, 0u),
                              ShroomVoiceFramePacketSize(8u), 101u);
  (void)ShroomVoiceJitterPopNext(&jitter, 200u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_POP_FEC, ShroomVoiceJitterPopNext(&jitter, 220u).kind);
}

void test_jitter_buffer_rejects_duplicates_and_bounds_overflow(void) {
  ShroomVoiceJitterBuffer jitter;
  TestVoiceWire frames[SHROOM_VOICE_JITTER_CAPACITY + 1u];

  ShroomVoiceJitterInit(&jitter);
  for (uint16_t index = 0u; index < SHROOM_VOICE_JITTER_CAPACITY; ++index) {
    const uint8_t flags = index == 0u ? SHROOM_VOICE_FLAG_START : 0u;
    TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_PUSH_ACCEPTED,
                      ShroomVoiceJitterPush(&jitter,
                                            BuildFrame(&frames[index], 2u, 4u, index, flags),
                                            ShroomVoiceFramePacketSize(8u), 100u + index));
  }
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_PUSH_DUPLICATE,
                    ShroomVoiceJitterPush(&jitter, (ShroomVoiceFramePacket*)frames[1].bytes,
                                          ShroomVoiceFramePacketSize(8u), 200u));
  TEST_ASSERT_EQUAL(SHROOM_VOICE_JITTER_PUSH_OVERFLOW,
                    ShroomVoiceJitterPush(&jitter,
                                          BuildFrame(&frames[SHROOM_VOICE_JITTER_CAPACITY], 2u, 4u,
                                                     SHROOM_VOICE_JITTER_CAPACITY, 0u),
                                          ShroomVoiceFramePacketSize(8u), 201u));
}

void test_mixer_applies_mute_volume_and_clamps_invalid_samples(void) {
  float mixed[5] = {0.9f, -0.9f, 0.0f, NAN, INFINITY};
  const float source[5] = {0.4f, -0.4f, 1.0f, 1.0f, 1.0f};

  ShroomVoiceMixAdd(mixed, source, 5u, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, mixed[0]);
  ShroomVoiceMixAdd(mixed, source, 5u, 1.0f);
  ShroomVoiceMixClamp(mixed, 5u);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, mixed[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, mixed[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, mixed[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, mixed[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, mixed[4]);
}

void test_runtime_captures_transmits_plays_back_recovers_and_stops(void) {
  const ShroomVoiceBackend backend = MakeBackend();
  FakeTransport transport = {0};
  float captured[SHROOM_VOICE_FRAME_SAMPLES];
  float output[SHROOM_VOICE_FRAME_SAMPLES];
  bool heard_audio = false;

  TEST_ASSERT_TRUE(ShroomVoiceConfigure(&backend));
  ShroomVoiceSetSessionActive(true);
  ShroomVoiceUpdate(true, CaptureSend, &transport);
  TEST_ASSERT_TRUE(ShroomVoiceIsRunning());
  TEST_ASSERT_NOT_NULL(fake_backend.process);

  for (uint32_t frame = 0u; frame < 3u; ++frame) {
    FillSine(captured, 0.35f, (float)frame);
    fake_backend.process(fake_backend.callback_context, output, captured,
                         SHROOM_VOICE_FRAME_SAMPLES);
  }
  TEST_ASSERT_TRUE(WaitForEncoded(&transport, 3u, true));
  TEST_ASSERT_TRUE(ShroomVoiceIsTransmitting());
  TEST_ASSERT_EQUAL_UINT8(SHROOM_VOICE_FLAG_START,
                          ((ShroomVoiceFramePacket*)transport.frames[0].bytes)->flags);

  TEST_ASSERT_TRUE(WaitForEncoded(&transport, 4u, false));
  TEST_ASSERT_FALSE(ShroomVoiceIsTransmitting());
  TEST_ASSERT_EQUAL_UINT8(SHROOM_VOICE_FLAG_END,
                          ((ShroomVoiceFramePacket*)transport.frames[3].bytes)->flags);

  for (size_t index = 0u; index < transport.count; ++index) {
    ((ShroomVoiceFramePacket*)transport.frames[index].bytes)->sender_id = 77u;
    TEST_ASSERT_TRUE(ShroomVoiceSubmitFrame(transport.frames[index].bytes, transport.sizes[index]));
  }
  for (uint32_t attempt = 0u; attempt < 250u; ++attempt) {
    memset(output, 0, sizeof(output));
    fake_backend.process(fake_backend.callback_context, output, NULL, SHROOM_VOICE_FRAME_SAMPLES);
    if (SignalEnergy(output) > 1.0f) {
      heard_audio = true;
      break;
    }
    ShroomVoiceThreadSleepMs(1u);
  }
  TEST_ASSERT_TRUE(heard_audio);
  for (uint32_t attempt = 0u;
       (attempt < 250u) && (ShroomVoiceGetStats().decoded_frames < 3u); ++attempt) {
    ShroomVoiceThreadSleepMs(1u);
  }
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(3u, ShroomVoiceGetStats().decoded_frames);

  fake_backend.healthy = false;
  fake_backend.device_lost(fake_backend.callback_context);
  ShroomVoiceUpdate(false, CaptureSend, &transport);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_STATUS_RECOVERING, ShroomVoiceGetStatus());
  TEST_ASSERT_FALSE(ShroomVoiceIsRunning());
  fake_backend.now_ms += 1001u;
  fake_backend.healthy = true;
  ShroomVoiceUpdate(false, CaptureSend, &transport);
  TEST_ASSERT_TRUE(ShroomVoiceIsRunning());
  TEST_ASSERT_EQUAL_UINT32(2u, fake_backend.start_count);

  ShroomVoiceSetSessionActive(false);
  TEST_ASSERT_FALSE(ShroomVoiceIsRunning());
  TEST_ASSERT_EQUAL_UINT32(2u, fake_backend.stop_count);
  TEST_ASSERT_EQUAL_UINT32(0u, ShroomVoiceGetStats().active_decoders);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_opus_round_trip_and_packet_loss_concealment_produce_bounded_audio);
  RUN_TEST(test_jitter_buffer_reorders_frames_and_wraps_sequence_numbers);
  RUN_TEST(test_jitter_buffer_uses_fec_only_for_immediately_following_packet);
  RUN_TEST(test_jitter_buffer_rejects_duplicates_and_bounds_overflow);
  RUN_TEST(test_mixer_applies_mute_volume_and_clamps_invalid_samples);
  RUN_TEST(test_runtime_captures_transmits_plays_back_recovers_and_stops);
  return UNITY_END();
}
