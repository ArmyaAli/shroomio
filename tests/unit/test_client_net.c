#include "unity.h"

#include <stddef.h>
#include <string.h>

#include "client/net.h"

void setUp(void) {}
void tearDown(void) {}

static ClientNetState MakePendingPing(uint32_t nonce, uint32_t sent_time_ms) {
  ClientNetState net = {0};
  net.pending_ping_nonce = nonce;
  net.pending_ping_sent_time_ms = sent_time_ms;
  return net;
}

static void test_client_net_records_fresh_pong(void) {
  ClientNetState net = MakePendingPing(42u, 1000u);

  TEST_ASSERT_TRUE(ClientNetTestCompletePendingPing(&net, 42u, 1120u));

  TEST_ASSERT_EQUAL_UINT32(0u, net.pending_ping_nonce);
  TEST_ASSERT_EQUAL_UINT32(120u, net.rtt_ms);
  TEST_ASSERT_EQUAL_UINT32(120u, net.rtt_average_ms);
  TEST_ASSERT_EQUAL_UINT32(1u, net.rtt_sample_count);
}

static void test_client_net_ignores_stale_pong_sample(void) {
  ClientNetState net = MakePendingPing(7u, 1000u);
  net.rtt_ms = 80u;
  net.rtt_average_ms = 80u;
  net.rtt_sample_count = 1u;
  net.rtt_samples[0] = 80u;

  TEST_ASSERT_FALSE(
      ClientNetTestCompletePendingPing(&net, 7u, 1000u + SHROOM_CLIENT_PING_TIMEOUT_MS));

  TEST_ASSERT_EQUAL_UINT32(0u, net.pending_ping_nonce);
  TEST_ASSERT_EQUAL_UINT32(80u, net.rtt_ms);
  TEST_ASSERT_EQUAL_UINT32(80u, net.rtt_average_ms);
  TEST_ASSERT_EQUAL_UINT32(1u, net.rtt_sample_count);
}

static void test_client_net_clears_timed_out_pending_ping(void) {
  ClientNetState net = MakePendingPing(9u, 5000u);

  ClientNetTestClearStalePendingPing(&net, 5000u + SHROOM_CLIENT_PING_TIMEOUT_MS);

  TEST_ASSERT_EQUAL_UINT32(0u, net.pending_ping_nonce);
}

static void test_client_net_accepts_trimmed_snapshot_packet(void) {
  ClientNetState net = {0};
  ShroomSnapshotPacket snapshot = {0};
  ENetPacket packet = {0};
  const size_t packet_size =
      offsetof(ShroomSnapshotPacket, players) + sizeof(ShroomSnapshotPlayerState);

  snapshot.tick = 123u;
  snapshot.last_processed_input_sequence = 77u;
  snapshot.player_count = 1u;
  snapshot.players[0].player_id = 42u;
  snapshot.players[0].entity_id = 43u;
  snapshot.players[0].position_x = 10.0f;
  snapshot.players[0].position_y = 20.0f;
  snapshot.players[0].mass = 30.0f;
  snapshot.players[0].radius = 4.0f;
  snapshot.players[0].alive = 1u;

  packet.data = (enet_uint8*)&snapshot;
  packet.dataLength = packet_size;

  ClientNetTestHandleSnapshot(&net, &packet);

  TEST_ASSERT_EQUAL_UINT64(123u, net.last_snapshot_tick);
  TEST_ASSERT_EQUAL_UINT32(77u, net.last_processed_input_sequence);
  TEST_ASSERT_EQUAL_UINT16(1u, net.snapshot_player_count);
  TEST_ASSERT_EQUAL_UINT32(42u, net.snapshot_players[0].player_id);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, net.snapshot_players[0].position_x);
}

static void test_client_net_ignores_truncated_snapshot_players(void) {
  ClientNetState net = {0};
  ShroomSnapshotPacket snapshot = {0};
  ENetPacket packet = {0};
  const size_t packet_size = offsetof(ShroomSnapshotPacket, players);

  net.snapshot_player_count = 9u;
  snapshot.tick = 123u;
  snapshot.player_count = 1u;
  packet.data = (enet_uint8*)&snapshot;
  packet.dataLength = packet_size;

  ClientNetTestHandleSnapshot(&net, &packet);

  TEST_ASSERT_EQUAL_UINT16(9u, net.snapshot_player_count);
}

