#include "unity.h"
#include "../src/shared/protocol.h"
#include <limits.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

void test_packet_header_size(void) { TEST_ASSERT_EQUAL(4, sizeof(ShroomPacketHeader)); }

void test_hello_packet_size(void) {
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 32, sizeof(ShroomHelloPacket));
}

void test_welcome_packet_size(void) {
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 4 + 4 + 2 + 2 + 4 + 4,
                    sizeof(ShroomWelcomePacket));
}

void test_input_packet_size(void) {
  /* header + sequence + movement direction + split direction + split flag/reserved + focused id */
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 4 + 4 + 4 + 4 + 1 + 3 + 4,
                    sizeof(ShroomInputPacket));
}

void test_ping_packet_size(void) {
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4, sizeof(ShroomPingPacket));
}

void test_pong_packet_size(void) {
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4, sizeof(ShroomPongPacket));
}

void test_voice_frame_packet_size(void) {
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 2 + 2 + 512, sizeof(ShroomVoiceFramePacket));
}

void test_snapshot_player_state_size(void) {
  TEST_ASSERT_EQUAL_INT(72, (int)sizeof(ShroomSnapshotPlayerState));
}

void test_participant_and_entity_capacities_are_separate_and_bounded(void) {
  TEST_ASSERT_EQUAL_UINT32(64u, SHROOM_MAX_PARTICIPANTS);
  TEST_ASSERT_EQUAL_UINT32(4u, SHROOM_MAX_SPLIT_PIECES);
  TEST_ASSERT_EQUAL_UINT32(256u, SHROOM_MAX_PLAYER_ENTITIES);
  TEST_ASSERT_EQUAL_UINT32(SHROOM_MAX_PLAYER_ENTITIES, SHROOM_MAX_SNAPSHOT_PLAYERS);
  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_PARTICIPANTS, sizeof(((ShroomLobbyRosterPacket*)0)->players) /
                                                        sizeof(ShroomLobbyRosterEntry));
  TEST_ASSERT_EQUAL_size_t(SHROOM_MAX_PLAYER_ENTITIES, sizeof(((ShroomSnapshotPacket*)0)->players) /
                                                           sizeof(ShroomSnapshotPlayerState));
  TEST_ASSERT_TRUE(SHROOM_MAX_SNAPSHOT_PACKET_SIZE <= UINT16_MAX);
}

void test_packet_type_values(void) {
  TEST_ASSERT_EQUAL(1, SHROOM_PACKET_HELLO);
  TEST_ASSERT_EQUAL(2, SHROOM_PACKET_WELCOME);
  TEST_ASSERT_EQUAL(3, SHROOM_PACKET_INPUT);
  TEST_ASSERT_EQUAL(4, SHROOM_PACKET_SNAPSHOT);
  TEST_ASSERT_EQUAL(5, SHROOM_PACKET_PING);
  TEST_ASSERT_EQUAL(6, SHROOM_PACKET_PONG);
  TEST_ASSERT_EQUAL(7, SHROOM_PACKET_SPORE_STATE);
  TEST_ASSERT_EQUAL(11, SHROOM_PACKET_LOBBY_LIST_QUERY);
  TEST_ASSERT_EQUAL(12, SHROOM_PACKET_LOBBY_LIST);
  TEST_ASSERT_EQUAL(13, SHROOM_PACKET_LOBBY_JOIN);
  TEST_ASSERT_EQUAL(14, SHROOM_PACKET_LOBBY_JOINED);
  TEST_ASSERT_EQUAL(15, SHROOM_PACKET_LOBBY_LEAVE);
  TEST_ASSERT_EQUAL(16, SHROOM_PACKET_LOBBY_CREATE);
  TEST_ASSERT_EQUAL(17, SHROOM_PACKET_LOBBY_CREATED);
  TEST_ASSERT_EQUAL(18, SHROOM_PACKET_POWERUP_STATE);
  TEST_ASSERT_EQUAL(19, SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG);
  TEST_ASSERT_EQUAL(20, SHROOM_PACKET_VOICE_FRAME);
  TEST_ASSERT_EQUAL(21, SHROOM_PACKET_READY_STATE);
  TEST_ASSERT_EQUAL(22, SHROOM_PACKET_ENTER_MATCH);
  TEST_ASSERT_EQUAL(23, SHROOM_PACKET_LOBBY_ROSTER);
  TEST_ASSERT_EQUAL(24, SHROOM_PACKET_REMATCH_VOTE);
  TEST_ASSERT_EQUAL(25, SHROOM_PACKET_INTERMISSION_STATUS);
}

