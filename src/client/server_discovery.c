#include "server_discovery.h"

#include <stdint.h>

static uint32_t g_next_generation = 1u;

static void CleanupTransport(ShroomServerDiscovery* discovery) {
  if (discovery == NULL) {
    return;
  }
  if (discovery->host != NULL) {
    enet_host_destroy(discovery->host);
    discovery->host = NULL;
  }
  discovery->directory_peer = NULL;
  for (size_t index = 0u; index < SHROOM_DIRECTORY_MAX_ENTRIES; ++index) {
    discovery->candidate_peers[index] = NULL;
  }
  if (discovery->enet_initialized) {
    enet_deinitialize();
    discovery->enet_initialized = false;
  }
}

static bool SendReliable(ENetPeer* peer, ShroomPacketChannel channel, const void* data,
                         size_t size) {
  ENetPacket* packet;

  if ((peer == NULL) || (data == NULL)) {
    return false;
  }
  packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
  if ((packet == NULL) || (enet_peer_send(peer, channel, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  return true;
}

static bool SendDirectoryQuery(ShroomServerDiscovery* discovery) {
  ShroomDirectoryQueryPacket query = {.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION,
                                      .generation = discovery->state.generation};
  ShroomPacketHeaderInit(&query.header, SHROOM_PACKET_DIRECTORY_QUERY, sizeof(query));
  return SendReliable(discovery->directory_peer, SHROOM_ENET_CHANNEL_CONTROL, &query,
                      sizeof(query));
}

static bool SendProbe(ShroomServerDiscovery* discovery, size_t index) {
  const ShroomDiscoveryCandidate* candidate = &discovery->state.candidates[index];
  ShroomServerProbePacket probe = {.protocol_version = SHROOM_PROTOCOL_VERSION,
                                   .generation = discovery->state.generation,
                                   .nonce = candidate->probe_nonce};
  ShroomPacketHeaderInit(&probe.header, SHROOM_PACKET_SERVER_PROBE, sizeof(probe));
  return SendReliable(discovery->candidate_peers[index], SHROOM_ENET_CHANNEL_CONTROL, &probe,
                      sizeof(probe));
}

static uint32_t ProbeNonce(const ShroomServerDiscovery* discovery, size_t index) {
  uint32_t nonce = discovery->state.generation ^ (uint32_t)(index + 1u) * 2654435761u;
  return nonce == 0u ? (uint32_t)index + 1u : nonce;
}

static void StartCandidateProbes(ShroomServerDiscovery* discovery, uint64_t now_ms) {
  if (discovery->directory_peer != NULL) {
    enet_peer_disconnect_now(discovery->directory_peer, 0u);
    discovery->directory_peer = NULL;
  }
  for (size_t index = 0u; index < discovery->state.candidate_count; ++index) {
    const ShroomDirectoryServerEntry* server = &discovery->state.candidates[index].server;
    ENetAddress address = {.port = server->port};
    ENetPeer* peer;

    if (!ShroomServerDiscoveryStateStartProbe(&discovery->state, index,
                                              ProbeNonce(discovery, index), now_ms) ||
        (enet_address_set_host(&address, server->host) != 0)) {
      ShroomServerDiscoveryStateMarkUnavailable(&discovery->state, index);
      continue;
    }
    peer = enet_host_connect(discovery->host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
    if (peer == NULL) {
      ShroomServerDiscoveryStateMarkUnavailable(&discovery->state, index);
      continue;
    }
    peer->data = (void*)(uintptr_t)(index + 1u);
    discovery->candidate_peers[index] = peer;
  }
  enet_host_flush(discovery->host);
}

static void HandleReceive(ShroomServerDiscovery* discovery, ENetEvent* event, uint64_t now_ms) {
  const uintptr_t peer_tag = (uintptr_t)event->peer->data;

  if ((event->channelID != SHROOM_ENET_CHANNEL_CONTROL) ||
      (event->packet->dataLength < sizeof(ShroomPacketHeader))) {
    return;
  }
  if (peer_tag == 0u) {
    const ShroomDiscoveryPhase before = discovery->state.phase;
    if (ShroomServerDiscoveryStateIngestDirectory(
            &discovery->state, (const ShroomDirectoryListPacket*)event->packet->data,
            event->packet->dataLength, now_ms) &&
        (before == SHROOM_DISCOVERY_DIRECTORY) &&
        (discovery->state.phase == SHROOM_DISCOVERY_PROBING)) {
      StartCandidateProbes(discovery, now_ms);
    }
    return;
  }
  {
    const size_t index = (size_t)(peer_tag - 1u);
    if ((index < discovery->state.candidate_count) &&
        ShroomServerDiscoveryStateAcceptProbe(
            &discovery->state, index, (const ShroomServerProbeResponsePacket*)event->packet->data,
            event->packet->dataLength, now_ms)) {
      enet_peer_disconnect_now(event->peer, 0u);
      discovery->candidate_peers[index] = NULL;
    }
  }
}

bool ShroomServerDiscoveryBegin(ShroomServerDiscovery* discovery, const char* directory_host,
                                uint16_t directory_port, uint64_t now_ms) {
  ENetAddress address = {.port = directory_port};
  uint32_t generation;

  if (discovery == NULL) {
    return false;
  }
  ShroomServerDiscoveryShutdown(discovery);
  generation = g_next_generation++;
  if (generation == 0u) {
    generation = g_next_generation++;
  }
  ShroomServerDiscoveryStateBegin(&discovery->state, generation, now_ms);
  if ((directory_host == NULL) || (directory_host[0] == '\0') || (directory_port == 0u) ||
      (enet_initialize() != 0)) {
    ShroomServerDiscoveryStateFailDirectory(&discovery->state);
    return false;
  }
  discovery->enet_initialized = true;
  discovery->host =
      enet_host_create(NULL, SHROOM_DIRECTORY_MAX_ENTRIES + 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if ((discovery->host == NULL) || (enet_address_set_host(&address, directory_host) != 0)) {
    ShroomServerDiscoveryStateFailDirectory(&discovery->state);
    CleanupTransport(discovery);
    return false;
  }
  discovery->directory_peer =
      enet_host_connect(discovery->host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (discovery->directory_peer == NULL) {
    ShroomServerDiscoveryStateFailDirectory(&discovery->state);
    CleanupTransport(discovery);
    return false;
  }
  discovery->directory_peer->data = NULL;
  return true;
}

void ShroomServerDiscoveryUpdate(ShroomServerDiscovery* discovery, uint64_t now_ms) {
  ENetEvent event;
  uint32_t serviced = 0u;

  if ((discovery == NULL) || (discovery->host == NULL)) {
    return;
  }
  while ((serviced < 128u) && (enet_host_service(discovery->host, &event, 0u) > 0)) {
    ++serviced;
    if (event.type == ENET_EVENT_TYPE_CONNECT) {
      const uintptr_t peer_tag = (uintptr_t)event.peer->data;
      if (peer_tag == 0u) {
        if (!SendDirectoryQuery(discovery)) {
          ShroomServerDiscoveryStateFailDirectory(&discovery->state);
        }
      } else {
        const size_t index = (size_t)(peer_tag - 1u);
        if ((index >= discovery->state.candidate_count) || !SendProbe(discovery, index)) {
          ShroomServerDiscoveryStateMarkUnavailable(&discovery->state, index);
        }
      }
      enet_host_flush(discovery->host);
    } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
      HandleReceive(discovery, &event, now_ms);
      enet_packet_destroy(event.packet);
    } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      const uintptr_t peer_tag = (uintptr_t)event.peer->data;
      if (peer_tag == 0u) {
        ShroomServerDiscoveryStateFailDirectory(&discovery->state);
        discovery->directory_peer = NULL;
      } else {
        const size_t index = (size_t)(peer_tag - 1u);
        ShroomServerDiscoveryStateMarkUnavailable(&discovery->state, index);
        if (index < SHROOM_DIRECTORY_MAX_ENTRIES) {
          discovery->candidate_peers[index] = NULL;
        }
      }
    }
  }
  ShroomServerDiscoveryStateUpdate(&discovery->state, now_ms);
  if (!ShroomServerDiscoveryIsActive(discovery)) {
    CleanupTransport(discovery);
  }
}

void ShroomServerDiscoveryCancel(ShroomServerDiscovery* discovery) {
  if (discovery == NULL) {
    return;
  }
  ShroomServerDiscoveryStateCancel(&discovery->state);
  CleanupTransport(discovery);
}

void ShroomServerDiscoveryShutdown(ShroomServerDiscovery* discovery) {
  if (discovery == NULL) {
    return;
  }
  CleanupTransport(discovery);
  discovery->state = (ShroomServerDiscoveryState){0};
}

bool ShroomServerDiscoveryIsActive(const ShroomServerDiscovery* discovery) {
  return (discovery != NULL) && ((discovery->state.phase == SHROOM_DISCOVERY_DIRECTORY) ||
                                 (discovery->state.phase == SHROOM_DISCOVERY_PROBING));
}
