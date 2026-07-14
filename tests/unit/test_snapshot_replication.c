#include "unity.h"

#include <stddef.h>
#include <stdio.h>
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

static ShroomSnapshotPlayerState SnapshotPlayer(uint32_t entity_id, float x) {
  ShroomSnapshotPlayerState player = {.player_id = entity_id,
                                      .entity_id = entity_id,
                                      .position_x = x,
                                      .position_y = 20.0f,
                                      .mass = 30.0f,
                                      .radius = 4.0f,
                                      .alive = 1u};
  snprintf(player.name, sizeof(player.name), "Player %u", entity_id);
  return player;
}

static ShroomSnapshotFrameMetadata Metadata(uint64_t tick) {
  return (ShroomSnapshotFrameMetadata){.tick = tick,
                                       .last_processed_input_sequence = 77u,
                                       .player_id = 5u,
                                       .entity_id = 50u,
                                       .match_phase = SHROOM_MATCH_PHASE_RUNNING};
}

static ShroomSnapshotAssemblyResult Push(ShroomSnapshotAssembly* assembly,
                                         ShroomSnapshotHistory* history,
                                         ShroomSnapshotPacket* packet,
                                         ShroomSnapshotFrameMetadata* metadata,
                                         ShroomSnapshotPlayerState* players, uint16_t* count) {
  return ShroomSnapshotAssemblyPush(assembly, packet, packet->header.size, history, metadata,
                                    players, count);
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
  TEST_ASSERT_LESS_OR_EQUAL_size_t(SHROOM_SNAPSHOT_MAX_CHUNKS,
                                   ShroomSnapshotChunkCount(SHROOM_MAX_PLAYER_ENTITIES));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
                                   sizeof(ShroomSnapshotPacket));
}

static void test_max_population_keyframe_round_trips_across_all_chunks(void) {
  static ShroomSnapshotAssembly assembly;
  static ShroomSnapshotHistory history;
  static ShroomSnapshotEncodedFrame encoded;
  static ShroomSnapshotPlayerState source[SHROOM_MAX_SNAPSHOT_PLAYERS];
  static ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS];
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotFrameMetadata source_metadata = Metadata(11u);
  uint16_t count = 0u;

  memset(&assembly, 0, sizeof(assembly));
  memset(&history, 0, sizeof(history));
  for (uint16_t index = 0u; index < SHROOM_MAX_SNAPSHOT_PLAYERS; ++index) {
    source[index] = SnapshotPlayer((uint32_t)index + 1u, (float)index);
  }

  TEST_ASSERT_TRUE(ShroomSnapshotEncodeFrame(&source_metadata, source,
                                             SHROOM_MAX_SNAPSHOT_PLAYERS, NULL, true,
                                             &encoded));
  TEST_ASSERT_EQUAL_UINT16(ShroomSnapshotChunkCount(SHROOM_MAX_SNAPSHOT_PLAYERS),
                           encoded.packet_count);
  for (uint16_t index = 0u; index < encoded.packet_count; ++index) {
    ShroomSnapshotAssemblyResult expected =
        index + 1u == encoded.packet_count ? SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE
                                          : SHROOM_SNAPSHOT_ASSEMBLY_PENDING;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
                                     encoded.packets[index].header.size);
    TEST_ASSERT_EQUAL(expected, Push(&assembly, &history, &encoded.packets[index],
                                     &metadata, players, &count));
  }
  TEST_ASSERT_EQUAL_UINT16(SHROOM_MAX_SNAPSHOT_PLAYERS, count);
  TEST_ASSERT_EQUAL_UINT32(1u, players[0].entity_id);
  TEST_ASSERT_EQUAL_UINT32(SHROOM_MAX_SNAPSHOT_PLAYERS, players[count - 1u].entity_id);
}