void test_lobby_packet_channel_mapping(void) {
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_LIST_QUERY));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_LIST));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_JOIN));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_JOINED));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_LEAVE));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_CREATE));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_CREATED));
}

void test_lobby_packet_reliability(void) {
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_LIST_QUERY));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_LIST));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_JOIN));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_JOINED));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_LEAVE));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_CREATE));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_CREATED));
}

void test_lobby_entry_struct(void) {
  ShroomLobbyEntry entry;
  memset(&entry, 0, sizeof(entry));

  entry.lobby_id = 1;
  strncpy(entry.name, "Arena 1", SHROOM_LOBBY_MAX_NAME_LENGTH);
  entry.player_count = 3;
  entry.bot_count = 15;
  entry.max_players = 28;
  entry.spectator_count = 1;
  entry.is_dynamic = 0;
  entry.game_mode = SHROOM_GAME_MODE_KING_OF_HILL;

  TEST_ASSERT_EQUAL(1, entry.lobby_id);
  TEST_ASSERT_EQUAL_STRING("Arena 1", entry.name);
  TEST_ASSERT_EQUAL(3, entry.player_count);
  TEST_ASSERT_EQUAL(15, entry.bot_count);
  TEST_ASSERT_EQUAL(28, entry.max_players);
  TEST_ASSERT_EQUAL(1, entry.spectator_count);
  TEST_ASSERT_EQUAL(0, entry.is_dynamic);
  TEST_ASSERT_EQUAL(SHROOM_GAME_MODE_KING_OF_HILL, entry.game_mode);
}

void test_lobby_join_packet_initialization(void) {
  ShroomLobbyJoinPacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOIN, sizeof(packet));
  packet.lobby_id = 2;
  packet.spectate = 1;

  TEST_ASSERT_EQUAL(SHROOM_PACKET_LOBBY_JOIN, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL, packet.header.reserved);
  TEST_ASSERT_EQUAL(2, packet.lobby_id);
  TEST_ASSERT_EQUAL(1, packet.spectate);
}

void test_lobby_joined_packet_initialization(void) {
  ShroomLobbyJoinedPacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_JOINED, sizeof(packet));
  packet.lobby_id = 3;
  packet.player_id = 42;
  packet.entity_id = 100;
  packet.spectating = 0;
  packet.game_mode = SHROOM_GAME_MODE_KING_OF_HILL;
  strncpy(packet.lobby_name, "Arena 3", SHROOM_LOBBY_MAX_NAME_LENGTH);
  packet.server_tick_rate = 30;
  packet.snapshot_rate = 15;
  packet.world_width = 6000.0f;
  packet.world_height = 6000.0f;

  TEST_ASSERT_EQUAL(SHROOM_PACKET_LOBBY_JOINED, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL, packet.header.reserved);
  TEST_ASSERT_EQUAL(3, packet.lobby_id);
  TEST_ASSERT_EQUAL(42, packet.player_id);
  TEST_ASSERT_EQUAL(0, packet.spectating);
  TEST_ASSERT_EQUAL(SHROOM_GAME_MODE_KING_OF_HILL, packet.game_mode);
  TEST_ASSERT_EQUAL_STRING("Arena 3", packet.lobby_name);
  TEST_ASSERT_EQUAL(30, packet.server_tick_rate);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6000.0f, packet.world_width);
}

void test_lobby_list_packet_capacity(void) {
  ShroomLobbyListPacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_LOBBY_LIST, sizeof(packet));
  packet.lobby_count = SHROOM_MAX_LOBBIES;

  TEST_ASSERT_EQUAL(SHROOM_PACKET_LOBBY_LIST, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_MAX_LOBBIES, packet.lobby_count);
  /* Verify the lobbies array is within the packet. */
  TEST_ASSERT_TRUE(sizeof(packet.lobbies) == SHROOM_MAX_LOBBIES * sizeof(ShroomLobbyEntry));
}

