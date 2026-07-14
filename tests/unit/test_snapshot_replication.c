#include "unity.h"

#include <stddef.h>
#include <string.h>

#include "shared/snapshot_replication.h"

void setUp(void) {}
void tearDown(void) {}

static ShroomPlayerState Player(uint32_t player_id, uint32_t entity_id, uint8_t piece_index,
                                float x, float mass) {
  return (ShroomPlayerState){.player_id = player_id,
                             .entity_id = entity_id,
                             .position = {x, 100.0f},
                             .mass = mass,
                             .radius = 10.0f,
                             .alive = true,
                             .piece_index = piece_index};
}

static ShroomWorldState World(void) {
  return (ShroomWorldState){.width = 6000.0f, .height = 6000.0f};
}

static ShroomSnapshotPacket Packet(uint64_t tick, uint16_t total, uint16_t chunk_index) {
  ShroomSnapshotPacket packet = {
      .tick = tick,
      .last_processed_input_sequence = 77u,
      .player_id = 5u,
      .entity_id = 50u,
      .total_player_count = total,
      .chunk_index = chunk_index,
      .chunk_count = (uint16_t)ShroomSnapshotChunkCount(total),
      .match_phase = SHROOM_MATCH_PHASE_RUNNING,
  };
  const size_t offset = (size_t)chunk_index * SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK;
  packet.player_count =
      (uint16_t)(total > offset + SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK
                     ? SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK
                     : total - offset);
  for (uint16_t index = 0u; index < packet.player_count; ++index) {
    packet.players[index].entity_id = (uint32_t)(offset + index + 1u);
    packet.players[index].player_id = packet.players[index].entity_id;
    packet.players[index].alive = 1u;
  }
  ShroomPacketHeaderInit(
      &packet.header, SHROOM_PACKET_SNAPSHOT,
      (uint16_t)(offsetof(ShroomSnapshotPacket, players) +
                 (size_t)packet.player_count * sizeof(ShroomSnapshotPlayerState)));
  return packet;
}

static ShroomSnapshotAssemblyResult Push(ShroomSnapshotAssembly* assembly,
                                         ShroomSnapshotPacket* packet,
                                         ShroomSnapshotFrameMetadata* metadata,
                                         ShroomSnapshotPlayerState* players, uint16_t* count) {
  return ShroomSnapshotAssemblyPush(assembly, packet, packet->header.size, metadata, players,
                                    count);
}

static void test_selection_keeps_self_splits_objective_threats_and_nearby_in_stable_order(void) {
  ShroomWorldState world = World();
  ShroomSnapshotInterestState state = {0};
  uint16_t selected[SHROOM_MAX_PLAYER_ENTITIES];

  world.players[world.player_count++] = Player(1u, 101u, 0u, 100.0f, 100.0f);
  world.players[world.player_count++] = Player(1u, 102u, 1u, 5900.0f, 50.0f);
  world.players[world.player_count++] = Player(2u, 201u, 0u, 3000.0f, 80.0f);
  world.players[world.player_count++] = Player(3u, 301u, 0u, 500.0f, 140.0f);
  world.players[world.player_count++] = Player(5u, 501u, 0u, 500.0f, 50.0f);
  world.players[world.player_count++] = Player(4u, 401u, 0u, 500.0f, 50.0f);
  world.players[5].is_bot = true;
  world.players[world.player_count++] = Player(6u, 601u, 0u, 5000.0f, 500.0f);
  world.objective_controller_id = 2u;

  TEST_ASSERT_EQUAL_size_t(6u, ShroomSnapshotSelectPlayers(
                                        &state, &world, 1u, 101u, false, selected,
                                        SHROOM_MAX_PLAYER_ENTITIES));
  TEST_ASSERT_EQUAL_UINT16(0u, selected[0]);
  TEST_ASSERT_EQUAL_UINT16(1u, selected[1]);
  TEST_ASSERT_EQUAL_UINT16(2u, selected[2]);
  TEST_ASSERT_EQUAL_UINT16(3u, selected[3]);
  TEST_ASSERT_EQUAL_UINT16(5u, selected[4]);
  TEST_ASSERT_EQUAL_UINT16(4u, selected[5]);
}

