#include "unity.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "server/voice_relay.h"

typedef union TestVoiceWire {
  max_align_t alignment;
  uint8_t bytes[SHROOM_VOICE_FRAME_MAX_SIZE];
} TestVoiceWire;

typedef struct SendCapture {
  size_t count;
  size_t recipients[SHROOM_SERVER_MAX_CLIENTS];
  size_t wire_size;
  TestVoiceWire last_wire;
} SendCapture;

static ShroomVoiceRelay relay;

void setUp(void) { ShroomVoiceRelayInit(&relay); }

void tearDown(void) {}

static ShroomVoiceFramePacket* BuildFrame(TestVoiceWire* wire, uint16_t payload_size,
                                          uint32_t claimed_sender, uint32_t stream_id,
                                          uint16_t sequence, uint8_t flags) {
  ShroomVoiceFramePacket* packet;
  size_t wire_size;

  memset(wire, 0, sizeof(*wire));
  packet = (ShroomVoiceFramePacket*)wire->bytes;
  wire_size = ShroomVoiceFramePacketSize(payload_size);
  ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_VOICE_FRAME, (uint16_t)wire_size);
  packet->sender_id = claimed_sender;
  packet->stream_id = stream_id;
  packet->timestamp = (uint32_t)sequence * 960u;
  packet->sequence = sequence;
  packet->payload_size = payload_size;
  packet->flags = flags;
  memset(ShroomVoiceFramePayload(packet), 0xA5, payload_size);
  return packet;
}

static bool CaptureSend(void* context, size_t peer_index, const void* data, size_t wire_size) {
  SendCapture* capture = (SendCapture*)context;

  if ((capture == NULL) || (capture->count >= SHROOM_SERVER_MAX_CLIENTS) ||
      (wire_size > sizeof(capture->last_wire.bytes))) {
    return false;
  }
  capture->recipients[capture->count++] = peer_index;
  capture->wire_size = wire_size;
  memcpy(capture->last_wire.bytes, data, wire_size);
  return true;
}

static ShroomVoiceRelayResult Route(size_t sender_index, TestVoiceWire* wire, uint64_t now_ms,
                                    ShroomVoiceRelaySendFn send_fn, void* context,
                                    size_t* delivered_count) {
  const ShroomVoiceFramePacket* packet = (const ShroomVoiceFramePacket*)wire->bytes;
  return ShroomVoiceRelayRoute(&relay, sender_index, wire->bytes, packet->header.size, now_ms,
                               send_fn, context, delivered_count);
}

void test_valid_frame_overwrites_sender_and_only_reaches_same_lobby(void) {
  TestVoiceWire wire;
  SendCapture capture = {0};
  size_t delivered = 0u;
  const ShroomVoiceFramePacket* relayed;

  ShroomVoiceRelaySetPeer(&relay, 0u, true, 10u, 101u);
  ShroomVoiceRelaySetPeer(&relay, 1u, true, 10u, 102u);
  ShroomVoiceRelaySetPeer(&relay, 2u, true, 20u, 201u);
  ShroomVoiceRelaySetPeer(&relay, 3u, true, 10u, 0u); /* same-lobby spectator */
  ShroomVoiceRelaySetPeer(&relay, 4u, false, 10u, 104u);
  BuildFrame(&wire, 32u, 9999u, 7u, 1u, SHROOM_VOICE_FLAG_START);

  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED,
                    Route(0u, &wire, 100u, CaptureSend, &capture, &delivered));
  TEST_ASSERT_EQUAL_size_t(2u, delivered);
  TEST_ASSERT_EQUAL_size_t(1u, capture.recipients[0]);
  TEST_ASSERT_EQUAL_size_t(3u, capture.recipients[1]);
  TEST_ASSERT_EQUAL_size_t(ShroomVoiceFramePacketSize(32u), capture.wire_size);
  relayed = (const ShroomVoiceFramePacket*)capture.last_wire.bytes;
  TEST_ASSERT_EQUAL_UINT32(101u, relayed->sender_id);
  TEST_ASSERT_EQUAL_UINT32(9999u, ((const ShroomVoiceFramePacket*)wire.bytes)->sender_id);
}

