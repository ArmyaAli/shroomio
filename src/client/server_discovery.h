#ifndef SHROOM_SERVER_DISCOVERY_H
#define SHROOM_SERVER_DISCOVERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <enet/enet.h>

#include "server_discovery_state.h"

typedef struct ShroomServerDiscovery {
  ShroomServerDiscoveryState state;
  ENetHost* host;
  ENetPeer* directory_peer;
  ENetPeer* candidate_peers[SHROOM_DIRECTORY_MAX_ENTRIES];
  bool enet_initialized;
} ShroomServerDiscovery;

bool ShroomServerDiscoveryBegin(ShroomServerDiscovery* discovery, const char* directory_host,
                                uint16_t directory_port, uint64_t now_ms);
void ShroomServerDiscoveryUpdate(ShroomServerDiscovery* discovery, uint64_t now_ms);
void ShroomServerDiscoveryCancel(ShroomServerDiscovery* discovery);
void ShroomServerDiscoveryShutdown(ShroomServerDiscovery* discovery);
bool ShroomServerDiscoveryIsActive(const ShroomServerDiscovery* discovery);

#endif
