#include "unity.h"

#include <string.h>

#include "server/directory_registry.h"

void setUp(void) {}
void tearDown(void) {}

static ShroomDirectoryHeartbeatPacket Heartbeat(uint64_t server_id, const char* host,
                                                uint16_t port) {
  ShroomDirectoryHeartbeatPacket heartbeat = {0};

  ShroomPacketHeaderInit(&heartbeat.header, SHROOM_PACKET_DIRECTORY_HEARTBEAT,
                         sizeof(heartbeat));
  heartbeat.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
  heartbeat.server.server_id = server_id;
  snprintf(heartbeat.server.name, sizeof(heartbeat.server.name), "Server %llu",
           (unsigned long long)server_id);
  snprintf(heartbeat.server.host, sizeof(heartbeat.server.host), "%s", host);
  heartbeat.server.port = port;
  heartbeat.server.player_count = 3u;
  heartbeat.server.capacity = 64u;
  return heartbeat;
}

static size_t ActiveCount(const ShroomDirectoryRegistry* registry) {
  ShroomDirectoryServerEntry entries[SHROOM_DIRECTORY_MAX_ENTRIES];
  return ShroomDirectoryRegistryCopyActive(registry, entries, SHROOM_DIRECTORY_MAX_ENTRIES);
}

static void test_valid_registration_is_copied(void) {
  ShroomDirectoryRegistry registry;
  ShroomDirectoryHeartbeatPacket heartbeat = Heartbeat(42u, "game.example", 7777u);
  ShroomDirectoryServerEntry entry = {0};

  ShroomDirectoryRegistryInit(&registry);
  TEST_ASSERT_TRUE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 1000u));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryRegistryCopyActive(&registry, &entry, 1u));
  TEST_ASSERT_EQUAL_UINT64(42u, entry.server_id);
  TEST_ASSERT_EQUAL_STRING("game.example", entry.host);
  TEST_ASSERT_EQUAL_UINT16(3u, entry.player_count);
}

static void test_refresh_updates_registration_and_extends_expiry(void) {
  ShroomDirectoryRegistry registry;
  ShroomDirectoryHeartbeatPacket heartbeat = Heartbeat(8u, "one.example", 7001u);
  ShroomDirectoryServerEntry entry = {0};

  ShroomDirectoryRegistryInit(&registry);
  TEST_ASSERT_TRUE(ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 0u));
  heartbeat.server.player_count = 9u;
  TEST_ASSERT_TRUE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 14000u));
  heartbeat.server.player_count = 2u;
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 13000u));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomDirectoryRegistryEvictExpired(&registry, 15000u));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryRegistryCopyActive(&registry, &entry, 1u));
  TEST_ASSERT_EQUAL_UINT16(9u, entry.player_count);
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryRegistryEvictExpired(&registry, 29000u));
}

static void test_server_id_and_endpoint_are_deduplicated(void) {
  ShroomDirectoryRegistry registry;
  ShroomDirectoryHeartbeatPacket first = Heartbeat(1u, "same.example", 7777u);
  ShroomDirectoryHeartbeatPacket moved = Heartbeat(1u, "moved.example", 8888u);
  ShroomDirectoryHeartbeatPacket replacement = Heartbeat(2u, "moved.example", 8888u);
  ShroomDirectoryServerEntry entry = {0};

  ShroomDirectoryRegistryInit(&registry);
  TEST_ASSERT_TRUE(ShroomDirectoryRegistryRegister(&registry, &first, sizeof(first), 1u));
  TEST_ASSERT_TRUE(ShroomDirectoryRegistryRegister(&registry, &moved, sizeof(moved), 2u));
  TEST_ASSERT_TRUE(
      ShroomDirectoryRegistryRegister(&registry, &replacement, sizeof(replacement), 3u));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryRegistryCopyActive(&registry, &entry, 1u));
  TEST_ASSERT_EQUAL_UINT64(2u, entry.server_id);
  TEST_ASSERT_EQUAL_STRING("moved.example", entry.host);
}