void test_malformed_truncated_and_oversized_frames_are_rejected(void) {
  TestVoiceWire wire;
  ShroomVoiceFramePacket* packet;
  size_t wire_size;

  ShroomVoiceRelaySetPeer(&relay, 0u, true, 10u, 101u);
  packet = BuildFrame(&wire, 12u, 101u, 1u, 1u, SHROOM_VOICE_FLAG_START);
  wire_size = packet->header.size;

  TEST_ASSERT_EQUAL(
      SHROOM_VOICE_RELAY_INVALID_FRAME,
      ShroomVoiceRelayRoute(&relay, 0u, wire.bytes, wire_size - 1u, 0u, NULL, NULL, NULL));
  packet->header.size = (uint16_t)(wire_size + 1u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_INVALID_FRAME,
                    ShroomVoiceRelayRoute(&relay, 0u, wire.bytes, wire_size, 0u, NULL, NULL, NULL));
  packet->header.size = (uint16_t)wire_size;
  packet->payload_size = SHROOM_VOICE_MAX_PAYLOAD_SIZE + 1u;
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_INVALID_FRAME,
                    ShroomVoiceRelayRoute(&relay, 0u, wire.bytes, wire_size, 0u, NULL, NULL, NULL));
  packet->payload_size = 12u;
  packet->flags = 0x80u;
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_INVALID_FRAME,
                    ShroomVoiceRelayRoute(&relay, 0u, wire.bytes, wire_size, 0u, NULL, NULL, NULL));
}

void test_frame_rate_limit_resets_after_window(void) {
  TestVoiceWire wire;

  ShroomVoiceRelaySetPeer(&relay, 0u, true, 10u, 101u);
  for (uint16_t sequence = 0u; sequence < SHROOM_VOICE_MAX_FRAMES_PER_WINDOW; ++sequence) {
    BuildFrame(&wire, 1u, 0u, 5u, sequence, sequence == 0u ? SHROOM_VOICE_FLAG_START : 0u);
    TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 100u, NULL, NULL, NULL));
  }
  BuildFrame(&wire, 1u, 0u, 5u, SHROOM_VOICE_MAX_FRAMES_PER_WINDOW, 0u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_FRAME_RATE_LIMITED,
                    Route(0u, &wire, 100u, NULL, NULL, NULL));

  BuildFrame(&wire, 1u, 0u, 6u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 1100u, NULL, NULL, NULL));
}

void test_byte_rate_limit_counts_complete_wire_size(void) {
  TestVoiceWire wire;
  size_t accepted = 0u;

  ShroomVoiceRelaySetPeer(&relay, 0u, true, 10u, 101u);
  for (;;) {
    const uint8_t flags = accepted == 0u ? SHROOM_VOICE_FLAG_START : 0u;
    ShroomVoiceRelayResult result;

    BuildFrame(&wire, SHROOM_VOICE_MAX_PAYLOAD_SIZE, 0u, 5u, (uint16_t)accepted, flags);
    result = Route(0u, &wire, 100u, NULL, NULL, NULL);
    if (result == SHROOM_VOICE_RELAY_BYTE_RATE_LIMITED) {
      break;
    }
    TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, result);
    ++accepted;
    TEST_ASSERT_TRUE(accepted < SHROOM_VOICE_MAX_FRAMES_PER_WINDOW);
  }
  TEST_ASSERT_EQUAL_size_t(SHROOM_VOICE_MAX_BYTES_PER_WINDOW / SHROOM_VOICE_FRAME_MAX_SIZE,
                           accepted);
}

void test_active_talker_cap_is_per_lobby_and_end_releases_slot(void) {
  TestVoiceWire wire;

  for (size_t index = 0u; index <= SHROOM_VOICE_MAX_ACTIVE_TALKERS; ++index) {
    ShroomVoiceRelaySetPeer(&relay, index, true, 10u, (uint32_t)(100u + index));
  }
  ShroomVoiceRelaySetPeer(&relay, 20u, true, 20u, 220u);
  BuildFrame(&wire, 8u, 0u, 1u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(20u, &wire, 100u, NULL, NULL, NULL));

  for (size_t index = 0u; index < SHROOM_VOICE_MAX_ACTIVE_TALKERS; ++index) {
    BuildFrame(&wire, 8u, 0u, (uint32_t)(index + 1u), 0u, SHROOM_VOICE_FLAG_START);
    TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(index, &wire, 100u, NULL, NULL, NULL));
  }
  BuildFrame(&wire, 8u, 0u, 99u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_TALKER_LIMITED,
                    Route(SHROOM_VOICE_MAX_ACTIVE_TALKERS, &wire, 100u, NULL, NULL, NULL));

  BuildFrame(&wire, 8u, 0u, 1u, 1u, SHROOM_VOICE_FLAG_END);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 101u, NULL, NULL, NULL));
  BuildFrame(&wire, 8u, 0u, 99u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED,
                    Route(SHROOM_VOICE_MAX_ACTIVE_TALKERS, &wire, 101u, NULL, NULL, NULL));
}