void test_lobby_config_constants(void) {
  TEST_ASSERT_EQUAL(8, SHROOM_MAX_LOBBIES);
  TEST_ASSERT_EQUAL(4, SHROOM_LOBBY_DEFAULT_COUNT);
  TEST_ASSERT_EQUAL(32, SHROOM_LOBBY_MAX_NAME_LENGTH);
  TEST_ASSERT_TRUE(SHROOM_LOBBY_DEFAULT_COUNT <= SHROOM_MAX_LOBBIES);
}

void test_protocol_constants(void) {
  TEST_ASSERT_EQUAL(7777, SHROOM_SERVER_PORT);
  TEST_ASSERT_EQUAL(8, SHROOM_PROTOCOL_VERSION);
  TEST_ASSERT_EQUAL(32, SHROOM_MAX_NAME_LENGTH);
  TEST_ASSERT_EQUAL(15, SHROOM_SNAPSHOT_RATE);
  TEST_ASSERT_EQUAL(256, SHROOM_MAX_SNAPSHOT_PLAYERS);
  TEST_ASSERT_EQUAL(5, SHROOM_SPORE_STATE_RATE);
  TEST_ASSERT_EQUAL(0, SHROOM_ENET_CHANNEL_CONTROL);
  TEST_ASSERT_EQUAL(1, SHROOM_ENET_CHANNEL_SNAPSHOT);
  TEST_ASSERT_EQUAL(2, SHROOM_ENET_CHANNEL_INPUT);
  TEST_ASSERT_EQUAL(3, SHROOM_ENET_CHANNEL_CHAT);
  TEST_ASSERT_EQUAL(4, SHROOM_ENET_CHANNEL_VOICE);
  TEST_ASSERT_EQUAL(5, SHROOM_ENET_CHANNEL_COUNT);
}

void test_packet_channel_mapping(void) {
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL, ShroomPacketTypeToChannel(SHROOM_PACKET_HELLO));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL, ShroomPacketTypeToChannel(SHROOM_PACKET_WELCOME));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_INPUT, ShroomPacketTypeToChannel(SHROOM_PACKET_INPUT));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_SNAPSHOT,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_SNAPSHOT));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_SNAPSHOT,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_SPORE_STATE));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_SNAPSHOT,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_POWERUP_STATE));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CHAT, ShroomPacketTypeToChannel(SHROOM_PACKET_CHAT));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_VOICE,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_VOICE_FRAME));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_AUTH_RESPONSE));
}

void test_packet_reliability_mapping(void) {
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_HELLO));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_WELCOME));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_PING));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_AUTH_REQUEST));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_CHAT));
  TEST_ASSERT_FALSE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_INPUT));
  TEST_ASSERT_FALSE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_SNAPSHOT));
  TEST_ASSERT_FALSE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_SPORE_STATE));
  TEST_ASSERT_FALSE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_POWERUP_STATE));
  TEST_ASSERT_FALSE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_VOICE_FRAME));
}

void test_packet_minimum_size_mapping(void) {
  TEST_ASSERT_EQUAL(sizeof(ShroomHelloPacket), ShroomPacketTypeMinimumSize(SHROOM_PACKET_HELLO));
  TEST_ASSERT_EQUAL(sizeof(ShroomWelcomePacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_WELCOME));
  TEST_ASSERT_EQUAL(sizeof(ShroomInputPacket), ShroomPacketTypeMinimumSize(SHROOM_PACKET_INPUT));
  TEST_ASSERT_EQUAL(offsetof(ShroomSnapshotPacket, players),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_SNAPSHOT));
  TEST_ASSERT_EQUAL(sizeof(ShroomPingPacket), ShroomPacketTypeMinimumSize(SHROOM_PACKET_PING));
  TEST_ASSERT_EQUAL(sizeof(ShroomPongPacket), ShroomPacketTypeMinimumSize(SHROOM_PACKET_PONG));
  TEST_ASSERT_EQUAL(offsetof(ShroomSporeStatePacket, spores),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_SPORE_STATE));
  TEST_ASSERT_EQUAL(sizeof(ShroomAuthRequestPacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_AUTH_REQUEST));
  TEST_ASSERT_EQUAL(sizeof(ShroomAuthResponsePacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_AUTH_RESPONSE));
  TEST_ASSERT_EQUAL(sizeof(ShroomChatPacket), ShroomPacketTypeMinimumSize(SHROOM_PACKET_CHAT));
  TEST_ASSERT_EQUAL(sizeof(ShroomVoiceFramePacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_VOICE_FRAME));
  TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_LIST_QUERY));
  TEST_ASSERT_EQUAL(offsetof(ShroomLobbyListPacket, lobbies),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_LIST));
  TEST_ASSERT_EQUAL(sizeof(ShroomLobbyJoinPacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_JOIN));
  TEST_ASSERT_EQUAL(sizeof(ShroomLobbyJoinedPacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_JOINED));
  TEST_ASSERT_EQUAL(sizeof(ShroomLobbyLeavePacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_LEAVE));
  TEST_ASSERT_EQUAL(sizeof(ShroomLobbyCreatePacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_CREATE));
  TEST_ASSERT_EQUAL(sizeof(ShroomLobbyCreatedPacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_CREATED));
  TEST_ASSERT_EQUAL(offsetof(ShroomPowerupStatePacket, powerups),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_POWERUP_STATE));
  TEST_ASSERT_EQUAL(offsetof(ShroomMushroomSpeciesCatalogPacket, species),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_MUSHROOM_SPECIES_CATALOG));
  TEST_ASSERT_EQUAL(sizeof(ShroomEnterMatchPacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_ENTER_MATCH));
  TEST_ASSERT_EQUAL(offsetof(ShroomLobbyRosterPacket, players),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_LOBBY_ROSTER));
}