static void test_hysteresis_retains_boundary_crossing_then_removes_far_entity(void) {
  ShroomWorldState world = World();
  ShroomSnapshotInterestState state = {0};
  uint16_t selected[4];

  world.players[world.player_count++] = Player(1u, 1u, 0u, 100.0f, 100.0f);
  world.players[world.player_count++] =
      Player(2u, 2u, 0u, 100.0f + SHROOM_WORLD_REPLICATION_INTEREST_RADIUS - 1.0f, 50.0f);
  TEST_ASSERT_EQUAL_size_t(
      2u, ShroomSnapshotSelectPlayers(&state, &world, 1u, 1u, false, selected, 4u));

  world.players[1].position.x = 100.0f + SHROOM_WORLD_REPLICATION_INTEREST_RADIUS + 100.0f;
  TEST_ASSERT_EQUAL_size_t(
      2u, ShroomSnapshotSelectPlayers(&state, &world, 1u, 1u, false, selected, 4u));

  world.players[1].position.x = 100.0f + SHROOM_WORLD_REPLICATION_INTEREST_RADIUS +
                                SHROOM_SNAPSHOT_INTEREST_HYSTERESIS + 1.0f;
  TEST_ASSERT_EQUAL_size_t(
      1u, ShroomSnapshotSelectPlayers(&state, &world, 1u, 1u, false, selected, 4u));
}

static void test_spectator_focus_and_max_population_are_supported(void) {
  ShroomWorldState world = World();
  ShroomSnapshotInterestState state = {0};
  uint16_t selected[SHROOM_MAX_PLAYER_ENTITIES];

  for (size_t index = 0u; index < SHROOM_MAX_PLAYER_ENTITIES; ++index) {
    world.players[index] = Player((uint32_t)index + 1u, (uint32_t)index + 100u, 0u,
                                  1000.0f + (float)(index % 10u), 100.0f);
  }
  world.player_count = SHROOM_MAX_PLAYER_ENTITIES;
  TEST_ASSERT_EQUAL_size_t(
      SHROOM_MAX_PLAYER_ENTITIES,
      ShroomSnapshotSelectPlayers(&state, &world, 0u, world.players[100].entity_id, true,
                                  selected, SHROOM_MAX_PLAYER_ENTITIES));
  TEST_ASSERT_EQUAL_UINT16(100u, selected[0]);
  TEST_ASSERT_EQUAL_size_t(SHROOM_SNAPSHOT_MAX_CHUNKS,
                           ShroomSnapshotChunkCount(SHROOM_MAX_PLAYER_ENTITIES));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
                                   sizeof(ShroomSnapshotPacket));
}

static void test_assembly_is_atomic_out_of_order_and_duplicate_safe(void) {
  ShroomSnapshotAssembly assembly = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  uint16_t count = 99u;
  ShroomSnapshotPacket first = Packet(10u, 16u, 0u);
  ShroomSnapshotPacket second = Packet(10u, 16u, 1u);

  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_PENDING,
                    Push(&assembly, &second, &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT16(99u, count);
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_PENDING,
                    Push(&assembly, &second, &metadata, players, &count));
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE,
                    Push(&assembly, &first, &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT16(16u, count);
  TEST_ASSERT_EQUAL_UINT64(10u, metadata.tick);
  for (uint16_t index = 0u; index < count; ++index) {
    TEST_ASSERT_EQUAL_UINT32((uint32_t)index + 1u, players[index].entity_id);
  }
}

static void test_missing_chunk_is_superseded_and_malformed_chunks_are_rejected(void) {
  ShroomSnapshotAssembly assembly = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  uint16_t count = 0u;
  ShroomSnapshotPacket old_first = Packet(20u, 16u, 0u);
  ShroomSnapshotPacket new_single = Packet(21u, 1u, 0u);
  ShroomSnapshotPacket malformed = Packet(22u, 16u, 0u);

  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_PENDING,
                    Push(&assembly, &old_first, &metadata, players, &count));
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE,
                    Push(&assembly, &new_single, &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT64(21u, metadata.tick);
  TEST_ASSERT_EQUAL_UINT16(1u, count);

  malformed.chunk_count = 1u;
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_REJECTED,
                    Push(&assembly, &malformed, &metadata, players, &count));
  malformed = Packet(22u, 16u, 0u);
  malformed.header.size -= 1u;
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_REJECTED,
                    ShroomSnapshotAssemblyPush(&assembly, &malformed, malformed.header.size,
                                               &metadata, players, &count));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_selection_keeps_self_splits_objective_threats_and_nearby_in_stable_order);
  RUN_TEST(test_hysteresis_retains_boundary_crossing_then_removes_far_entity);
  RUN_TEST(test_spectator_focus_and_max_population_are_supported);
  RUN_TEST(test_assembly_is_atomic_out_of_order_and_duplicate_safe);
  RUN_TEST(test_missing_chunk_is_superseded_and_malformed_chunks_are_rejected);
  return UNITY_END();
}
