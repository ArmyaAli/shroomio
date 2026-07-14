#include "unity.h"

#include <stddef.h>
#include <string.h>

#include "shared/world_replication.h"

void setUp(void) {}
void tearDown(void) {}

static const ShroomWorldStateRecord* FindRecord(const ShroomWorldStateRecord* records,
                                                size_t count, ShroomEntityId entity_id) {
  for (size_t index = 0u; index < count; ++index) {
    if (records[index].entity_id == entity_id) {
      return &records[index];
    }
  }
  return NULL;
}

static size_t MakePacket(unsigned char* storage, uint64_t tick, uint16_t chunk_index,
                         uint16_t chunk_count, bool keyframe,
                         const ShroomWorldStateRecord* records, uint8_t record_count) {
  const size_t size = offsetof(ShroomWorldStatePacket, records) +
                      (size_t)record_count * sizeof(ShroomWorldStateRecord);
  ShroomWorldStatePacket* packet = (ShroomWorldStatePacket*)storage;

  memset(storage, 0, SHROOM_MAX_UNRELIABLE_PACKET_SIZE);
  ShroomPacketHeaderInit(&packet->header, SHROOM_PACKET_WORLD_STATE, (uint16_t)size);
  packet->tick = tick;
  packet->chunk_index = chunk_index;
  packet->chunk_count = chunk_count;
  packet->flags = keyframe ? SHROOM_WORLD_STATE_FLAG_KEYFRAME : 0u;
  packet->record_count = record_count;
  memcpy(packet->records, records, (size_t)record_count * sizeof(records[0]));
  return size;
}

static bool Apply(unsigned char* storage, size_t size,
                  ShroomWorldReplicationClientState* state,
                  ShroomSnapshotSporeState* spores, uint16_t* spore_count,
                  ShroomSnapshotPowerupState* powerups, uint16_t* powerup_count) {
  return ShroomWorldReplicationApplyPacket(
      state, spores, spore_count, powerups, powerup_count,
      (const ShroomWorldStatePacket*)storage, size);
}

static void test_builds_mixed_interest_scoped_spawn_update_and_remove_records(void) {
  ShroomWorldState world = {.tick = 1u, .spore_count = 3u, .powerup_count = 2u};
  ShroomWorldReplicationPeerState peer = {0};
  ShroomWorldStateRecord records[SHROOM_MAX_WORLD_STATE_RECORDS];
  ShroomWorldReplicationBatch batch;

  world.spores[0] = (ShroomSporeState){.entity_id = 10u,
                                       .position = {100.0f, 100.0f},
                                       .value = 7u,
                                       .active = true};
  world.spores[1] = (ShroomSporeState){.entity_id = 11u,
                                       .position = {200.0f, 100.0f},
                                       .value = 7u,
                                       .active = true};
  world.spores[2] = (ShroomSporeState){.entity_id = 12u,
                                       .position = {5000.0f, 5000.0f},
                                       .value = 7u,
                                       .active = true};
  world.powerups[0] = (ShroomPowerupState){.entity_id = 20u,
                                           .position = {300.0f, 100.0f},
                                           .type = SHROOM_POWERUP_SPEED,
                                           .active = true};
  world.powerups[1] = (ShroomPowerupState){.entity_id = 21u,
                                           .position = {5200.0f, 5000.0f},
                                           .type = SHROOM_POWERUP_SHIELD,
                                           .active = true};

  batch = ShroomWorldReplicationBuild(&peer, &world, (ShroomVec2){0.0f, 0.0f}, false,
                                      records, SHROOM_MAX_WORLD_STATE_RECORDS);
  TEST_ASSERT_TRUE(batch.keyframe);
  TEST_ASSERT_EQUAL_UINT32(3u, batch.record_count);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_WORLD_RECORD_SPAWN, FindRecord(records, 3u, 10u)->operation);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_WORLD_ENTITY_POWERUP,
                          FindRecord(records, 3u, 20u)->entity_kind);
  TEST_ASSERT_NULL(FindRecord(records, 3u, 12u));

  world.tick = 7u;
  world.spores[0].position.x = 150.0f;
  world.powerups[0].active = false;
  batch = ShroomWorldReplicationBuild(&peer, &world, (ShroomVec2){0.0f, 0.0f}, false,
                                      records, SHROOM_MAX_WORLD_STATE_RECORDS);
  TEST_ASSERT_FALSE(batch.keyframe);
  TEST_ASSERT_EQUAL_UINT32(2u, batch.record_count);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_WORLD_RECORD_UPDATE, FindRecord(records, 2u, 10u)->operation);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_WORLD_RECORD_REMOVE, FindRecord(records, 2u, 20u)->operation);
}