static void test_client_net_accepts_trimmed_lobby_list_packet(void) {
  ClientNetState net = {0};
  ShroomLobbyListPacket list = {0};
  ENetPacket packet = {0};
  const size_t packet_size = offsetof(ShroomLobbyListPacket, lobbies) + sizeof(ShroomLobbyEntry);

  list.lobby_count = 1u;
  list.lobbies[0].lobby_id = 77u;
  list.lobbies[0].player_count = 3u;
  list.lobbies[0].max_players = 16u;
  strncpy(list.lobbies[0].name, "Loopback", sizeof(list.lobbies[0].name) - 1u);
  packet.data = (enet_uint8*)&list;
  packet.dataLength = packet_size;

  ClientNetTestHandleLobbyList(&net, &packet);

  TEST_ASSERT_EQUAL_UINT8(1u, net.lobby_count);
  TEST_ASSERT_EQUAL_UINT32(77u, net.lobby_list[0].lobby_id);
  TEST_ASSERT_EQUAL_STRING("Loopback", net.lobby_list[0].name);
}

static void test_client_net_ignores_truncated_lobby_list_entries(void) {
  ClientNetState net = {0};
  ShroomLobbyListPacket list = {0};
  ENetPacket packet = {0};

  net.lobby_count = 2u;
  list.lobby_count = 1u;
  packet.data = (enet_uint8*)&list;
  packet.dataLength = offsetof(ShroomLobbyListPacket, lobbies);

  ClientNetTestHandleLobbyList(&net, &packet);

  TEST_ASSERT_EQUAL_UINT8(2u, net.lobby_count);
}

static void test_client_net_accepts_trimmed_mushroom_species_catalog_packet(void) {
  ClientNetState net = {0};
  ShroomMushroomSpeciesCatalogPacket catalog = {0};
  ENetPacket packet = {0};
  const size_t packet_size = offsetof(ShroomMushroomSpeciesCatalogPacket, species) +
                             sizeof(ShroomMushroomSpeciesEntry);

  catalog.species_count = 1u;
  catalog.species[0].species_id = 5u;
  catalog.species[0].pattern_id = 2u;
  strncpy(catalog.species[0].name, "Amanita", sizeof(catalog.species[0].name) - 1u);
  packet.data = (enet_uint8*)&catalog;
  packet.dataLength = packet_size;

  ClientNetTestHandleMushroomSpeciesCatalog(&net, &packet);

  TEST_ASSERT_TRUE(net.mushroom_species_catalog_received);
  TEST_ASSERT_EQUAL_UINT8(1u, net.mushroom_species_count);
  TEST_ASSERT_EQUAL_UINT8(5u, net.mushroom_species[0].species_id);
  TEST_ASSERT_EQUAL_STRING("Amanita", net.mushroom_species[0].name);
}

static void test_client_net_ignores_truncated_mushroom_species_entries(void) {
  ClientNetState net = {0};
  ShroomMushroomSpeciesCatalogPacket catalog = {0};
  ENetPacket packet = {0};

  net.mushroom_species_count = 3u;
  catalog.species_count = 1u;
  packet.data = (enet_uint8*)&catalog;
  packet.dataLength = offsetof(ShroomMushroomSpeciesCatalogPacket, species);

  ClientNetTestHandleMushroomSpeciesCatalog(&net, &packet);

  TEST_ASSERT_FALSE(net.mushroom_species_catalog_received);
  TEST_ASSERT_EQUAL_UINT8(3u, net.mushroom_species_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_client_net_records_fresh_pong);
  RUN_TEST(test_client_net_ignores_stale_pong_sample);
  RUN_TEST(test_client_net_clears_timed_out_pending_ping);
  RUN_TEST(test_client_net_accepts_trimmed_snapshot_packet);
  RUN_TEST(test_client_net_ignores_truncated_snapshot_players);
  RUN_TEST(test_client_net_accepts_trimmed_lobby_list_packet);
  RUN_TEST(test_client_net_ignores_truncated_lobby_list_entries);
  RUN_TEST(test_client_net_accepts_trimmed_mushroom_species_catalog_packet);
  RUN_TEST(test_client_net_ignores_truncated_mushroom_species_entries);
  return UNITY_END();
}
