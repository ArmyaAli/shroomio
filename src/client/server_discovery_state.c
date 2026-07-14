#include "server_discovery_state.h"

#include <limits.h>
#include <string.h>

static bool TextIsTerminated(const char* text, size_t capacity) {
  return (text != NULL) && (memchr(text, '\0', capacity) != NULL) && (text[0] != '\0');
}

static bool EndpointMatches(const ShroomDirectoryServerEntry* left,
                            const ShroomDirectoryServerEntry* right) {
  return (left->port == right->port) && (strncmp(left->host, right->host, sizeof(left->host)) == 0);
}

static bool NormalizeEntry(ShroomDirectoryServerEntry* entry) {
  if ((entry == NULL) || (entry->server_id == 0ull) ||
      !TextIsTerminated(entry->name, sizeof(entry->name)) ||
      !TextIsTerminated(entry->host, sizeof(entry->host)) || (entry->port == 0u) ||
      (entry->capacity == 0u) || (entry->player_count > entry->capacity) ||
      (entry->player_count > SHROOM_SERVER_MAX_CLIENTS)) {
    return false;
  }
  if (entry->capacity > SHROOM_SERVER_MAX_CLIENTS) {
    entry->capacity = SHROOM_SERVER_MAX_CLIENTS;
  }
  entry->reserved = 0u;
  return entry->player_count < entry->capacity;
}

static void MaybeFinish(ShroomServerDiscoveryState* state) {
  if ((state == NULL) || (state->phase != SHROOM_DISCOVERY_PROBING)) {
    return;
  }
  for (size_t index = 0u; index < state->candidate_count; ++index) {
    if (state->candidates[index].status == SHROOM_DISCOVERY_CANDIDATE_PENDING) {
      return;
    }
  }
  state->phase = SHROOM_DISCOVERY_COMPLETE;
}

static void AddDirectoryEntry(ShroomServerDiscoveryState* state,
                              const ShroomDirectoryServerEntry* source) {
  ShroomDirectoryServerEntry entry = *source;

  if ((entry.capacity > 0u) && (entry.player_count == entry.capacity) &&
      (entry.player_count <= SHROOM_SERVER_MAX_CLIENTS) &&
      TextIsTerminated(entry.name, sizeof(entry.name)) &&
      TextIsTerminated(entry.host, sizeof(entry.host)) && (entry.server_id != 0ull) &&
      (entry.port != 0u)) {
    ++state->full_server_count;
  }
  if (!NormalizeEntry(&entry)) {
    return;
  }
  for (size_t index = 0u; index < state->candidate_count; ++index) {
    if (EndpointMatches(&state->candidates[index].server, &entry)) {
      return;
    }
  }
  if (state->candidate_count < SHROOM_DIRECTORY_MAX_ENTRIES) {
    ShroomDiscoveryCandidate* candidate = &state->candidates[state->candidate_count++];
    *candidate = (ShroomDiscoveryCandidate){.server = entry};
  }
}

void ShroomServerDiscoveryStateBegin(ShroomServerDiscoveryState* state, uint32_t generation,
                                     uint64_t now_ms) {
  if (state == NULL) {
    return;
  }
  *state = (ShroomServerDiscoveryState){0};
  if (generation == 0u) {
    state->phase = SHROOM_DISCOVERY_FAILED;
    return;
  }
  state->phase = SHROOM_DISCOVERY_DIRECTORY;
  state->generation = generation;
  state->started_ms = now_ms;
}