void test_talker_timeout_and_lifecycle_reset_release_capacity(void) {
  TestVoiceWire wire;

  for (size_t index = 0u; index <= SHROOM_VOICE_MAX_ACTIVE_TALKERS; ++index) {
    ShroomVoiceRelaySetPeer(&relay, index, true, 10u, (uint32_t)(100u + index));
  }
  for (size_t index = 0u; index < SHROOM_VOICE_MAX_ACTIVE_TALKERS; ++index) {
    BuildFrame(&wire, 8u, 0u, (uint32_t)(index + 1u), 0u, SHROOM_VOICE_FLAG_START);
    TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(index, &wire, 100u, NULL, NULL, NULL));
  }

  ShroomVoiceRelaySetPeer(&relay, 0u, true, 0u, 0u); /* lobby leave */
  TEST_ASSERT_FALSE(relay.peers[0].active_talker);
  BuildFrame(&wire, 8u, 0u, 99u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED,
                    Route(SHROOM_VOICE_MAX_ACTIVE_TALKERS, &wire, 101u, NULL, NULL, NULL));

  ShroomVoiceRelaySetPeer(&relay, 1u, false, 0u, 0u); /* disconnect */
  TEST_ASSERT_FALSE(relay.peers[1].connected);
  ShroomVoiceRelaySetPeer(&relay, 30u, true, 10u, 130u);
  BuildFrame(&wire, 8u, 0u, 130u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(30u, &wire, 102u, NULL, NULL, NULL));

  ShroomVoiceRelaySetPeer(&relay, 31u, true, 10u, 131u);
  BuildFrame(&wire, 8u, 0u, 131u, 0u, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED,
                    Route(31u, &wire, 102u + SHROOM_VOICE_TALKER_TIMEOUT_MS, NULL, NULL, NULL));
}

void test_stream_lifecycle_and_sequence_wrap_are_explicit(void) {
  TestVoiceWire wire;

  ShroomVoiceRelaySetPeer(&relay, 0u, true, 10u, 101u);
  BuildFrame(&wire, 8u, 0u, 7u, UINT16_MAX, 0u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_STREAM_NOT_STARTED,
                    Route(0u, &wire, 100u, NULL, NULL, NULL));

  BuildFrame(&wire, 8u, 0u, 7u, UINT16_MAX, SHROOM_VOICE_FLAG_START);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 100u, NULL, NULL, NULL));
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, relay.peers[0].last_sequence);

  BuildFrame(&wire, 8u, 0u, 7u, 0u, 0u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 101u, NULL, NULL, NULL));
  TEST_ASSERT_EQUAL_UINT16(0u, relay.peers[0].last_sequence);

  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_SEQUENCE_REJECTED, Route(0u, &wire, 101u, NULL, NULL, NULL));
  BuildFrame(&wire, 8u, 0u, 7u, UINT16_MAX, 0u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_SEQUENCE_REJECTED, Route(0u, &wire, 101u, NULL, NULL, NULL));
  BuildFrame(&wire, 8u, 0u, 7u, 2u, 0u); /* forward gaps are valid after packet loss */
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 101u, NULL, NULL, NULL));

  BuildFrame(&wire, 8u, 0u, 8u, 1u, 0u);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_STREAM_NOT_STARTED,
                    Route(0u, &wire, 102u, NULL, NULL, NULL));
  BuildFrame(&wire, 8u, 0u, 8u, 1u, SHROOM_VOICE_FLAG_START | SHROOM_VOICE_FLAG_END);
  TEST_ASSERT_EQUAL(SHROOM_VOICE_RELAY_ACCEPTED, Route(0u, &wire, 102u, NULL, NULL, NULL));
  TEST_ASSERT_FALSE(relay.peers[0].active_talker);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_frame_overwrites_sender_and_only_reaches_same_lobby);
  RUN_TEST(test_malformed_truncated_and_oversized_frames_are_rejected);
  RUN_TEST(test_frame_rate_limit_resets_after_window);
  RUN_TEST(test_byte_rate_limit_counts_complete_wire_size);
  RUN_TEST(test_active_talker_cap_is_per_lobby_and_end_releases_slot);
  RUN_TEST(test_talker_timeout_and_lifecycle_reset_release_capacity);
  RUN_TEST(test_stream_lifecycle_and_sequence_wrap_are_explicit);
  return UNITY_END();
}
