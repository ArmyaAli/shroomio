#include "unity.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "client/net.h"
#include "shared/world.h"

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
  snapshot.game_mode = SHROOM_GAME_MODE_KING_OF_HILL;
  snapshot.objective_target_score = SHROOM_KOTH_TARGET_SCORE;
  snapshot.objective_controller_id = 42u;
  snapshot.objective_contested = 0u;
  snapshot.players[0].player_id = 42u;
  snapshot.players[0].entity_id = 43u;
  snapshot.players[0].position_x = 10.0f;
  snapshot.players[0].position_y = 20.0f;
  snapshot.players[0].mass = 30.0f;
  snapshot.players[0].radius = 4.0f;
  snapshot.players[0].alive = 1u;
  snapshot.players[0].objective_score = 12.5f;

  packet.data = (enet_uint8*)&snapshot;
  packet.dataLength = packet_size;

  ClientNetTestHandleSnapshot(&net, &packet);

  TEST_ASSERT_EQUAL_UINT64(123u, net.last_snapshot_tick);
  TEST_ASSERT_EQUAL_UINT32(77u, net.last_processed_input_sequence);
  TEST_ASSERT_EQUAL_UINT16(1u, net.snapshot_player_count);
  TEST_ASSERT_EQUAL_UINT32(42u, net.snapshot_players[0].player_id);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, net.snapshot_players[0].position_x);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_GAME_MODE_KING_OF_HILL, net.game_mode);
  TEST_ASSERT_EQUAL_FLOAT(SHROOM_KOTH_TARGET_SCORE, net.objective_target_score);
  TEST_ASSERT_EQUAL_UINT32(42u, net.objective_controller_id);
  TEST_ASSERT_FALSE(net.objective_contested);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, net.snapshot_players[0].objective_score);
}

static void test_hello_uses_configured_player_name(void) {
  ClientNetState net = {0};
  ShroomHelloPacket hello = {0};
  snprintf(net.player_name, sizeof(net.player_name), "%s", "  Moss@@  Runner!! ");

  ClientNetTestBuildHello(&net, &hello);

  TEST_ASSERT_EQUAL_UINT8(SHROOM_PACKET_HELLO, hello.header.type);
  TEST_ASSERT_EQUAL_UINT16(sizeof(hello), hello.header.size);
  TEST_ASSERT_EQUAL_STRING("Moss Runner", hello.name);
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

static void test_client_net_ignores_duplicate_and_out_of_order_snapshots(void) {
  ClientNetState net = {0};
  ShroomSnapshotPacket snapshot = {0};
  ENetPacket packet = {
      .data = (enet_uint8*)&snapshot,
      .dataLength = offsetof(ShroomSnapshotPacket, players) + sizeof(snapshot.players[0]),
  };

  snapshot.tick = 20u;
  snapshot.player_count = 1u;
  snapshot.players[0].position_x = 200.0f;
  ClientNetTestHandleSnapshot(&net, &packet);
  TEST_ASSERT_TRUE(net.snapshot_received);
  TEST_ASSERT_EQUAL_FLOAT(200.0f, net.snapshot_players[0].position_x);

  snapshot.tick = 19u;
  snapshot.players[0].position_x = 19.0f;
  ClientNetTestHandleSnapshot(&net, &packet);
  TEST_ASSERT_EQUAL_UINT64(20u, net.last_snapshot_tick);
  TEST_ASSERT_EQUAL_FLOAT(200.0f, net.snapshot_players[0].position_x);

  snapshot.tick = 20u;
  snapshot.players[0].position_x = 20.0f;
  ClientNetTestHandleSnapshot(&net, &packet);
  TEST_ASSERT_EQUAL_FLOAT(200.0f, net.snapshot_players[0].position_x);
}

static void test_client_net_accepts_chunked_spore_state_packet(void) {
  ClientNetState net = {0};
  ShroomSporeStatePacket spore_state = {0};
  ENetPacket packet = {0};
  const size_t packet_size =
      offsetof(ShroomSporeStatePacket, spores) + (2u * sizeof(ShroomSnapshotSporeState));

  ShroomPacketHeaderInit(&spore_state.header, SHROOM_PACKET_SPORE_STATE, (uint16_t)packet_size);
  spore_state.tick = 55u;
  spore_state.spore_count = 5u;
  spore_state.reserved = 3u;
  spore_state.spores[0] = (ShroomSnapshotSporeState){.entity_id = 40u, .position_x = 4.0f};
  spore_state.spores[1] = (ShroomSnapshotSporeState){.entity_id = 50u, .position_x = 5.0f};
  packet.data = (enet_uint8*)&spore_state;
  packet.dataLength = packet_size;

  ClientNetTestHandleSporeState(&net, &packet);

  TEST_ASSERT_EQUAL_UINT16(5u, net.spore_count);
  TEST_ASSERT_EQUAL_UINT32(40u, net.snapshot_spores[3].entity_id);
  TEST_ASSERT_EQUAL_UINT32(50u, net.snapshot_spores[4].entity_id);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, net.snapshot_spores[4].position_x);
}

