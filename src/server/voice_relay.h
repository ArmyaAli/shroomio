#ifndef SHROOM_SERVER_VOICE_RELAY_H
#define SHROOM_SERVER_VOICE_RELAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_VOICE_RATE_WINDOW_MS 1000u
#define SHROOM_VOICE_MAX_FRAMES_PER_WINDOW 50u
#define SHROOM_VOICE_MAX_BYTES_PER_WINDOW 16384u
#define SHROOM_VOICE_MAX_ACTIVE_TALKERS 8u
#define SHROOM_VOICE_TALKER_TIMEOUT_MS 500u

typedef struct ShroomVoiceRelayPeer {
  bool connected;
  bool active_talker;
  bool has_sequence;
  uint32_t lobby_id;
  uint32_t player_id;
  uint32_t active_stream_id;
  uint32_t frame_count;
  uint32_t byte_count;
  uint16_t last_sequence;
  uint64_t window_start_ms;
  uint64_t last_frame_ms;
} ShroomVoiceRelayPeer;

typedef struct ShroomVoiceRelay {
  ShroomVoiceRelayPeer peers[SHROOM_SERVER_MAX_CLIENTS];
} ShroomVoiceRelay;

typedef enum ShroomVoiceRelayResult {
  SHROOM_VOICE_RELAY_ACCEPTED = 0,
  SHROOM_VOICE_RELAY_INVALID_PEER,
  SHROOM_VOICE_RELAY_INVALID_FRAME,
  SHROOM_VOICE_RELAY_NOT_IN_LOBBY,
  SHROOM_VOICE_RELAY_STREAM_NOT_STARTED,
  SHROOM_VOICE_RELAY_SEQUENCE_REJECTED,
  SHROOM_VOICE_RELAY_FRAME_RATE_LIMITED,
  SHROOM_VOICE_RELAY_BYTE_RATE_LIMITED,
  SHROOM_VOICE_RELAY_TALKER_LIMITED,
} ShroomVoiceRelayResult;

typedef bool (*ShroomVoiceRelaySendFn)(void* context, size_t peer_index, const void* data,
                                       size_t wire_size);

void ShroomVoiceRelayInit(ShroomVoiceRelay* relay);
void ShroomVoiceRelaySetPeer(ShroomVoiceRelay* relay, size_t peer_index, bool connected,
                             uint32_t lobby_id, uint32_t player_id);
ShroomVoiceRelayResult ShroomVoiceRelayRoute(ShroomVoiceRelay* relay, size_t sender_index,
                                             const void* data, size_t wire_size, uint64_t now_ms,
                                             ShroomVoiceRelaySendFn send_fn, void* send_context,
                                             size_t* delivered_count);

#endif