static void test_keyframe_round_trip_is_atomic_out_of_order_and_duplicate_safe(void) {
  ShroomSnapshotAssembly assembly = {0};
  ShroomSnapshotHistory history = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotFrameMetadata source_metadata = Metadata(10u);
  ShroomSnapshotEncodedFrame encoded;
  ShroomSnapshotPlayerState source[20];
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  uint16_t count = 99u;
  for (uint16_t index = 0u; index < 20u; ++index) {
    source[index] = SnapshotPlayer((uint32_t)index + 1u, (float)index);
  }
  TEST_ASSERT_TRUE(ShroomSnapshotEncodeFrame(&source_metadata, source, 20u, NULL, true, &encoded));
  TEST_ASSERT_GREATER_THAN_UINT16(1u, encoded.packet_count);
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_PENDING,
                    Push(&assembly, &history, &encoded.packets[1], &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT16(99u, count);
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_PENDING,
                    Push(&assembly, &history, &encoded.packets[1], &metadata, players, &count));
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE,
                    Push(&assembly, &history, &encoded.packets[0], &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT16(20u, count);
  TEST_ASSERT_EQUAL_UINT64(10u, metadata.tick);
  for (uint16_t index = 0u; index < count; ++index) {
    TEST_ASSERT_EQUAL_UINT32((uint32_t)index + 1u, players[index].entity_id);
  }
}

static void test_delta_suppresses_unchanged_components_and_reduces_bytes(void) {
  ShroomSnapshotFrameMetadata first_metadata = Metadata(20u);
  ShroomSnapshotFrameMetadata second_metadata = Metadata(21u);
  ShroomSnapshotEncodedFrame keyframe;
  ShroomSnapshotEncodedFrame delta;
  ShroomSnapshotPlayerState players[8];
  ShroomSnapshotFrame baseline = {.tick = 20u, .player_count = 8u};
  for (uint16_t index = 0u; index < 8u; ++index) {
    players[index] = SnapshotPlayer((uint32_t)index + 1u, (float)index);
    baseline.players[index] = players[index];
  }
  TEST_ASSERT_TRUE(ShroomSnapshotEncodeFrame(&first_metadata, players, 8u, NULL, true, &keyframe));
  players[3].position_x = 99.0f;
  TEST_ASSERT_TRUE(
      ShroomSnapshotEncodeFrame(&second_metadata, players, 8u, &baseline, false, &delta));
  TEST_ASSERT_EQUAL_UINT16(1u, delta.packets[0].record_count);
  TEST_ASSERT_LESS_THAN_size_t(keyframe.wire_bytes, delta.wire_bytes);
  {
    ShroomSnapshotRecordHeader record;
    memcpy(&record, delta.packets[0].payload, sizeof(record));
    TEST_ASSERT_EQUAL_UINT16(SHROOM_SNAPSHOT_COMPONENT_TRANSFORM, record.component_mask);
    TEST_ASSERT_EQUAL_UINT8(SHROOM_SNAPSHOT_RECORD_UPDATE, record.operation);
  }
}

static void test_delta_applies_spawn_update_and_despawn_against_baseline(void) {
  ShroomSnapshotAssembly assembly = {0};
  ShroomSnapshotHistory history = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotFrameMetadata source_metadata = Metadata(31u);
  ShroomSnapshotEncodedFrame encoded;
  ShroomSnapshotPlayerState baseline_players[3] = {
      SnapshotPlayer(1u, 1.0f), SnapshotPlayer(2u, 2.0f), SnapshotPlayer(3u, 3.0f)};
  ShroomSnapshotPlayerState current[3] = {
      SnapshotPlayer(1u, 10.0f), SnapshotPlayer(3u, 3.0f), SnapshotPlayer(4u, 4.0f)};
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  uint16_t count = 0u;
  const ShroomSnapshotFrame* baseline;
  ShroomSnapshotHistoryStore(&history, 30u, baseline_players, 3u);
  baseline = ShroomSnapshotHistoryFind(&history, 30u);
  TEST_ASSERT_TRUE(
      ShroomSnapshotEncodeFrame(&source_metadata, current, 3u, baseline, false, &encoded));
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE,
                    Push(&assembly, &history, &encoded.packets[0], &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT16(3u, count);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, players[0].position_x);
  TEST_ASSERT_EQUAL_UINT32(3u, players[1].entity_id);
  TEST_ASSERT_EQUAL_UINT32(4u, players[2].entity_id);
}

static void test_missing_baseline_is_rejected_then_keyframe_recovers(void) {
  ShroomSnapshotAssembly assembly = {0};
  ShroomSnapshotHistory history = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotFrameMetadata source_metadata = Metadata(41u);
  ShroomSnapshotEncodedFrame encoded;
  ShroomSnapshotPlayerState source = SnapshotPlayer(1u, 1.0f);
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  ShroomSnapshotFrame missing = {.tick = 40u, .player_count = 1u, .players = {source}};
  uint16_t count = 0u;

  source.position_x = 2.0f;
  TEST_ASSERT_TRUE(
      ShroomSnapshotEncodeFrame(&source_metadata, &source, 1u, &missing, false, &encoded));
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_REJECTED,
                    Push(&assembly, &history, &encoded.packets[0], &metadata, players, &count));
  source_metadata.tick = 42u;
  TEST_ASSERT_TRUE(ShroomSnapshotEncodeFrame(&source_metadata, &source, 1u, NULL, true, &encoded));
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_COMPLETE,
                    Push(&assembly, &history, &encoded.packets[0], &metadata, players, &count));
  TEST_ASSERT_EQUAL_UINT64(42u, metadata.tick);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, players[0].position_x);
}