static void test_collect_remove_and_same_entity_respawn_are_authoritative(void) {
  ShroomWorldState world = {.tick = 1u, .powerup_count = 1u};
  ShroomWorldReplicationPeerState peer = {0};
  ShroomWorldStateRecord records[SHROOM_MAX_WORLD_STATE_RECORDS];
  ShroomWorldReplicationBatch batch;

  world.powerups[0] = (ShroomPowerupState){.entity_id = 44u,
                                           .position = {100.0f, 100.0f},
                                           .type = SHROOM_POWERUP_MAGNET,
                                           .active = true};
  ShroomWorldReplicationBuild(&peer, &world, (ShroomVec2){100.0f, 100.0f}, false, records,
                              SHROOM_MAX_WORLD_STATE_RECORDS);
  world.tick = 7u;
  world.powerups[0].active = false;
  batch = ShroomWorldReplicationBuild(&peer, &world, (ShroomVec2){100.0f, 100.0f}, false,
                                      records, SHROOM_MAX_WORLD_STATE_RECORDS);
  TEST_ASSERT_EQUAL_UINT32(1u, batch.record_count);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_WORLD_RECORD_REMOVE, records[0].operation);

  world.tick = 13u;
  world.powerups[0].active = true;
  world.powerups[0].position = (ShroomVec2){350.0f, 400.0f};
  world.powerups[0].type = SHROOM_POWERUP_SHIELD;
  batch = ShroomWorldReplicationBuild(&peer, &world, (ShroomVec2){100.0f, 100.0f}, false,
                                      records, SHROOM_MAX_WORLD_STATE_RECORDS);
  TEST_ASSERT_EQUAL_UINT32(1u, batch.record_count);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_WORLD_RECORD_SPAWN, records[0].operation);
  TEST_ASSERT_EQUAL_FLOAT(350.0f, records[0].position_x);
  TEST_ASSERT_EQUAL_UINT8(SHROOM_POWERUP_SHIELD, records[0].powerup_type);
}

static void test_packet_boundaries_are_mtu_safe(void) {
  const size_t per_packet = ShroomWorldStatePacketMaxRecords();
  const size_t header_size = offsetof(ShroomWorldStatePacket, records);

  TEST_ASSERT_GREATER_THAN_UINT32(0u, per_packet);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
                                  header_size + per_packet * sizeof(ShroomWorldStateRecord));
  TEST_ASSERT_GREATER_THAN_UINT32(
      SHROOM_MAX_UNRELIABLE_PACKET_SIZE,
      header_size + (per_packet + 1u) * sizeof(ShroomWorldStateRecord));
  TEST_ASSERT_EQUAL_UINT32(1u, ShroomWorldReplicationPacketCount(per_packet));
  TEST_ASSERT_EQUAL_UINT32(2u, ShroomWorldReplicationPacketCount(per_packet + 1u));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(
      SHROOM_MAX_UNRELIABLE_PACKET_SIZE * 2u,
      ShroomWorldReplicationPacketBytes(per_packet + 1u));
}