static void test_moving_id_onto_registered_endpoint_removes_duplicate(void) {
  ShroomDirectoryRegistry registry;
  ShroomDirectoryHeartbeatPacket first = Heartbeat(1u, "one.example", 7777u);
  ShroomDirectoryHeartbeatPacket second = Heartbeat(2u, "two.example", 8888u);
  ShroomDirectoryHeartbeatPacket moved = Heartbeat(1u, "two.example", 8888u);
  ShroomDirectoryServerEntry entry = {0};

  ShroomDirectoryRegistryInit(&registry);
  TEST_ASSERT_TRUE(ShroomDirectoryRegistryRegister(&registry, &first, sizeof(first), 1u));
  TEST_ASSERT_TRUE(ShroomDirectoryRegistryRegister(&registry, &second, sizeof(second), 2u));
  TEST_ASSERT_TRUE(ShroomDirectoryRegistryRegister(&registry, &moved, sizeof(moved), 3u));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryRegistryCopyActive(&registry, &entry, 1u));
  TEST_ASSERT_EQUAL_UINT64(1u, entry.server_id);
  TEST_ASSERT_EQUAL_STRING("two.example", entry.host);
}

static void test_capacity_is_clamped_and_impossible_load_rejected(void) {
  ShroomDirectoryRegistry registry;
  ShroomDirectoryHeartbeatPacket heartbeat = Heartbeat(4u, "large.example", 7777u);
  ShroomDirectoryServerEntry entry = {0};

  ShroomDirectoryRegistryInit(&registry);
  heartbeat.server.capacity = UINT16_MAX;
  heartbeat.server.player_count = 100u;
  TEST_ASSERT_TRUE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 100u));
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryRegistryCopyActive(&registry, &entry, 1u));
  TEST_ASSERT_EQUAL_UINT16(SHROOM_SERVER_MAX_CLIENTS, entry.capacity);

  heartbeat.server.server_id = 5u;
  heartbeat.server.player_count = SHROOM_SERVER_MAX_CLIENTS + 1u;
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 101u));
  TEST_ASSERT_EQUAL_size_t(1u, ActiveCount(&registry));
}

static void test_malformed_stale_and_wrong_version_packets_are_rejected(void) {
  ShroomDirectoryRegistry registry;
  ShroomDirectoryHeartbeatPacket heartbeat = Heartbeat(6u, "valid.example", 7777u);

  ShroomDirectoryRegistryInit(&registry);
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat) - 1u, 0u));
  heartbeat.protocol_version += 1u;
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 0u));
  heartbeat.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
  memset(heartbeat.server.host, 'x', sizeof(heartbeat.server.host));
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 0u));
  heartbeat = Heartbeat(6u, "valid.example", 7777u);
  snprintf(heartbeat.server.host, sizeof(heartbeat.server.host), "%s", "bad host.example");
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 0u));
  heartbeat = Heartbeat(6u, "0.0.0.0", 7777u);
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 0u));
  heartbeat = Heartbeat(6u, "valid.example", 7777u);
  heartbeat.server.player_count = 65u;
  heartbeat.server.capacity = 64u;
  TEST_ASSERT_FALSE(
      ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), 0u));
  TEST_ASSERT_EQUAL_size_t(0u, ActiveCount(&registry));
}

static void test_registry_is_bounded_to_32_entries(void) {
  ShroomDirectoryRegistry registry;

  ShroomDirectoryRegistryInit(&registry);
  for (uint64_t index = 0u; index < SHROOM_DIRECTORY_MAX_ENTRIES; ++index) {
    char host[32];
    ShroomDirectoryHeartbeatPacket heartbeat;
    snprintf(host, sizeof(host), "server-%llu.test", (unsigned long long)index);
    heartbeat = Heartbeat(index + 1u, host, (uint16_t)(7000u + index));
    TEST_ASSERT_TRUE(
        ShroomDirectoryRegistryRegister(&registry, &heartbeat, sizeof(heartbeat), index));
  }
  {
    ShroomDirectoryHeartbeatPacket overflow = Heartbeat(1000u, "overflow.test", 9000u);
    TEST_ASSERT_FALSE(
        ShroomDirectoryRegistryRegister(&registry, &overflow, sizeof(overflow), 100u));
  }
  TEST_ASSERT_EQUAL_size_t(SHROOM_DIRECTORY_MAX_ENTRIES, ActiveCount(&registry));
}

