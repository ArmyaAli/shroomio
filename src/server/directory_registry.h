#ifndef SHROOM_DIRECTORY_REGISTRY_H
#define SHROOM_DIRECTORY_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_DIRECTORY_ENTRY_TTL_MS 15000ull

typedef struct ShroomDirectoryRegistration {
  ShroomDirectoryServerEntry server;
  uint64_t last_seen_ms;
  bool active;
} ShroomDirectoryRegistration;

typedef struct ShroomDirectoryRegistry {
  ShroomDirectoryRegistration registrations[SHROOM_DIRECTORY_MAX_ENTRIES];
} ShroomDirectoryRegistry;

void ShroomDirectoryRegistryInit(ShroomDirectoryRegistry* registry);
bool ShroomDirectoryRegistryRegister(ShroomDirectoryRegistry* registry,
                                     const ShroomDirectoryHeartbeatPacket* heartbeat,
                                     size_t packet_size, uint64_t now_ms);
size_t ShroomDirectoryRegistryEvictExpired(ShroomDirectoryRegistry* registry, uint64_t now_ms);
size_t ShroomDirectoryRegistryCopyActive(const ShroomDirectoryRegistry* registry,
                                         ShroomDirectoryServerEntry* entries, size_t capacity);
bool ShroomDirectoryQueryIsValid(const ShroomDirectoryQueryPacket* query, size_t packet_size);
size_t ShroomDirectoryListChunkCount(size_t entry_count);
size_t ShroomDirectoryBuildListPacket(const ShroomDirectoryServerEntry* entries, size_t entry_count,
                                      uint32_t generation, size_t chunk_index,
                                      ShroomDirectoryListPacket* packet);

#endif