void test_match_entry_packets_are_reliable_control_messages(void) {
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_ENTER_MATCH));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_LOBBY_ROSTER));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_ENTER_MATCH));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_LOBBY_ROSTER));
}

void test_voice_packet_initialization(void) {
  ShroomVoiceFramePacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_VOICE_FRAME, sizeof(packet));
  packet.player_id = 4;
  packet.payload_size = 128;
  packet.payload[0] = 0xABu;

  TEST_ASSERT_EQUAL(SHROOM_PACKET_VOICE_FRAME, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_VOICE, packet.header.reserved);
  TEST_ASSERT_EQUAL(sizeof(packet), packet.header.size);
  TEST_ASSERT_EQUAL(4, packet.player_id);
  TEST_ASSERT_EQUAL(128, packet.payload_size);
  TEST_ASSERT_EQUAL_HEX8(0xAB, packet.payload[0]);
}

void test_packet_header_initializes_channel_metadata(void) {
  ShroomPacketHeader header = {0};

  ShroomPacketHeaderInit(&header, SHROOM_PACKET_INPUT, sizeof(ShroomInputPacket));

  TEST_ASSERT_EQUAL(SHROOM_PACKET_INPUT, header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_INPUT, header.reserved);
  TEST_ASSERT_EQUAL(sizeof(ShroomInputPacket), header.size);
}

void test_packet_header_validates_expected_channel(void) {
  ShroomPacketHeader header = {0};

  ShroomPacketHeaderInit(&header, SHROOM_PACKET_SNAPSHOT, sizeof(ShroomSnapshotPacket));

  TEST_ASSERT_TRUE(ShroomPacketHeaderUsesExpectedChannel(&header, SHROOM_ENET_CHANNEL_SNAPSHOT));
  TEST_ASSERT_FALSE(ShroomPacketHeaderUsesExpectedChannel(&header, SHROOM_ENET_CHANNEL_INPUT));

  header.reserved = SHROOM_ENET_CHANNEL_CONTROL;
  TEST_ASSERT_FALSE(ShroomPacketHeaderUsesExpectedChannel(&header, SHROOM_ENET_CHANNEL_SNAPSHOT));
}

void test_snapshot_spore_state_size(void) {
  TEST_ASSERT_EQUAL(4 + 4 + 4 + 2 + 2, sizeof(ShroomSnapshotSporeState));
}