static void test_query_validation_requires_current_version_and_generation(void) {
  ShroomDirectoryQueryPacket query = {0};

  ShroomPacketHeaderInit(&query.header, SHROOM_PACKET_DIRECTORY_QUERY, sizeof(query));
  query.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
  query.generation = 17u;
  TEST_ASSERT_TRUE(ShroomDirectoryQueryIsValid(&query, sizeof(query)));
  TEST_ASSERT_FALSE(ShroomDirectoryQueryIsValid(&query, sizeof(query) - 1u));
  query.generation = 0u;
  TEST_ASSERT_FALSE(ShroomDirectoryQueryIsValid(&query, sizeof(query)));
  query.generation = 17u;
  query.protocol_version += 1u;
  TEST_ASSERT_FALSE(ShroomDirectoryQueryIsValid(&query, sizeof(query)));
}

static void test_list_builder_chunks_32_entries_and_preserves_generation(void) {
  ShroomDirectoryServerEntry entries[SHROOM_DIRECTORY_MAX_ENTRIES] = {0};
  ShroomDirectoryListPacket packet;
  size_t packet_size;

  for (size_t index = 0u; index < SHROOM_DIRECTORY_MAX_ENTRIES; ++index) {
    entries[index].server_id = index + 1u;
  }
  TEST_ASSERT_EQUAL_size_t(1u, ShroomDirectoryListChunkCount(0u));
  TEST_ASSERT_EQUAL_size_t(5u, ShroomDirectoryListChunkCount(SHROOM_DIRECTORY_MAX_ENTRIES));
  packet_size = ShroomDirectoryBuildListPacket(entries, SHROOM_DIRECTORY_MAX_ENTRIES, 77u, 4u,
                                                &packet);
  TEST_ASSERT_EQUAL_size_t(SHROOM_DIRECTORY_LIST_PACKET_SIZE(4u), packet_size);
  TEST_ASSERT_TRUE(packet_size <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE);
  TEST_ASSERT_EQUAL_UINT32(77u, packet.generation);
  TEST_ASSERT_EQUAL_UINT8(4u, packet.chunk_index);
  TEST_ASSERT_EQUAL_UINT8(5u, packet.chunk_count);
  TEST_ASSERT_EQUAL_UINT8(4u, packet.entry_count);
  TEST_ASSERT_EQUAL_UINT64(29u, packet.entries[0].server_id);
  TEST_ASSERT_EQUAL_UINT64(32u, packet.entries[3].server_id);
  TEST_ASSERT_EQUAL_size_t(0u, ShroomDirectoryBuildListPacket(
                                   entries, SHROOM_DIRECTORY_MAX_ENTRIES, 0u, 0u, &packet));
  TEST_ASSERT_EQUAL_size_t(0u, ShroomDirectoryBuildListPacket(
                                   entries, SHROOM_DIRECTORY_MAX_ENTRIES, 1u, 5u, &packet));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_valid_registration_is_copied);
  RUN_TEST(test_refresh_updates_registration_and_extends_expiry);
  RUN_TEST(test_server_id_and_endpoint_are_deduplicated);
  RUN_TEST(test_moving_id_onto_registered_endpoint_removes_duplicate);
  RUN_TEST(test_capacity_is_clamped_and_impossible_load_rejected);
  RUN_TEST(test_malformed_stale_and_wrong_version_packets_are_rejected);
  RUN_TEST(test_registry_is_bounded_to_32_entries);
  RUN_TEST(test_query_validation_requires_current_version_and_generation);
  RUN_TEST(test_list_builder_chunks_32_entries_and_preserves_generation);
  return UNITY_END();
}