static void test_client_net_ignores_misaligned_spore_state_packet(void) {
  ClientNetState net = {0};
  ShroomSporeStatePacket spore_state = {0};
  ENetPacket packet = {0};

  net.spore_count = 7u;
  spore_state.spore_count = 1u;
  packet.data = (enet_uint8*)&spore_state;
  packet.dataLength = offsetof(ShroomSporeStatePacket, spores) + 1u;

  ClientNetTestHandleSporeState(&net, &packet);

  TEST_ASSERT_EQUAL_UINT16(7u, net.spore_count);
}

static void test_client_net_zero_spore_state_clears_stale_spores(void) {
  ClientNetState net = {0};
  ShroomSporeStatePacket spore_state = {0};
  ENetPacket packet = {0};

  net.spore_count = 3u;
  net.snapshot_spores[0].entity_id = 99u;
  ShroomPacketHeaderInit(&spore_state.header, SHROOM_PACKET_SPORE_STATE,
                         (uint16_t)offsetof(ShroomSporeStatePacket, spores));
  packet.data = (enet_uint8*)&spore_state;
  packet.dataLength = offsetof(ShroomSporeStatePacket, spores);

  ClientNetTestHandleSporeState(&net, &packet);

  TEST_ASSERT_EQUAL_UINT16(0u, net.spore_count);
  TEST_ASSERT_EQUAL_UINT32(0u, net.snapshot_spores[0].entity_id);
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
  const size_t packet_size =
      offsetof(ShroomMushroomSpeciesCatalogPacket, species) + sizeof(ShroomMushroomSpeciesEntry);

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

static void test_client_net_connect_timeout_flips_to_friendly_error(void) {
  ClientNetState net = {0};
  net.status = CLIENT_NET_CONNECTING;
  net.connect_started_ms = 1000u;

  ClientNetTestCheckConnectTimeout(&net, 1000u + SHROOM_CLIENT_CONNECT_TIMEOUT_MS);

  TEST_ASSERT_EQUAL_UINT32(CLIENT_NET_ERROR, net.status);
  TEST_ASSERT_EQUAL_STRING(SHROOM_NET_CONNECT_UNREACHABLE_MSG, net.status_text);
  /* The timer is cleared so a later frame can't re-fire the transition. */
  TEST_ASSERT_EQUAL_UINT32(0u, net.connect_started_ms);
}

static void test_client_net_connect_timeout_not_fired_stays_connecting(void) {
  ClientNetState net = {0};
  net.status = CLIENT_NET_CONNECTING;
  net.connect_started_ms = 1000u;

  /* One ms short of the timeout window — still connecting. */
  ClientNetTestCheckConnectTimeout(&net, 1000u + SHROOM_CLIENT_CONNECT_TIMEOUT_MS - 1u);

  TEST_ASSERT_EQUAL_UINT32(CLIENT_NET_CONNECTING, net.status);
  TEST_ASSERT_EQUAL_UINT32(1000u, net.connect_started_ms);
}

static void test_client_net_connect_timeout_ignores_non_connecting_states(void) {
  ClientNetState net = {0};
  /* An established session must never be flagged as a connect timeout. */
  net.status = CLIENT_NET_CONNECTED;
  net.connect_started_ms = 1000u;

  ClientNetTestCheckConnectTimeout(&net, 1000u + SHROOM_CLIENT_CONNECT_TIMEOUT_MS);

  TEST_ASSERT_EQUAL_UINT32(CLIENT_NET_CONNECTED, net.status);
}

static void test_client_net_shutdown_clears_session_state(void) {
  ClientNetState net = {0};

  net.status = CLIENT_NET_ERROR;
  net.welcome_received = true;
  net.handshake_received = true;
  net.player_id = 12u;
  net.entity_id = 34u;
  net.lobby_id = 56u;
  net.lobby_count = 1u;
  net.pending_ping_nonce = 78u;
  snprintf(net.status_text, sizeof(net.status_text), "%s", "stale error");

  ClientNetShutdown(&net);

  TEST_ASSERT_EQUAL_UINT32(CLIENT_NET_DISCONNECTED, net.status);
  TEST_ASSERT_FALSE(net.welcome_received);
  TEST_ASSERT_FALSE(net.handshake_received);
  TEST_ASSERT_EQUAL_UINT32(0u, net.player_id);
  TEST_ASSERT_EQUAL_UINT32(0u, net.entity_id);
  TEST_ASSERT_EQUAL_UINT32(0u, net.lobby_id);
  TEST_ASSERT_EQUAL_UINT8(0u, net.lobby_count);
  TEST_ASSERT_EQUAL_UINT32(0u, net.pending_ping_nonce);
  TEST_ASSERT_EQUAL_CHAR('\0', net.status_text[0]);
}

static void test_client_net_accepts_authoritative_lobby_roster(void) {
  ClientNetState net = {.lobby_id = 9u};
  ShroomLobbyRosterPacket roster = {0};
  ENetPacket packet = {0};
  const size_t packet_size =
      offsetof(ShroomLobbyRosterPacket, players) + sizeof(ShroomLobbyRosterEntry);
  roster.lobby_id = 9u;
  roster.player_count = 1u;
  roster.match_started = 1u;
  roster.players[0] = (ShroomLobbyRosterEntry){.player_id = 42u, .is_ready = 1u};
  packet.data = (enet_uint8*)&roster;
  packet.dataLength = packet_size;

  ClientNetTestHandleLobbyRoster(&net, &packet);

  TEST_ASSERT_TRUE(net.lobby_roster_received);
  TEST_ASSERT_TRUE(net.lobby_match_started);
  TEST_ASSERT_EQUAL_UINT16(1u, net.lobby_roster_count);
  TEST_ASSERT_EQUAL_UINT32(42u, net.lobby_roster[0].player_id);
}

static void test_client_net_rejects_invalid_lobby_roster(void) {
  ClientNetState net = {.lobby_id = 9u};
  ShroomLobbyRosterPacket roster = {.lobby_id = 8u, .player_count = 1u};
  ENetPacket packet = {.data = (enet_uint8*)&roster,
                       .dataLength = offsetof(ShroomLobbyRosterPacket, players)};
  ClientNetTestHandleLobbyRoster(&net, &packet);
  TEST_ASSERT_FALSE(net.lobby_roster_received);

  roster.lobby_id = 9u;
  ClientNetTestHandleLobbyRoster(&net, &packet);
  TEST_ASSERT_FALSE(net.lobby_roster_received);
}

static void test_gameplay_input_requires_explicit_match_entry(void) {
  ENetPeer peer = {.state = ENET_PEER_STATE_CONNECTED};
  ClientNetState net = {
      .peer = &peer,
      .welcome_received = true,
      .match_phase = SHROOM_MATCH_PHASE_RUNNING,
  };
  TEST_ASSERT_FALSE(ClientNetTestCanSendGameplayInput(&net));
  net.match_entry_sent = true;
  TEST_ASSERT_TRUE(ClientNetTestCanSendGameplayInput(&net));
}

static void test_lobby_session_resume_requires_live_connection_and_identity(void) {
  ENetHost host = {0};
  ENetPeer peer = {0};
  ClientNetState net = {
      .host = &host,
      .peer = &peer,
      .status = CLIENT_NET_CONNECTED,
      .enet_initialized = true,
      .welcome_received = true,
      .match_entry_sent = true,
      .lobby_id = 9u,
      .player_id = 42u,
      .entity_id = 84u,
  };

  TEST_ASSERT_TRUE(ClientNetCanResumeLobbySession(&net));

  net.peer = NULL;
  TEST_ASSERT_FALSE(ClientNetCanResumeLobbySession(&net));
  net.peer = &peer;
  net.match_entry_sent = false;
  TEST_ASSERT_FALSE(ClientNetCanResumeLobbySession(&net));
  net.match_entry_sent = true;
  net.entity_id = 0u;
  TEST_ASSERT_FALSE(ClientNetCanResumeLobbySession(&net));
}

static void test_spectator_lobby_session_can_resume_without_player_identity(void) {
  ENetHost host = {0};
  ENetPeer peer = {0};
  ClientNetState net = {
      .host = &host,
      .peer = &peer,
      .status = CLIENT_NET_CONNECTED,
      .enet_initialized = true,
      .spectating = true,
      .match_entry_sent = true,
      .lobby_id = 9u,
  };

  TEST_ASSERT_TRUE(ClientNetCanResumeLobbySession(&net));
}

static void test_client_net_accepts_authoritative_intermission_status(void) {
  ClientNetState net = {0};
  ShroomIntermissionStatusPacket status = {.round_id = 12u,
                                           .seconds_remaining = 9.0f,
                                           .eligible_count = 3u,
                                           .play_again_votes = 2u,
                                           .your_vote = SHROOM_REMATCH_VOTE_PLAY_AGAIN,
                                           .can_vote = 1u};
  ENetPacket packet = {.data = (enet_uint8*)&status, .dataLength = sizeof(status)};

  ClientNetTestHandleIntermissionStatus(&net, &packet);
  TEST_ASSERT_TRUE(net.intermission_received);
  TEST_ASSERT_EQUAL_UINT32(12u, net.intermission.round_id);
  TEST_ASSERT_EQUAL_UINT16(2u, net.intermission.play_again_votes);
  TEST_ASSERT_EQUAL(SHROOM_REMATCH_VOTE_PLAY_AGAIN, net.intermission.your_vote);
}

static void test_client_net_rejects_truncated_intermission_status(void) {
  ClientNetState net = {0};
  ShroomIntermissionStatusPacket status = {0};
  ENetPacket packet = {.data = (enet_uint8*)&status, .dataLength = sizeof(status) - 1u};

  ClientNetTestHandleIntermissionStatus(&net, &packet);
  TEST_ASSERT_FALSE(net.intermission_received);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_client_net_records_fresh_pong);
  RUN_TEST(test_client_net_ignores_stale_pong_sample);
  RUN_TEST(test_client_net_clears_timed_out_pending_ping);
  RUN_TEST(test_client_net_accepts_trimmed_snapshot_packet);
  RUN_TEST(test_hello_uses_configured_player_name);
  RUN_TEST(test_client_net_ignores_truncated_snapshot_players);
  RUN_TEST(test_client_net_ignores_duplicate_and_out_of_order_snapshots);
  RUN_TEST(test_client_net_accepts_chunked_spore_state_packet);
  RUN_TEST(test_client_net_ignores_misaligned_spore_state_packet);
  RUN_TEST(test_client_net_zero_spore_state_clears_stale_spores);
  RUN_TEST(test_client_net_accepts_trimmed_lobby_list_packet);
  RUN_TEST(test_client_net_ignores_truncated_lobby_list_entries);
  RUN_TEST(test_client_net_accepts_trimmed_mushroom_species_catalog_packet);
  RUN_TEST(test_client_net_ignores_truncated_mushroom_species_entries);
  RUN_TEST(test_client_net_connect_timeout_flips_to_friendly_error);
  RUN_TEST(test_client_net_connect_timeout_not_fired_stays_connecting);
  RUN_TEST(test_client_net_connect_timeout_ignores_non_connecting_states);
  RUN_TEST(test_client_net_shutdown_clears_session_state);
  RUN_TEST(test_client_net_accepts_authoritative_lobby_roster);
  RUN_TEST(test_client_net_rejects_invalid_lobby_roster);
  RUN_TEST(test_gameplay_input_requires_explicit_match_entry);
  RUN_TEST(test_lobby_session_resume_requires_live_connection_and_identity);
  RUN_TEST(test_spectator_lobby_session_can_resume_without_player_identity);
  RUN_TEST(test_client_net_accepts_authoritative_intermission_status);
  RUN_TEST(test_client_net_rejects_truncated_intermission_status);
  return UNITY_END();
}