void test_spore_state_packet_initialization(void) {
  ShroomSporeStatePacket packet;
  uint16_t actual_size =
      (uint16_t)(sizeof(ShroomPacketHeader) + sizeof(uint64_t) + sizeof(uint16_t) +
                 sizeof(uint16_t) + 3 * sizeof(ShroomSnapshotSporeState));
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_SPORE_STATE, actual_size);
  packet.tick = 100;
  packet.spore_count = 3;

  packet.spores[0] = (ShroomSnapshotSporeState){
      .entity_id = 1, .position_x = 100.0f, .position_y = 200.0f, .value = 8};
  packet.spores[1] = (ShroomSnapshotSporeState){
      .entity_id = 2, .position_x = 300.0f, .position_y = 400.0f, .value = 8};
  packet.spores[2] = (ShroomSnapshotSporeState){
      .entity_id = 3, .position_x = 500.0f, .position_y = 600.0f, .value = 8};

  TEST_ASSERT_EQUAL(SHROOM_PACKET_SPORE_STATE, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_SNAPSHOT, packet.header.reserved);
  TEST_ASSERT_EQUAL(100, packet.tick);
  TEST_ASSERT_EQUAL(3, packet.spore_count);
  TEST_ASSERT_EQUAL(1, packet.spores[0].entity_id);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, packet.spores[0].position_x);
  TEST_ASSERT_EQUAL(8, packet.spores[0].value);
  TEST_ASSERT_EQUAL(3, packet.spores[2].entity_id);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, packet.spores[2].position_y);
}

void test_spore_state_packet_chunk_size_is_bounded(void) {
  const uint16_t max_spores = ShroomSporeStatePacketMaxSpores();
  const size_t packet_size = offsetof(ShroomSporeStatePacket, spores) +
                             ((size_t)max_spores * sizeof(ShroomSnapshotSporeState));
  const size_t next_packet_size = packet_size + sizeof(ShroomSnapshotSporeState);

  TEST_ASSERT_TRUE(max_spores > 0u);
  TEST_ASSERT_TRUE(packet_size <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE);
  TEST_ASSERT_TRUE(next_packet_size > SHROOM_MAX_UNRELIABLE_PACKET_SIZE);
  TEST_ASSERT_TRUE(packet_size < 65536u);
}

void test_snapshot_powerup_state_size(void) {
  TEST_ASSERT_EQUAL(4 + 4 + 4 + 1 + 1 + 2, sizeof(ShroomSnapshotPowerupState));
}

void test_powerup_state_packet_initialization(void) {
  ShroomPowerupStatePacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_POWERUP_STATE, sizeof(packet));
  packet.tick = 42;
  packet.powerup_count = 2;
  packet.powerups[0] = (ShroomSnapshotPowerupState){
      .entity_id = 5,
      .position_x = 120.0f,
      .position_y = 240.0f,
      .type = SHROOM_POWERUP_TYPE_SPEED,
      .active = 1,
  };
  packet.powerups[1] = (ShroomSnapshotPowerupState){
      .entity_id = 6,
      .position_x = 320.0f,
      .position_y = 640.0f,
      .type = SHROOM_POWERUP_TYPE_SHIELD,
      .active = 0,
  };

  TEST_ASSERT_EQUAL(SHROOM_PACKET_POWERUP_STATE, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_SNAPSHOT, packet.header.reserved);
  TEST_ASSERT_EQUAL(42, packet.tick);
  TEST_ASSERT_EQUAL(2, packet.powerup_count);
  TEST_ASSERT_EQUAL(5, packet.powerups[0].entity_id);
  TEST_ASSERT_EQUAL(SHROOM_POWERUP_TYPE_SPEED, packet.powerups[0].type);
  TEST_ASSERT_EQUAL(1, packet.powerups[0].active);
  TEST_ASSERT_EQUAL(SHROOM_POWERUP_TYPE_SHIELD, packet.powerups[1].type);
  TEST_ASSERT_EQUAL(0, packet.powerups[1].active);
}

void test_hello_packet_initialization(void) {
  ShroomHelloPacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_HELLO, sizeof(ShroomHelloPacket));
  packet.protocol_version = SHROOM_PROTOCOL_VERSION;
  strncpy(packet.name, "TestPlayer", SHROOM_MAX_NAME_LENGTH);

  TEST_ASSERT_EQUAL(SHROOM_PACKET_HELLO, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL, packet.header.reserved);
  TEST_ASSERT_EQUAL(sizeof(ShroomHelloPacket), packet.header.size);
  TEST_ASSERT_EQUAL(SHROOM_PROTOCOL_VERSION, packet.protocol_version);
  TEST_ASSERT_EQUAL_STRING("TestPlayer", packet.name);
}

