#include "unity.h"
#include "../src/shared/protocol.h"
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

void test_packet_header_size(void) {
    TEST_ASSERT_EQUAL(4, sizeof(ShroomPacketHeader));
}

void test_hello_packet_size(void) {
    TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 32, sizeof(ShroomHelloPacket));
}

void test_welcome_packet_size(void) {
    TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 4 + 4 + 2 + 2 + 4 + 4, sizeof(ShroomWelcomePacket));
}

void test_input_packet_size(void) {
    TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4 + 4 + 4, sizeof(ShroomInputPacket));
}

void test_ping_packet_size(void) {
    TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4, sizeof(ShroomPingPacket));
}

void test_pong_packet_size(void) {
    TEST_ASSERT_EQUAL(sizeof(ShroomPacketHeader) + 4, sizeof(ShroomPongPacket));
}

void test_snapshot_player_state_size(void) {
    TEST_ASSERT_EQUAL(4 + 4 + 4 + 4 + 4 + 4 + 1 + 1 + 2, sizeof(ShroomSnapshotPlayerState));
}

void test_packet_type_values(void) {
    TEST_ASSERT_EQUAL(1, SHROOM_PACKET_HELLO);
    TEST_ASSERT_EQUAL(2, SHROOM_PACKET_WELCOME);
    TEST_ASSERT_EQUAL(3, SHROOM_PACKET_INPUT);
    TEST_ASSERT_EQUAL(4, SHROOM_PACKET_SNAPSHOT);
    TEST_ASSERT_EQUAL(5, SHROOM_PACKET_PING);
    TEST_ASSERT_EQUAL(6, SHROOM_PACKET_PONG);
    TEST_ASSERT_EQUAL(7, SHROOM_PACKET_SPORE_STATE);
}

void test_protocol_constants(void) {
    TEST_ASSERT_EQUAL(7777, SHROOM_SERVER_PORT);
    TEST_ASSERT_EQUAL(1, SHROOM_PROTOCOL_VERSION);
    TEST_ASSERT_EQUAL(32, SHROOM_MAX_NAME_LENGTH);
    TEST_ASSERT_EQUAL(15, SHROOM_SNAPSHOT_RATE);
    TEST_ASSERT_EQUAL(128, SHROOM_MAX_SNAPSHOT_PLAYERS);
    TEST_ASSERT_EQUAL(5, SHROOM_SPORE_STATE_RATE);
}

void test_snapshot_spore_state_size(void) {
    TEST_ASSERT_EQUAL(4 + 4 + 4 + 2 + 2, sizeof(ShroomSnapshotSporeState));
}

void test_spore_state_packet_initialization(void) {
    ShroomSporeStatePacket packet;
    uint16_t actual_size = (uint16_t)(sizeof(ShroomPacketHeader) + sizeof(uint64_t) +
                                      sizeof(uint16_t) + sizeof(uint16_t) +
                                      3 * sizeof(ShroomSnapshotSporeState));
    memset(&packet, 0, sizeof(packet));

    packet.header.type = SHROOM_PACKET_SPORE_STATE;
    packet.header.size = actual_size;
    packet.tick = 100;
    packet.spore_count = 3;

    packet.spores[0] = (ShroomSnapshotSporeState){
        .entity_id = 1, .position_x = 100.0f, .position_y = 200.0f, .value = 8};
    packet.spores[1] = (ShroomSnapshotSporeState){
        .entity_id = 2, .position_x = 300.0f, .position_y = 400.0f, .value = 8};
    packet.spores[2] = (ShroomSnapshotSporeState){
        .entity_id = 3, .position_x = 500.0f, .position_y = 600.0f, .value = 8};

    TEST_ASSERT_EQUAL(SHROOM_PACKET_SPORE_STATE, packet.header.type);
    TEST_ASSERT_EQUAL(100, packet.tick);
    TEST_ASSERT_EQUAL(3, packet.spore_count);
    TEST_ASSERT_EQUAL(1, packet.spores[0].entity_id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, packet.spores[0].position_x);
    TEST_ASSERT_EQUAL(8, packet.spores[0].value);
    TEST_ASSERT_EQUAL(3, packet.spores[2].entity_id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, packet.spores[2].position_y);
}

void test_hello_packet_initialization(void) {
    ShroomHelloPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    packet.header.type = SHROOM_PACKET_HELLO;
    packet.header.size = sizeof(ShroomHelloPacket);
    packet.protocol_version = SHROOM_PROTOCOL_VERSION;
    strncpy(packet.name, "TestPlayer", SHROOM_MAX_NAME_LENGTH);
    
    TEST_ASSERT_EQUAL(SHROOM_PACKET_HELLO, packet.header.type);
    TEST_ASSERT_EQUAL(sizeof(ShroomHelloPacket), packet.header.size);
    TEST_ASSERT_EQUAL(SHROOM_PROTOCOL_VERSION, packet.protocol_version);
    TEST_ASSERT_EQUAL_STRING("TestPlayer", packet.name);
}

void test_input_packet_initialization(void) {
    ShroomInputPacket packet;
    memset(&packet, 0, sizeof(packet));
    
    packet.header.type = SHROOM_PACKET_INPUT;
    packet.header.size = sizeof(ShroomInputPacket);
    packet.sequence = 42;
    packet.direction_x = 0.5f;
    packet.direction_y = -0.5f;
    
    TEST_ASSERT_EQUAL(SHROOM_PACKET_INPUT, packet.header.type);
    TEST_ASSERT_EQUAL(sizeof(ShroomInputPacket), packet.header.size);
    TEST_ASSERT_EQUAL(42, packet.sequence);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, packet.direction_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, packet.direction_y);
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
    state.alive = 1;
    state.is_bot = 0;
    
    TEST_ASSERT_EQUAL(1, state.player_id);
    TEST_ASSERT_EQUAL(100, state.entity_id);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1000.0f, state.position_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2000.0f, state.position_y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 150.0f, state.mass);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, state.radius);
    TEST_ASSERT_EQUAL(1, state.alive);
    TEST_ASSERT_EQUAL(0, state.is_bot);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_packet_header_size);
    RUN_TEST(test_hello_packet_size);
    RUN_TEST(test_welcome_packet_size);
    RUN_TEST(test_input_packet_size);
    RUN_TEST(test_ping_packet_size);
    RUN_TEST(test_pong_packet_size);
    RUN_TEST(test_snapshot_player_state_size);
    RUN_TEST(test_packet_type_values);
    RUN_TEST(test_protocol_constants);
    RUN_TEST(test_hello_packet_initialization);
    RUN_TEST(test_input_packet_initialization);
    RUN_TEST(test_snapshot_player_state_initialization);
    RUN_TEST(test_snapshot_spore_state_size);
    RUN_TEST(test_spore_state_packet_initialization);
    return UNITY_END();
}