bool ShroomServerDiscoveryStateIngestDirectory(ShroomServerDiscoveryState* state,
                                               const ShroomDirectoryListPacket* packet,
                                               size_t packet_size, uint64_t now_ms) {
  uint32_t chunk_bit;

  if ((state == NULL) || (packet == NULL) || (state->phase != SHROOM_DISCOVERY_DIRECTORY) ||
      (now_ms < state->started_ms) ||
      ((now_ms - state->started_ms) >= SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS) ||
      (packet_size < SHROOM_DIRECTORY_LIST_HEADER_SIZE) ||
      (packet->header.type != SHROOM_PACKET_DIRECTORY_LIST) ||
      (packet->header.size != packet_size) ||
      (packet->protocol_version != SHROOM_DIRECTORY_PROTOCOL_VERSION) ||
      (packet->generation != state->generation) || (packet->chunk_count == 0u) ||
      (packet->chunk_count > SHROOM_DIRECTORY_MAX_ENTRIES) ||
      (packet->chunk_index >= packet->chunk_count) ||
      (packet->entry_count > SHROOM_DIRECTORY_ENTRIES_PER_PACKET) ||
      (packet_size != SHROOM_DIRECTORY_LIST_PACKET_SIZE(packet->entry_count))) {
    return false;
  }
  if ((state->directory_chunk_count != 0u) &&
      (state->directory_chunk_count != packet->chunk_count)) {
    return false;
  }
  state->directory_chunk_count = packet->chunk_count;
  chunk_bit = 1u << packet->chunk_index;
  if ((state->received_directory_chunks & chunk_bit) == 0u) {
    for (size_t index = 0u; index < packet->entry_count; ++index) {
      AddDirectoryEntry(state, &packet->entries[index]);
    }
    state->received_directory_chunks |= chunk_bit;
  }
  if (state->received_directory_chunks == (state->directory_chunk_count == 32u
                                               ? UINT32_MAX
                                               : (1u << state->directory_chunk_count) - 1u)) {
    state->phase =
        state->candidate_count == 0u ? SHROOM_DISCOVERY_COMPLETE : SHROOM_DISCOVERY_PROBING;
  }
  return true;
}

bool ShroomServerDiscoveryStateStartProbe(ShroomServerDiscoveryState* state, size_t index,
                                          uint32_t nonce, uint64_t now_ms) {
  ShroomDiscoveryCandidate* candidate;

  if ((state == NULL) || (state->phase != SHROOM_DISCOVERY_PROBING) ||
      (index >= state->candidate_count) || (nonce == 0u) || (now_ms < state->started_ms)) {
    return false;
  }
  candidate = &state->candidates[index];
  if ((candidate->status != SHROOM_DISCOVERY_CANDIDATE_PENDING) ||
      (candidate->probe_started_ms != 0ull)) {
    return false;
  }
  candidate->probe_nonce = nonce;
  candidate->probe_started_ms = now_ms == 0ull ? 1ull : now_ms;
  return true;
}

bool ShroomServerDiscoveryStateAcceptProbe(ShroomServerDiscoveryState* state, size_t index,
                                           const ShroomServerProbeResponsePacket* packet,
                                           size_t packet_size, uint64_t now_ms) {
  ShroomDiscoveryCandidate* candidate;
  uint16_t capacity;

  if ((state == NULL) || (packet == NULL) || (state->phase != SHROOM_DISCOVERY_PROBING) ||
      (index >= state->candidate_count) || (packet_size != sizeof(*packet)) ||
      (packet->header.type != SHROOM_PACKET_SERVER_PROBE_RESPONSE) ||
      (packet->header.size != sizeof(*packet)) ||
      (packet->protocol_version != SHROOM_PROTOCOL_VERSION) ||
      (packet->generation != state->generation)) {
    return false;
  }
  candidate = &state->candidates[index];
  if ((candidate->status != SHROOM_DISCOVERY_CANDIDATE_PENDING) ||
      (candidate->probe_started_ms == 0ull) || (packet->nonce != candidate->probe_nonce) ||
      (now_ms < candidate->probe_started_ms)) {
    return false;
  }
  capacity = packet->capacity;
  if (capacity > SHROOM_SERVER_MAX_CLIENTS) {
    capacity = SHROOM_SERVER_MAX_CLIENTS;
  }
  if ((capacity == 0u) || (packet->player_count > capacity)) {
    return false;
  }
  candidate->server.player_count = packet->player_count;
  candidate->server.capacity = capacity;
  if (packet->player_count >= capacity) {
    candidate->status = SHROOM_DISCOVERY_CANDIDATE_UNAVAILABLE;
  } else {
    const uint64_t elapsed = now_ms - candidate->probe_started_ms;
    candidate->latency_ms = elapsed > UINT16_MAX ? UINT16_MAX : (uint16_t)elapsed;
    candidate->status = SHROOM_DISCOVERY_CANDIDATE_REACHABLE;
  }
  MaybeFinish(state);
  return true;
}

