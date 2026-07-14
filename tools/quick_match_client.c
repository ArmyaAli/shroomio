#include <enet/enet.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client/quick_match.h"
#include "client/server_discovery.h"
#include "shared/protocol.h"

#define QUICK_MATCH_TEST_TIMEOUT_MS 5000u

static uint64_t NowMs(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000ull + (uint64_t)now.tv_nsec / 1000000ull;
}

static bool ParsePort(const char* text, uint16_t* port) {
  char* end = NULL;
  const unsigned long value = strtoul(text, &end, 10);
  if ((text == end) || (*end != '\0') || (value == 0u) || (value > UINT16_MAX)) {
    return false;
  }
  *port = (uint16_t)value;
  return true;
}

static bool SendPacket(ENetPeer* peer, const void* data, size_t size) {
  ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
  if ((packet == NULL) || (enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  return true;
}

static bool SendHello(ENetPeer* peer) {
  ShroomHelloPacket hello = {.protocol_version = SHROOM_PROTOCOL_VERSION};
  ShroomPacketHeaderInit(&hello.header, SHROOM_PACKET_HELLO, sizeof(hello));
  snprintf(hello.name, sizeof(hello.name), "Quick Match Integration");
  return SendPacket(peer, &hello, sizeof(hello));
}

static bool SendLobbyListQuery(ENetPeer* peer) {
  ShroomPacketHeader query;
  ShroomPacketHeaderInit(&query, SHROOM_PACKET_LOBBY_LIST_QUERY, sizeof(query));
  return SendPacket(peer, &query, sizeof(query));
}

static bool ConnectToLobby(const ShroomQuickMatchCandidate* selected) {
  ENetAddress address = {.port = selected->port};
  ENetHost* host = NULL;
  ENetPeer* peer = NULL;
  const uint64_t deadline = NowMs() + QUICK_MATCH_TEST_TIMEOUT_MS;
  bool welcomed = false;
  bool lobby_list_received = false;

  if ((enet_initialize() != 0) || (enet_address_set_host(&address, selected->host) != 0)) {
    return false;
  }
  host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if (host != NULL) {
    peer = enet_host_connect(host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  }
  while ((peer != NULL) && !lobby_list_received && (NowMs() < deadline)) {
    ENetEvent event;
    const int serviced = enet_host_service(host, &event, 10u);
    if (serviced < 0) {
      break;
    }
    if (serviced == 0) {
      continue;
    }
    if (event.type == ENET_EVENT_TYPE_CONNECT) {
      if (!SendHello(peer)) {
        break;
      }
    } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
      if (event.packet->dataLength >= sizeof(ShroomPacketHeader)) {
        const ShroomPacketHeader* header = (const ShroomPacketHeader*)event.packet->data;
        if ((header->type == SHROOM_PACKET_WELCOME) && !welcomed) {
          welcomed = true;
          SendLobbyListQuery(peer);
        } else if (header->type == SHROOM_PACKET_LOBBY_LIST) {
          lobby_list_received = welcomed;
        }
      }
      enet_packet_destroy(event.packet);
    } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      break;
    }
  }
  if (peer != NULL) {
    enet_peer_disconnect_now(peer, 0u);
  }
  if (host != NULL) {
    enet_host_destroy(host);
  }
  enet_deinitialize();
  return welcomed && lobby_list_received;
}

int main(int argc, char** argv) {
  ShroomServerDiscovery discovery = {0};
  ShroomQuickMatchState quick_match;
  ShroomQuickMatchCandidate candidates[SHROOM_DIRECTORY_MAX_ENTRIES] = {0};
  const ShroomQuickMatchCandidate* selected;
  uint16_t directory_port;
  uint16_t loaded_port;
  uint16_t expected_port;
  const uint64_t started_ms = NowMs();
  size_t count;
  int result = 1;

  if ((argc != 5) || !ParsePort(argv[2], &directory_port) ||
      !ParsePort(argv[3], &loaded_port) || !ParsePort(argv[4], &expected_port)) {
    fprintf(stderr, "usage: %s DIRECTORY_HOST DIRECTORY_PORT LOADED_PORT EXPECTED_PORT\n", argv[0]);
    return 2;
  }
  if (!ShroomServerDiscoveryBegin(&discovery, argv[1], directory_port, started_ms)) {
    fprintf(stderr, "quick match discovery could not start\n");
    return 1;
  }
  while (ShroomServerDiscoveryIsActive(&discovery) &&
         ((NowMs() - started_ms) <= SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS + 1000ull)) {
    struct timespec delay = {.tv_nsec = 1000000};
    ShroomServerDiscoveryUpdate(&discovery, NowMs());
    nanosleep(&delay, NULL);
  }
  count = ShroomServerDiscoveryStateResultCount(&discovery.state);
  if ((discovery.state.phase != SHROOM_DISCOVERY_COMPLETE) || (count != 2u)) {
    fprintf(stderr, "quick match discovery failed: phase=%d count=%zu\n",
            (int)discovery.state.phase, count);
    goto cleanup;
  }
  for (size_t index = 0u; index < count; ++index) {
    const ShroomDiscoveryCandidate* source =
        ShroomServerDiscoveryStateResult(&discovery.state, index);
    ShroomQuickMatchCandidate* candidate = &candidates[index];
    snprintf(candidate->name, sizeof(candidate->name), "%s", source->server.name);
    snprintf(candidate->host, sizeof(candidate->host), "%s", source->server.host);
    candidate->port = source->server.port;
    candidate->capacity = source->server.capacity;
    candidate->reachable = true;
    if (candidate->port == loaded_port) {
      candidate->latency_ms = 180u;
      candidate->player_count = candidate->capacity - 1u;
    } else {
      candidate->latency_ms = 60u;
      candidate->player_count = 4u;
    }
  }
  ShroomQuickMatchBegin(&quick_match);
  if (!ShroomQuickMatchSetCandidates(&quick_match, candidates, count, NowMs())) {
    fprintf(stderr, "quick match found no joinable candidate\n");
    goto cleanup;
  }
  selected = ShroomQuickMatchSelected(&quick_match);
  if ((selected == NULL) || (selected->port != expected_port)) {
    fprintf(stderr, "quick match selected port %u instead of %u\n",
            selected != NULL ? selected->port : 0u, expected_port);
    goto cleanup;
  }
  ShroomQuickMatchUpdate(&quick_match,
                         quick_match.preview_started_ms + SHROOM_QUICK_MATCH_PREVIEW_MS);
  ShroomServerDiscoveryShutdown(&discovery);
  if ((quick_match.phase != SHROOM_QUICK_MATCH_CONNECTING) || !ConnectToLobby(selected)) {
    fprintf(stderr, "quick match did not transition into the selected server lobby\n");
    goto cleanup;
  }
  ShroomQuickMatchConnectionSucceeded(&quick_match);
  printf("quick match integration passed: selected=%s:%u phase=%d\n", selected->host,
         selected->port, (int)quick_match.phase);
  result = 0;

cleanup:
  ShroomServerDiscoveryShutdown(&discovery);
  return result;
}
