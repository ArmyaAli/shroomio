#include "directory_registry.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool IsTerminatedPrintableText(const char* text, size_t capacity) {
  bool has_visible = false;

  for (size_t index = 0u; index < capacity; ++index) {
    const unsigned char character = (unsigned char)text[index];
    if (character == '\0') {
      return has_visible;
    }
    if (!isprint(character)) {
      return false;
    }
    if (!isspace(character)) {
      has_visible = true;
    }
  }
  return false;
}

static bool IsValidAdvertisedHost(const char* host, size_t capacity) {
  size_t length = 0u;

  while ((length < capacity) && (host[length] != '\0')) {
    const unsigned char character = (unsigned char)host[length];
    if (!isalnum(character) && (character != '.') && (character != '-') && (character != ':')) {
      return false;
    }
    ++length;
  }
  if ((length == 0u) || (length == capacity)) {
    return false;
  }
  return (strcmp(host, "0.0.0.0") != 0) && (strcmp(host, "::") != 0);
}

static bool EndpointMatches(const ShroomDirectoryServerEntry* left,
                            const ShroomDirectoryServerEntry* right) {
  return (left->port == right->port) && (strncmp(left->host, right->host, sizeof(left->host)) == 0);
}

static uint64_t StableEndpointId(const char* host, uint16_t port) {
  uint64_t hash = 1469598103934665603ull;

  for (const unsigned char* cursor = (const unsigned char*)host; *cursor != '\0'; ++cursor) {
    hash = (hash ^ *cursor) * 1099511628211ull;
  }
  hash = (hash ^ (uint8_t)(port >> 8u)) * 1099511628211ull;
  hash = (hash ^ (uint8_t)port) * 1099511628211ull;
  return hash == 0ull ? 1ull : hash;
}

static bool ValidateAndClampEntry(ShroomDirectoryServerEntry* entry) {
  if ((entry->server_id == 0ull) || !IsTerminatedPrintableText(entry->name, sizeof(entry->name)) ||
      !IsValidAdvertisedHost(entry->host, sizeof(entry->host)) || (entry->port == 0u) ||
      (entry->capacity == 0u) || (entry->player_count > entry->capacity) ||
      (entry->player_count > SHROOM_SERVER_MAX_CLIENTS)) {
    return false;
  }

  if (entry->capacity > SHROOM_SERVER_MAX_CLIENTS) {
    entry->capacity = SHROOM_SERVER_MAX_CLIENTS;
  }
  entry->reserved = 0u;
  return true;
}

void ShroomDirectoryRegistryInit(ShroomDirectoryRegistry* registry) {
  if (registry != NULL) {
    memset(registry, 0, sizeof(*registry));
  }
}

bool ShroomDirectoryRegistryRegister(ShroomDirectoryRegistry* registry,
                                     const ShroomDirectoryHeartbeatPacket* heartbeat,
                                     size_t packet_size, const char* observed_host,
                                     uint64_t now_ms) {
  ShroomDirectoryServerEntry entry;
  size_t id_match = SHROOM_DIRECTORY_MAX_ENTRIES;
  size_t endpoint_match = SHROOM_DIRECTORY_MAX_ENTRIES;
  size_t available = SHROOM_DIRECTORY_MAX_ENTRIES;
  size_t target = SHROOM_DIRECTORY_MAX_ENTRIES;

  if ((registry == NULL) || (heartbeat == NULL) || (observed_host == NULL) ||
      (packet_size != sizeof(ShroomDirectoryHeartbeatPacket)) ||
      (heartbeat->header.type != SHROOM_PACKET_DIRECTORY_HEARTBEAT) ||
      (heartbeat->header.size != sizeof(ShroomDirectoryHeartbeatPacket)) ||
      (heartbeat->protocol_version != SHROOM_DIRECTORY_PROTOCOL_VERSION)) {
    return false;
  }

  entry = heartbeat->server;
  const int host_length = snprintf(entry.host, sizeof(entry.host), "%s", observed_host);
  if ((host_length < 0) || ((size_t)host_length >= sizeof(entry.host))) {
    return false;
  }
  entry.server_id = StableEndpointId(entry.host, entry.port);
  if (!ValidateAndClampEntry(&entry)) {
    return false;
  }

  for (size_t index = 0u; index < SHROOM_DIRECTORY_MAX_ENTRIES; ++index) {
    ShroomDirectoryRegistration* registration = &registry->registrations[index];
    if (registration->active) {
      if (registration->server.server_id == entry.server_id) {
        id_match = index;
      }
      if (EndpointMatches(&registration->server, &entry)) {
        endpoint_match = index;
      }
    } else if (available == SHROOM_DIRECTORY_MAX_ENTRIES) {
      available = index;
    }
  }
  if ((id_match != SHROOM_DIRECTORY_MAX_ENTRIES) &&
      (now_ms < registry->registrations[id_match].last_seen_ms)) {
    return false;
  }
  if ((endpoint_match != SHROOM_DIRECTORY_MAX_ENTRIES) &&
      (now_ms < registry->registrations[endpoint_match].last_seen_ms)) {
    return false;
  }
  target = id_match != SHROOM_DIRECTORY_MAX_ENTRIES ? id_match : endpoint_match;
  if (target == SHROOM_DIRECTORY_MAX_ENTRIES) {
    target = available;
  }
  if (target == SHROOM_DIRECTORY_MAX_ENTRIES) {
    return false;
  }
  if ((id_match != SHROOM_DIRECTORY_MAX_ENTRIES) &&
      (endpoint_match != SHROOM_DIRECTORY_MAX_ENTRIES) && (id_match != endpoint_match)) {
    registry->registrations[endpoint_match] = (ShroomDirectoryRegistration){0};
  }

  registry->registrations[target].server = entry;
  registry->registrations[target].last_seen_ms = now_ms;
  registry->registrations[target].active = true;
  return true;
}

