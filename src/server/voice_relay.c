#include "voice_relay.h"

#include <string.h>

typedef struct ShroomVoiceWireBuffer {
  _Alignas(max_align_t) uint8_t bytes[SHROOM_VOICE_FRAME_MAX_SIZE];
} ShroomVoiceWireBuffer;

static void ResetPeer(ShroomVoiceRelayPeer* peer) {
  if (peer != NULL) {
    *peer = (ShroomVoiceRelayPeer){0};
  }
}

static bool TalkerExpired(const ShroomVoiceRelayPeer* peer, uint64_t now_ms) {
  return peer->active_talker &&
         ((now_ms < peer->last_frame_ms) ||
          ((now_ms - peer->last_frame_ms) >= SHROOM_VOICE_TALKER_TIMEOUT_MS));
}

static void ExpireTalkers(ShroomVoiceRelay* relay, uint64_t now_ms) {
  for (size_t index = 0u; index < SHROOM_SERVER_MAX_CLIENTS; ++index) {
    ShroomVoiceRelayPeer* peer = &relay->peers[index];
    if (TalkerExpired(peer, now_ms)) {
      peer->active_talker = false;
      peer->active_stream_id = 0u;
      peer->has_sequence = false;
    }
  }
}

static size_t CountLobbyTalkers(const ShroomVoiceRelay* relay, uint32_t lobby_id) {
  size_t count = 0u;

  for (size_t index = 0u; index < SHROOM_SERVER_MAX_CLIENTS; ++index) {
    const ShroomVoiceRelayPeer* peer = &relay->peers[index];
    if (peer->connected && peer->active_talker && (peer->lobby_id == lobby_id)) {
      ++count;
    }
  }
  return count;
}

static void ResetRateWindowIfNeeded(ShroomVoiceRelayPeer* peer, uint64_t now_ms) {
  if ((peer->frame_count == 0u) || (now_ms < peer->window_start_ms) ||
      ((now_ms - peer->window_start_ms) >= SHROOM_VOICE_RATE_WINDOW_MS)) {
    peer->window_start_ms = now_ms;
    peer->frame_count = 0u;
    peer->byte_count = 0u;
  }
}

void ShroomVoiceRelayInit(ShroomVoiceRelay* relay) {
  if (relay != NULL) {
    memset(relay, 0, sizeof(*relay));
  }
}

void ShroomVoiceRelaySetPeer(ShroomVoiceRelay* relay, size_t peer_index, bool connected,
                             uint32_t lobby_id, uint32_t player_id) {
  ShroomVoiceRelayPeer* peer;

  if ((relay == NULL) || (peer_index >= SHROOM_SERVER_MAX_CLIENTS)) {
    return;
  }
  peer = &relay->peers[peer_index];
  if (!connected) {
    ResetPeer(peer);
    return;
  }
  if (!peer->connected || (peer->lobby_id != lobby_id) || (peer->player_id != player_id)) {
    ResetPeer(peer);
  }
  peer->connected = true;
  peer->lobby_id = lobby_id;
  peer->player_id = player_id;
}

ShroomVoiceRelayResult ShroomVoiceRelayRoute(ShroomVoiceRelay* relay, size_t sender_index,
                                             const void* data, size_t wire_size, uint64_t now_ms,
                                             ShroomVoiceRelaySendFn send_fn, void* send_context,
                                             size_t* delivered_count) {
  ShroomVoiceRelayPeer* sender;
  const ShroomVoiceFramePacket* incoming = (const ShroomVoiceFramePacket*)data;
  ShroomVoiceWireBuffer output;
  bool starts_stream;
  size_t delivered = 0u;

  if (delivered_count != NULL) {
    *delivered_count = 0u;
  }
  if ((relay == NULL) || (sender_index >= SHROOM_SERVER_MAX_CLIENTS)) {
    return SHROOM_VOICE_RELAY_INVALID_PEER;
  }
  if (!ShroomVoiceFramePacketIsValid(data, wire_size)) {
    return SHROOM_VOICE_RELAY_INVALID_FRAME;
  }

  sender = &relay->peers[sender_index];
  if (!sender->connected || (sender->lobby_id == 0u) || (sender->player_id == 0u)) {
    return SHROOM_VOICE_RELAY_NOT_IN_LOBBY;
  }

  ExpireTalkers(relay, now_ms);
  ResetRateWindowIfNeeded(sender, now_ms);
  if (sender->frame_count >= SHROOM_VOICE_MAX_FRAMES_PER_WINDOW) {
    return SHROOM_VOICE_RELAY_FRAME_RATE_LIMITED;
  }
  if ((wire_size > SHROOM_VOICE_MAX_BYTES_PER_WINDOW) ||
      (sender->byte_count > (SHROOM_VOICE_MAX_BYTES_PER_WINDOW - wire_size))) {
    return SHROOM_VOICE_RELAY_BYTE_RATE_LIMITED;
  }

  starts_stream = (incoming->flags & SHROOM_VOICE_FLAG_START) != 0u;
  if (!sender->active_talker && !starts_stream) {
    return SHROOM_VOICE_RELAY_STREAM_NOT_STARTED;
  }
  if (sender->active_talker && (sender->active_stream_id != incoming->stream_id) &&
      !starts_stream) {
    return SHROOM_VOICE_RELAY_STREAM_NOT_STARTED;
  }
  if (sender->active_talker && (sender->active_stream_id == incoming->stream_id) &&
      sender->has_sequence && !starts_stream) {
    const uint16_t sequence_delta = (uint16_t)(incoming->sequence - sender->last_sequence);
    if ((sequence_delta == 0u) || (sequence_delta >= 0x8000u)) {
      return SHROOM_VOICE_RELAY_SEQUENCE_REJECTED;
    }
  }
  if (!sender->active_talker &&
      (CountLobbyTalkers(relay, sender->lobby_id) >= SHROOM_VOICE_MAX_ACTIVE_TALKERS)) {
    return SHROOM_VOICE_RELAY_TALKER_LIMITED;
  }

  memcpy(&output, data, wire_size);
  ((ShroomVoiceFramePacket*)output.bytes)->sender_id = sender->player_id;
  sender->active_talker = true;
  sender->active_stream_id = incoming->stream_id;
  sender->last_sequence = incoming->sequence;
  sender->has_sequence = true;
  sender->last_frame_ms = now_ms;
  ++sender->frame_count;
  sender->byte_count += (uint32_t)wire_size;

  if (send_fn != NULL) {
    for (size_t index = 0u; index < SHROOM_SERVER_MAX_CLIENTS; ++index) {
      const ShroomVoiceRelayPeer* recipient = &relay->peers[index];
      if ((index == sender_index) || !recipient->connected ||
          (recipient->lobby_id != sender->lobby_id)) {
        continue;
      }
      if (send_fn(send_context, index, output.bytes, wire_size)) {
        ++delivered;
      }
    }
  }

  if ((incoming->flags & SHROOM_VOICE_FLAG_END) != 0u) {
    sender->active_talker = false;
    sender->active_stream_id = 0u;
    sender->has_sequence = false;
  }
  if (delivered_count != NULL) {
    *delivered_count = delivered;
  }
  return SHROOM_VOICE_RELAY_ACCEPTED;
}