static void test_history_expiry_and_tick_wrap_are_bounded(void) {
  ShroomSnapshotHistory history = {0};
  ShroomSnapshotPlayerState player = SnapshotPlayer(1u, 1.0f);
  for (uint64_t tick = 1u; tick <= SHROOM_SNAPSHOT_HISTORY_SIZE + 1u; ++tick) {
    ShroomSnapshotHistoryStore(&history, tick, &player, 1u);
  }
  TEST_ASSERT_NULL(ShroomSnapshotHistoryFind(&history, 1u));
  TEST_ASSERT_NOT_NULL(ShroomSnapshotHistoryFind(&history, SHROOM_SNAPSHOT_HISTORY_SIZE + 1u));
  TEST_ASSERT_TRUE(ShroomSnapshotTickIsNewer(0u, UINT64_MAX));
  TEST_ASSERT_FALSE(ShroomSnapshotTickIsNewer(UINT64_MAX, 0u));
}

static void test_malformed_payload_is_rejected(void) {
  ShroomSnapshotAssembly assembly = {0};
  ShroomSnapshotHistory history = {0};
  ShroomSnapshotFrameMetadata metadata = {0};
  ShroomSnapshotFrameMetadata source_metadata = Metadata(50u);
  ShroomSnapshotEncodedFrame encoded;
  ShroomSnapshotPlayerState source = SnapshotPlayer(1u, 1.0f);
  ShroomSnapshotPlayerState players[SHROOM_MAX_SNAPSHOT_PLAYERS] = {0};
  uint16_t count = 0u;

  TEST_ASSERT_TRUE(ShroomSnapshotEncodeFrame(&source_metadata, &source, 1u, NULL, true, &encoded));
  encoded.packets[0].payload[offsetof(ShroomSnapshotRecordHeader, size)] = 1u;
  TEST_ASSERT_EQUAL(SHROOM_SNAPSHOT_ASSEMBLY_REJECTED,
                    Push(&assembly, &history, &encoded.packets[0], &metadata, players, &count));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_selection_keeps_self_splits_objective_threats_and_nearby_in_stable_order);
  RUN_TEST(test_hysteresis_retains_boundary_crossing_then_removes_far_entity);
  RUN_TEST(test_spectator_focus_and_max_population_are_supported);
  RUN_TEST(test_max_population_keyframe_round_trips_across_all_chunks);
  RUN_TEST(test_keyframe_round_trip_is_atomic_out_of_order_and_duplicate_safe);
  RUN_TEST(test_delta_suppresses_unchanged_components_and_reduces_bytes);
  RUN_TEST(test_delta_applies_spawn_update_and_despawn_against_baseline);
  RUN_TEST(test_missing_baseline_is_rejected_then_keyframe_recovers);
  RUN_TEST(test_history_expiry_and_tick_wrap_are_bounded);
  RUN_TEST(test_malformed_payload_is_rejected);
  return UNITY_END();
}