static void test_keyframe_repairs_loss_and_reordered_duplicates_without_stale_entities(void) {
  _Alignas(ShroomWorldStatePacket) unsigned char storage[SHROOM_MAX_UNRELIABLE_PACKET_SIZE];
  ShroomWorldReplicationClientState state = {0};
  ShroomSnapshotSporeState spores[SHROOM_MAX_SPORES] = {{.entity_id = 99u}};
  ShroomSnapshotPowerupState powerups[SHROOM_MAX_POWERUPS] = {{.entity_id = 77u, .active = 1u}};
  uint16_t spore_count = 1u;
  uint16_t powerup_count = 1u;
  const ShroomWorldStateRecord first = {.entity_id = 10u,
                                        .position_x = 10.0f,
                                        .entity_kind = SHROOM_WORLD_ENTITY_SPORE,
                                        .operation = SHROOM_WORLD_RECORD_SPAWN};
  const ShroomWorldStateRecord second = {.entity_id = 20u,
                                         .position_x = 20.0f,
                                         .entity_kind = SHROOM_WORLD_ENTITY_POWERUP,
                                         .operation = SHROOM_WORLD_RECORD_SPAWN,
                                         .powerup_type = SHROOM_POWERUP_SPEED};
  size_t size;

  size = MakePacket(storage, 30u, 1u, 2u, true, &second, 1u);
  TEST_ASSERT_TRUE(Apply(storage, size, &state, spores, &spore_count, powerups, &powerup_count));
  TEST_ASSERT_EQUAL_UINT16(1u, spore_count);
  TEST_ASSERT_EQUAL_UINT16(2u, powerup_count);
  TEST_ASSERT_TRUE(state.keyframe_pending);
  TEST_ASSERT_TRUE(Apply(storage, size, &state, spores, &spore_count, powerups, &powerup_count));

  size = MakePacket(storage, 30u, 0u, 2u, true, &first, 1u);
  TEST_ASSERT_TRUE(Apply(storage, size, &state, spores, &spore_count, powerups, &powerup_count));
  TEST_ASSERT_EQUAL_UINT16(1u, spore_count);
  TEST_ASSERT_EQUAL_UINT32(10u, spores[0].entity_id);
  TEST_ASSERT_EQUAL_UINT16(1u, powerup_count);
  TEST_ASSERT_EQUAL_UINT32(20u, powerups[0].entity_id);
  TEST_ASSERT_FALSE(state.keyframe_pending);

  size = MakePacket(storage, 29u, 0u, 1u, false, &second, 1u);
  TEST_ASSERT_FALSE(Apply(storage, size, &state, spores, &spore_count, powerups, &powerup_count));
  TEST_ASSERT_EQUAL_UINT16(1u, powerup_count);

  size = MakePacket(storage, 60u, 0u, 1u, true, &first, 1u);
  TEST_ASSERT_TRUE(Apply(storage, size, &state, spores, &spore_count, powerups, &powerup_count));
  TEST_ASSERT_EQUAL_UINT16(0u, powerup_count);
}

static void test_interest_keyframe_reduces_packets_and_bytes_from_full_world_burst(void) {
  ShroomWorldState world = {.tick = 1u,
                            .width = SHROOM_WORLD_WIDTH,
                            .height = SHROOM_WORLD_HEIGHT,
                            .spore_count = SHROOM_SPORE_TARGET_COUNT,
                            .powerup_count = SHROOM_MAX_POWERUPS};
  ShroomWorldReplicationPeerState peer = {0};
  ShroomWorldStateRecord records[SHROOM_MAX_WORLD_STATE_RECORDS];
  ShroomWorldReplicationBatch batch;
  const uint16_t old_spores_per_packet = ShroomSporeStatePacketMaxSpores();
  const size_t old_spore_packets =
      (world.spore_count + old_spores_per_packet - 1u) / old_spores_per_packet;
  const size_t old_packets = old_spore_packets + 1u;
  const size_t old_bytes =
      old_spore_packets * offsetof(ShroomSporeStatePacket, spores) +
      world.spore_count * sizeof(ShroomSnapshotSporeState) + sizeof(ShroomPowerupStatePacket);

  for (size_t index = 0u; index < world.spore_count; ++index) {
    const size_t column = index % 40u;
    const size_t row = index / 40u;
    world.spores[index] = (ShroomSporeState){
        .entity_id = (ShroomEntityId)(index + 1u),
        .position = {(float)column * 150.0f + 75.0f, (float)row * 210.0f + 75.0f},
        .value = SHROOM_SPORE_VALUE,
        .active = true};
  }
  for (size_t index = 0u; index < world.powerup_count; ++index) {
    world.powerups[index] = (ShroomPowerupState){
        .entity_id = (ShroomEntityId)(5000u + index),
        .position = {(float)index * 700.0f, (float)index * 700.0f},
        .type = SHROOM_POWERUP_SPEED,
        .active = true};
  }
  batch = ShroomWorldReplicationBuild(&peer, &world, (ShroomVec2){3000.0f, 3000.0f}, false,
                                      records, SHROOM_MAX_WORLD_STATE_RECORDS);

  TEST_ASSERT_TRUE(batch.keyframe);
  TEST_ASSERT_LESS_THAN_UINT32(world.spore_count + world.powerup_count, batch.record_count);
  TEST_ASSERT_LESS_THAN_UINT32(old_packets,
                              ShroomWorldReplicationPacketCount(batch.record_count));
  TEST_ASSERT_LESS_THAN_UINT32(old_bytes,
                              ShroomWorldReplicationPacketBytes(batch.record_count));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_builds_mixed_interest_scoped_spawn_update_and_remove_records);
  RUN_TEST(test_collect_remove_and_same_entity_respawn_are_authoritative);
  RUN_TEST(test_packet_boundaries_are_mtu_safe);
  RUN_TEST(test_keyframe_repairs_loss_and_reordered_duplicates_without_stale_entities);
  RUN_TEST(test_interest_keyframe_reduces_packets_and_bytes_from_full_world_burst);
  return UNITY_END();
}