void test_input_packet_initialization(void) {
  ShroomInputPacket packet;
  memset(&packet, 0, sizeof(packet));

  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_INPUT, sizeof(ShroomInputPacket));
  packet.sequence = 42;
  packet.direction_x = 0.5f;
  packet.direction_y = -0.5f;
  packet.split_direction_x = 1.0f;
  packet.split_direction_y = 0.25f;

  TEST_ASSERT_EQUAL(SHROOM_PACKET_INPUT, packet.header.type);
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_INPUT, packet.header.reserved);
  TEST_ASSERT_EQUAL(sizeof(ShroomInputPacket), packet.header.size);
  TEST_ASSERT_EQUAL(42, packet.sequence);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, packet.direction_x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, packet.direction_y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, packet.split_direction_x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.25f, packet.split_direction_y);
}

void test_snapshot_player_state_initialization(void) {
  ShroomSnapshotPlayerState state;
  memset(&state, 0, sizeof(state));

  state.player_id = 1;
  state.entity_id = 100;
  state.position_x = 1000.0f;
  state.position_y = 2000.0f;
  state.mass = 150.0f;
  state.radius = 20.0f;
  strncpy(state.name, "ArenaScout", SHROOM_MAX_NAME_LENGTH);
  state.alive = 1;
  state.is_bot = 0;
  state.objective_score = 42.5f;

  TEST_ASSERT_EQUAL(1, state.player_id);
  TEST_ASSERT_EQUAL(100, state.entity_id);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1000.0f, state.position_x);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2000.0f, state.position_y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, state.mass);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, state.radius);
  TEST_ASSERT_EQUAL_STRING("ArenaScout", state.name);
  TEST_ASSERT_EQUAL(1, state.alive);
  TEST_ASSERT_EQUAL(0, state.is_bot);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.5f, state.objective_score);
}

void test_intermission_packets_are_reliable_control_messages(void) {
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_REMATCH_VOTE));
  TEST_ASSERT_EQUAL(SHROOM_ENET_CHANNEL_CONTROL,
                    ShroomPacketTypeToChannel(SHROOM_PACKET_INTERMISSION_STATUS));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_REMATCH_VOTE));
  TEST_ASSERT_TRUE(ShroomPacketTypeUsesReliableDelivery(SHROOM_PACKET_INTERMISSION_STATUS));
  TEST_ASSERT_EQUAL(sizeof(ShroomRematchVotePacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_REMATCH_VOTE));
  TEST_ASSERT_EQUAL(sizeof(ShroomIntermissionStatusPacket),
                    ShroomPacketTypeMinimumSize(SHROOM_PACKET_INTERMISSION_STATUS));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_packet_header_size);
  RUN_TEST(test_hello_packet_size);
  RUN_TEST(test_welcome_packet_size);
  RUN_TEST(test_input_packet_size);
  RUN_TEST(test_ping_packet_size);
  RUN_TEST(test_pong_packet_size);
  RUN_TEST(test_voice_frame_packet_size);
  RUN_TEST(test_snapshot_player_state_size);
  RUN_TEST(test_participant_and_entity_capacities_are_separate_and_bounded);
  RUN_TEST(test_packet_type_values);
  RUN_TEST(test_protocol_constants);
  RUN_TEST(test_packet_channel_mapping);
  RUN_TEST(test_packet_reliability_mapping);
  RUN_TEST(test_packet_minimum_size_mapping);
  RUN_TEST(test_match_entry_packets_are_reliable_control_messages);
  RUN_TEST(test_packet_header_initializes_channel_metadata);
  RUN_TEST(test_packet_header_validates_expected_channel);
  RUN_TEST(test_hello_packet_initialization);
  RUN_TEST(test_input_packet_initialization);
  RUN_TEST(test_voice_packet_initialization);
  RUN_TEST(test_snapshot_player_state_initialization);
  RUN_TEST(test_intermission_packets_are_reliable_control_messages);
  RUN_TEST(test_snapshot_spore_state_size);
  RUN_TEST(test_spore_state_packet_initialization);
  RUN_TEST(test_spore_state_packet_chunk_size_is_bounded);
  RUN_TEST(test_snapshot_powerup_state_size);
  RUN_TEST(test_powerup_state_packet_initialization);
  RUN_TEST(test_lobby_packet_channel_mapping);
  RUN_TEST(test_lobby_packet_reliability);
  RUN_TEST(test_lobby_entry_struct);
  RUN_TEST(test_lobby_join_packet_initialization);
  RUN_TEST(test_lobby_joined_packet_initialization);
  RUN_TEST(test_lobby_list_packet_capacity);
  RUN_TEST(test_lobby_config_constants);
  return UNITY_END();
}
