#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <enet/enet.h>

#include "shared/protocol.h"

static uint64_t NowMs(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return ((uint64_t)now.tv_sec * 1000ull) + (uint64_t)now.tv_nsec / 1000000ull;
}

static bool ParseUnsigned(const char* text, unsigned long long* value) {
  char* end = NULL;
  *value = strtoull(text, &end, 10);
  return (end != text) && (*end == '\0');
}

int main(int argc, char** argv) {
  ENetAddress address = {0};
  ENetHost* host = NULL;
  ENetPeer* peer = NULL;
  unsigned long long port_value = 0u;
  unsigned long long expected_count = 0u;
  unsigned long long expected_id = 0u;
  bool expect_id = false;
  uint64_t deadline;
  int result = 1;

  if ((argc != 4) && (argc != 6)) {
    fprintf(stderr, "usage: %s HOST PORT EXPECTED_COUNT [--expect-id ID]\n", argv[0]);
    return 1;
  }
  if (!ParseUnsigned(argv[2], &port_value) || (port_value == 0u) || (port_value > 65535u) ||
      !ParseUnsigned(argv[3], &expected_count) ||
      (expected_count > SHROOM_DIRECTORY_MAX_ENTRIES)) {
    fprintf(stderr, "invalid numeric argument\n");
    return 1;
  }
  if (argc == 6) {
    if ((strcmp(argv[4], "--expect-id") != 0) || !ParseUnsigned(argv[5], &expected_id) ||
        (expected_id == 0u)) {
      fprintf(stderr, "invalid expected server id\n");
      return 1;
    }
    expect_id = true;
  }
  if (enet_initialize() != 0) {
    return 1;
  }
  host = enet_host_create(NULL, 1u, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  address.port = (enet_uint16)port_value;
  if ((host == NULL) || (enet_address_set_host(&address, argv[1]) != 0)) {
    goto cleanup;
  }
  peer = enet_host_connect(host, &address, SHROOM_ENET_CHANNEL_COUNT, 0u);
  if (peer == NULL) {
    goto cleanup;
  }

  deadline = NowMs() + 7000ull;
  while (NowMs() < deadline) {
    ENetEvent event;
    if (enet_host_service(host, &event, 50u) <= 0) {
      continue;
    }
    if (event.type == ENET_EVENT_TYPE_CONNECT) {
      ShroomDirectoryQueryPacket query = {0};
      ENetPacket* packet;
      ShroomPacketHeaderInit(&query.header, SHROOM_PACKET_DIRECTORY_QUERY, sizeof(query));
      query.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
      query.generation = 0x494u;
      packet = enet_packet_create(&query, sizeof(query), ENET_PACKET_FLAG_RELIABLE);
      if ((packet == NULL) ||
          (enet_peer_send(peer, SHROOM_ENET_CHANNEL_CONTROL, packet) != 0)) {
        if (packet != NULL) {
          enet_packet_destroy(packet);
        }
        goto cleanup;
      }
      enet_host_flush(host);
    } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
      const ShroomDirectoryListPacket* list =
          (const ShroomDirectoryListPacket*)event.packet->data;
      if ((event.channelID == SHROOM_ENET_CHANNEL_CONTROL) &&
          (event.packet->dataLength >= SHROOM_DIRECTORY_LIST_HEADER_SIZE) &&
          (list->header.type == SHROOM_PACKET_DIRECTORY_LIST) &&
          (list->header.size == event.packet->dataLength) &&
          (list->protocol_version == SHROOM_DIRECTORY_PROTOCOL_VERSION) &&
          (list->generation == 0x494u) && (list->chunk_index == 0u) &&
          (list->chunk_count == 1u) &&
          (event.packet->dataLength == SHROOM_DIRECTORY_LIST_PACKET_SIZE(list->entry_count)) &&
          (list->entry_count == expected_count)) {
        bool found_id = !expect_id;
        bool entries_valid = true;
        for (size_t index = 0u; index < list->entry_count; ++index) {
          const ShroomDirectoryServerEntry* entry = &list->entries[index];
          printf("%llu %s %s:%u %u/%u\n", (unsigned long long)entry->server_id, entry->name,
                 entry->host, entry->port, entry->player_count, entry->capacity);
          if (entry->server_id == expected_id) {
            found_id = true;
          }
          if ((entry->player_count != 0u) || (entry->capacity != SHROOM_SERVER_MAX_CLIENTS)) {
            entries_valid = false;
          }
        }
        result = (found_id && entries_valid) ? 0 : 1;
      }
      enet_packet_destroy(event.packet);
      if (result == 0) {
        break;
      }
    } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
      break;
    }
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
