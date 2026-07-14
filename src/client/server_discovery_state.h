#ifndef SHROOM_SERVER_DISCOVERY_STATE_H
#define SHROOM_SERVER_DISCOVERY_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_DISCOVERY_PROBE_TIMEOUT_MS 2000ull
#define SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS 5000ull

typedef enum ShroomDiscoveryPhase {
  SHROOM_DISCOVERY_IDLE = 0,
  SHROOM_DISCOVERY_DIRECTORY,
  SHROOM_DISCOVERY_PROBING,
  SHROOM_DISCOVERY_COMPLETE,
  SHROOM_DISCOVERY_FAILED,
  SHROOM_DISCOVERY_CANCELLED,
} ShroomDiscoveryPhase;

typedef enum ShroomDiscoveryCandidateStatus {
  SHROOM_DISCOVERY_CANDIDATE_PENDING = 0,
  SHROOM_DISCOVERY_CANDIDATE_REACHABLE,
  SHROOM_DISCOVERY_CANDIDATE_UNAVAILABLE,
} ShroomDiscoveryCandidateStatus;

typedef struct ShroomDiscoveryCandidate {
  ShroomDirectoryServerEntry server;
  uint64_t probe_started_ms;
  uint32_t probe_nonce;
  uint16_t latency_ms;
  ShroomDiscoveryCandidateStatus status;
} ShroomDiscoveryCandidate;

typedef struct ShroomServerDiscoveryState {
  ShroomDiscoveryPhase phase;
  uint32_t generation;
  uint64_t started_ms;
  uint32_t received_directory_chunks;
  uint8_t directory_chunk_count;
  size_t full_server_count;
  size_t candidate_count;
  ShroomDiscoveryCandidate candidates[SHROOM_DIRECTORY_MAX_ENTRIES];
} ShroomServerDiscoveryState;

void ShroomServerDiscoveryStateBegin(ShroomServerDiscoveryState* state, uint32_t generation,
                                     uint64_t now_ms);
bool ShroomServerDiscoveryStateIngestDirectory(ShroomServerDiscoveryState* state,
                                               const ShroomDirectoryListPacket* packet,
                                               size_t packet_size, uint64_t now_ms);
bool ShroomServerDiscoveryStateStartProbe(ShroomServerDiscoveryState* state, size_t index,
                                          uint32_t nonce, uint64_t now_ms);
bool ShroomServerDiscoveryStateAcceptProbe(ShroomServerDiscoveryState* state, size_t index,
                                           const ShroomServerProbeResponsePacket* packet,
                                           size_t packet_size, uint64_t now_ms);
void ShroomServerDiscoveryStateMarkUnavailable(ShroomServerDiscoveryState* state, size_t index);
void ShroomServerDiscoveryStateUpdate(ShroomServerDiscoveryState* state, uint64_t now_ms);
void ShroomServerDiscoveryStateFailDirectory(ShroomServerDiscoveryState* state);
void ShroomServerDiscoveryStateCancel(ShroomServerDiscoveryState* state);
size_t ShroomServerDiscoveryStateResultCount(const ShroomServerDiscoveryState* state);
const ShroomDiscoveryCandidate*
ShroomServerDiscoveryStateResult(const ShroomServerDiscoveryState* state, size_t result_index);

#endif
