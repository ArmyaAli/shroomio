#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server_healthcheck.h"

static bool ParseUnsigned(const char* text, uint32_t minimum, uint32_t maximum, uint32_t* value) {
  char* end = NULL;
  unsigned long parsed;

  if ((text == NULL) || (text[0] == '\0')) {
    return false;
  }
  errno = 0;
  parsed = strtoul(text, &end, 10);
  if ((errno == ERANGE) || (end == text) || (*end != '\0') || (parsed < minimum) ||
      (parsed > maximum)) {
    return false;
  }
  *value = (uint32_t)parsed;
  return true;
}

bool ShroomHealthcheckLoadConfig(ShroomHealthcheckConfig* config, int argc, char** argv) {
  uint32_t parsed;

  *config = (ShroomHealthcheckConfig){
      .host = "127.0.0.1", .port = SHROOM_SERVER_PORT, .timeout_ms = 2000u};
  for (int index = 1; index < argc; ++index) {
    if ((index + 1) >= argc) {
      return false;
    }
    if (strcmp(argv[index], "--host") == 0) {
      config->host = argv[++index];
      if (config->host[0] == '\0') {
        return false;
      }
    } else if (strcmp(argv[index], "--port") == 0) {
      if (!ParseUnsigned(argv[++index], 1u, UINT16_MAX, &parsed)) {
        return false;
      }
      config->port = (uint16_t)parsed;
    } else if (strcmp(argv[index], "--timeout-ms") == 0) {
      if (!ParseUnsigned(argv[++index], 100u, 60000u, &config->timeout_ms)) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

bool ShroomHealthcheckValidateResponse(uint8_t channel, const uint8_t* data, size_t size,
                                       const ShroomServerProbeResponsePacket** response) {
  const ShroomServerProbeResponsePacket* candidate;

  if ((data == NULL) || (response == NULL) || (channel != SHROOM_ENET_CHANNEL_CONTROL) ||
      (size != sizeof(*candidate))) {
    return false;
  }
  candidate = (const ShroomServerProbeResponsePacket*)data;
  if ((candidate->header.type != SHROOM_PACKET_SERVER_PROBE_RESPONSE) ||
      ((size_t)candidate->header.size != sizeof(*candidate)) ||
      !ShroomPacketHeaderUsesExpectedChannel(&candidate->header, channel) ||
      (candidate->protocol_version != SHROOM_PROTOCOL_VERSION) ||
      (candidate->generation != SHROOM_HEALTHCHECK_GENERATION) ||
      (candidate->nonce != SHROOM_HEALTHCHECK_NONCE) || (candidate->capacity == 0u) ||
      (candidate->player_count > candidate->capacity)) {
    return false;
  }
  *response = candidate;
  return true;
}

#ifndef SHROOM_HEALTHCHECK_NO_MAIN

#include <enet/enet.h>

#include <time.h>

static uint64_t NowMs(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000ull + (uint64_t)now.tv_nsec / 1000000ull;
}

static bool SendProbe(ENetPeer* peer) {
  ShroomServerProbePacket probe = {.protocol_version = SHROOM_PROTOCOL_VERSION,
                                   .generation = SHROOM_HEALTHCHECK_GENERATION,
                                   .nonce = SHROOM_HEALTHCHECK_NONCE};
  ENetPacket* packet;

  ShroomPacketHeaderInit(&probe.header, SHROOM_PACKET_SERVER_PROBE, sizeof(probe));
  packet = enet_packet_create(&probe, sizeof(probe), ENET_PACKET_FLAG_RELIABLE);
  if ((packet == NULL) || (enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL, packet) != 0)) {
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return false;
  }
  enet_host_flush(peer->host);
  return true;
}

int main(int argc, char** argv) {
  ShroomHealthcheckConfig config;
  ENetAddress address = {0};
  ENetHost* host = NULL;
  ENetPeer* peer = NULL;
  uint64_t deadline;
  int result = 1;

  if (!ShroomHealthcheckLoadConfig(&config, argc, argv)) {
    fprintf(stderr, "usage: %s [--host HOST] [--port PORT] [--timeout-ms MS]\n", argv[0]);
    return 2;
  }
  if (enet_initialize() != 0) {
    fprintf(stderr, "unhealthy: ENet initialization failed\n");
    return 1;
  }
  host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  address.port = config.port;
  if ((host == NULL) || (enet_address_set_host(&address, config.host) != 0)) {
    fprintf(stderr, "unhealthy: cannot resolve %s:%u\n", config.host, config.port);
    goto cleanup;
  }
  peer = enet_host_connect(host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (peer == NULL) {
    fprintf(stderr, "unhealthy: cannot start probe\n");
    goto cleanup;
  }

  deadline = NowMs() + config.timeout_ms;
  while (NowMs() < deadline) {
    ENetEvent event;
    const int serviced = enet_host_service(host, &event, 50u);

    if (serviced <= 0) {
      continue;
    }
    if (event.type == ENET_EVENT_TYPE_CONNECT) {
      if (!SendProbe(peer)) {
        fprintf(stderr, "unhealthy: probe send failed\n");
        break;
      }
    } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
      const ShroomServerProbeResponsePacket* response = NULL;
      const bool healthy = ShroomHealthcheckValidateResponse(event.channelID, event.packet->data,
                                                             event.packet->dataLength, &response);
      if (healthy) {
        printf("healthy players=%u capacity=%u\n", response->player_count, response->capacity);
        result = 0;
      }
      enet_packet_destroy(event.packet);
      if (healthy) {
        break;
      }
    } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      break;
    }
  }
  if (result != 0) {
    fprintf(stderr, "unhealthy: no valid probe response from %s:%u within %u ms\n", config.host,
            config.port, config.timeout_ms);
  }

cleanup:
  if (peer != NULL) {
    enet_peer_disconnect_now(peer, 0u);
  }
  if (host != NULL) {
    enet_host_destroy(host);
  }
  enet_deinitialize();
  return result;
}

#endif