void ShroomServerDiscoveryStateMarkUnavailable(ShroomServerDiscoveryState* state, size_t index) {
  if ((state == NULL) || (state->phase != SHROOM_DISCOVERY_PROBING) ||
      (index >= state->candidate_count)) {
    return;
  }
  if (state->candidates[index].status == SHROOM_DISCOVERY_CANDIDATE_PENDING) {
    state->candidates[index].status = SHROOM_DISCOVERY_CANDIDATE_UNAVAILABLE;
  }
  MaybeFinish(state);
}

void ShroomServerDiscoveryStateUpdate(ShroomServerDiscoveryState* state, uint64_t now_ms) {
  if ((state == NULL) || (now_ms < state->started_ms)) {
    return;
  }
  if ((state->phase == SHROOM_DISCOVERY_DIRECTORY) &&
      ((now_ms - state->started_ms) >= SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS)) {
    state->phase = SHROOM_DISCOVERY_FAILED;
    return;
  }
  if (state->phase != SHROOM_DISCOVERY_PROBING) {
    return;
  }
  for (size_t index = 0u; index < state->candidate_count; ++index) {
    ShroomDiscoveryCandidate* candidate = &state->candidates[index];
    if ((candidate->status == SHROOM_DISCOVERY_CANDIDATE_PENDING) &&
        (((candidate->probe_started_ms != 0ull) && (now_ms >= candidate->probe_started_ms) &&
          ((now_ms - candidate->probe_started_ms) >= SHROOM_DISCOVERY_PROBE_TIMEOUT_MS)) ||
         ((now_ms - state->started_ms) >= SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS))) {
      candidate->status = SHROOM_DISCOVERY_CANDIDATE_UNAVAILABLE;
    }
  }
  MaybeFinish(state);
}

void ShroomServerDiscoveryStateFailDirectory(ShroomServerDiscoveryState* state) {
  if ((state != NULL) && (state->phase == SHROOM_DISCOVERY_DIRECTORY)) {
    state->phase = SHROOM_DISCOVERY_FAILED;
  }
}

void ShroomServerDiscoveryStateCancel(ShroomServerDiscoveryState* state) {
  if ((state != NULL) && (state->phase != SHROOM_DISCOVERY_IDLE)) {
    state->phase = SHROOM_DISCOVERY_CANCELLED;
  }
}

size_t ShroomServerDiscoveryStateResultCount(const ShroomServerDiscoveryState* state) {
  size_t count = 0u;

  if (state == NULL) {
    return 0u;
  }
  for (size_t index = 0u; index < state->candidate_count; ++index) {
    if (state->candidates[index].status == SHROOM_DISCOVERY_CANDIDATE_REACHABLE) {
      ++count;
    }
  }
  return count;
}

const ShroomDiscoveryCandidate*
ShroomServerDiscoveryStateResult(const ShroomServerDiscoveryState* state, size_t result_index) {
  if (state == NULL) {
    return NULL;
  }
  for (size_t index = 0u; index < state->candidate_count; ++index) {
    if (state->candidates[index].status == SHROOM_DISCOVERY_CANDIDATE_REACHABLE) {
      if (result_index == 0u) {
        return &state->candidates[index];
      }
      --result_index;
    }
  }
  return NULL;
}