size_t ShroomDirectoryRegistryEvictExpired(ShroomDirectoryRegistry* registry, uint64_t now_ms) {
  size_t evicted = 0u;

  if (registry == NULL) {
    return 0u;
  }
  for (size_t index = 0u; index < SHROOM_DIRECTORY_MAX_ENTRIES; ++index) {
    ShroomDirectoryRegistration* registration = &registry->registrations[index];
    if (registration->active && (now_ms >= registration->last_seen_ms) &&
        ((now_ms - registration->last_seen_ms) >= SHROOM_DIRECTORY_ENTRY_TTL_MS)) {
      *registration = (ShroomDirectoryRegistration){0};
      ++evicted;
    }
  }
  return evicted;
}

size_t ShroomDirectoryRegistryCopyActive(const ShroomDirectoryRegistry* registry,
                                         ShroomDirectoryServerEntry* entries, size_t capacity) {
  size_t count = 0u;

  if ((registry == NULL) || (entries == NULL)) {
    return 0u;
  }
  if (capacity > SHROOM_DIRECTORY_MAX_ENTRIES) {
    capacity = SHROOM_DIRECTORY_MAX_ENTRIES;
  }
  for (size_t index = 0u; (index < SHROOM_DIRECTORY_MAX_ENTRIES) && (count < capacity); ++index) {
    if (registry->registrations[index].active) {
      entries[count++] = registry->registrations[index].server;
    }
  }
  return count;
}

bool ShroomDirectoryQueryIsValid(const ShroomDirectoryQueryPacket* query, size_t packet_size) {
  return (query != NULL) && (packet_size == sizeof(*query)) &&
         (query->header.type == SHROOM_PACKET_DIRECTORY_QUERY) &&
         (query->header.size == sizeof(*query)) &&
         (query->protocol_version == SHROOM_DIRECTORY_PROTOCOL_VERSION) &&
         (query->generation != 0u);
}

size_t ShroomDirectoryListChunkCount(size_t entry_count) {
  if (entry_count > SHROOM_DIRECTORY_MAX_ENTRIES) {
    entry_count = SHROOM_DIRECTORY_MAX_ENTRIES;
  }
  return entry_count == 0u ? 1u
                           : (entry_count + SHROOM_DIRECTORY_ENTRIES_PER_PACKET - 1u) /
                                 SHROOM_DIRECTORY_ENTRIES_PER_PACKET;
}

size_t ShroomDirectoryBuildListPacket(const ShroomDirectoryServerEntry* entries, size_t entry_count,
                                      uint32_t generation, size_t chunk_index,
                                      ShroomDirectoryListPacket* packet) {
  const size_t chunk_count = ShroomDirectoryListChunkCount(entry_count);
  const size_t start = chunk_index * SHROOM_DIRECTORY_ENTRIES_PER_PACKET;
  size_t count;
  size_t packet_size;

  if ((packet == NULL) || (generation == 0u) || (entry_count > SHROOM_DIRECTORY_MAX_ENTRIES) ||
      (chunk_index >= chunk_count) || ((entry_count > 0u) && (entries == NULL))) {
    return 0u;
  }
  count = entry_count > start ? entry_count - start : 0u;
  if (count > SHROOM_DIRECTORY_ENTRIES_PER_PACKET) {
    count = SHROOM_DIRECTORY_ENTRIES_PER_PACKET;
  }
  packet_size = SHROOM_DIRECTORY_LIST_PACKET_SIZE(count);
  *packet = (ShroomDirectoryListPacket){0};
  ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_DIRECTORY_LIST, (uint16_t)packet_size);
  packet->protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
  packet->generation = generation;
  packet->chunk_index = (uint8_t)chunk_index;
  packet->chunk_count = (uint8_t)chunk_count;
  packet->entry_count = (uint8_t)count;
  if (count > 0u) {
    memcpy(packet->entries, &entries[start], count * sizeof(entries[0]));
  }
  return packet_size;
}
