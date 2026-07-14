#include "unity.h"

#include <stdio.h>
#include <string.h>

#include "client/server_discovery_state.h"

static ShroomServerDiscoveryState state;

void setUp(void) { ShroomServerDiscoveryStateBegin(&state, 77u, 1000ull); }

void tearDown(void) {}

static ShroomDirectoryServerEntry Entry(uint64_t id, const char* host, uint16_t port,
                                        uint16_t players, uint16_t capacity) {
  ShroomDirectoryServerEntry entry = {
      .server_id = id, .port = port, .player_count = players, .capacity = capacity};
  snprintf(entry.name, sizeof(entry.name), "Server %llu", (unsigned long long)id);
  snprintf(entry.host, sizeof(entry.host), "%s", host);
  return entry;
}

static ShroomDirectoryListPacket DirectoryPacket(uint32_t generation, uint8_t chunk_index,
                                                 uint8_t chunk_count,
                                                 const ShroomDirectoryServerEntry* entries,
                                                 uint8_t entry_count) {
  ShroomDirectoryListPacket packet = {0};
  const size_t size = SHROOM_DIRECTORY_LIST_PACKET_SIZE(entry_count);
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_DIRECTORY_LIST, (uint16_t)size);
  packet.protocol_version = SHROOM_DIRECTORY_PROTOCOL_VERSION;
  packet.generation = generation;
  packet.chunk_index = chunk_index;
  packet.chunk_count = chunk_count;
  packet.entry_count = entry_count;
  if (entry_count > 0u) {
    memcpy(packet.entries, entries, (size_t)entry_count * sizeof(entries[0]));
  }
  return packet;
}

static ShroomServerProbeResponsePacket ProbeResponse(uint32_t generation, uint32_t nonce,
                                                     uint16_t players, uint16_t capacity) {
  ShroomServerProbeResponsePacket packet = {.protocol_version = SHROOM_PROTOCOL_VERSION,
                                            .generation = generation,
                                            .nonce = nonce,
                                            .player_count = players,
                                            .capacity = capacity};
  ShroomPacketHeaderInit(&packet.header, SHROOM_PACKET_SERVER_PROBE_RESPONSE, sizeof(packet));
  return packet;
}

static void LoadOneCandidate(void) {
  const ShroomDirectoryServerEntry entry = Entry(1u, "127.0.0.1", 7777u, 1u, 16u);
  const ShroomDirectoryListPacket packet = DirectoryPacket(77u, 0u, 1u, &entry, 1u);
  TEST_ASSERT_TRUE(ShroomServerDiscoveryStateIngestDirectory(
      &state, &packet, SHROOM_DIRECTORY_LIST_PACKET_SIZE(1u), 1050ull));
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_PROBING, state.phase);
}

void test_directory_chunks_deduplicate_clamp_and_filter_full_entries(void) {
  ShroomDirectoryServerEntry first_chunk[] = {Entry(1u, "127.0.0.4", 7777u, 4u, 5000u),
                                              Entry(2u, "127.0.0.2", 7777u, 8u, 8u),
                                              Entry(5u, "127.0.0.5", 7777u, 1u, 5000u)};
  ShroomDirectoryServerEntry second_chunk[] = {Entry(3u, "127.0.0.4", 7777u, 2u, 32u),
                                               Entry(4u, "127.0.0.3", 7778u, 0u, 16u)};
  ShroomDirectoryListPacket packet = DirectoryPacket(77u, 1u, 2u, second_chunk, 2u);

  TEST_ASSERT_TRUE(ShroomServerDiscoveryStateIngestDirectory(
      &state, &packet, SHROOM_DIRECTORY_LIST_PACKET_SIZE(2u), 1010ull));
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_DIRECTORY, state.phase);
  packet = DirectoryPacket(77u, 0u, 2u, first_chunk, 3u);
  TEST_ASSERT_TRUE(ShroomServerDiscoveryStateIngestDirectory(
      &state, &packet, SHROOM_DIRECTORY_LIST_PACKET_SIZE(3u), 1020ull));
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_PROBING, state.phase);
  TEST_ASSERT_EQUAL_size_t(3u, state.candidate_count);
  TEST_ASSERT_EQUAL_UINT16(SHROOM_SERVER_MAX_CLIENTS, state.candidates[2].server.capacity);
}

void test_generation_mismatch_and_stale_list_are_ignored(void) {
  const ShroomDirectoryServerEntry entry = Entry(1u, "127.0.0.1", 7777u, 0u, 16u);
  ShroomDirectoryListPacket packet = DirectoryPacket(78u, 0u, 1u, &entry, 1u);

  TEST_ASSERT_FALSE(ShroomServerDiscoveryStateIngestDirectory(
      &state, &packet, SHROOM_DIRECTORY_LIST_PACKET_SIZE(1u), 1100ull));
  packet.generation = 77u;
  TEST_ASSERT_FALSE(ShroomServerDiscoveryStateIngestDirectory(
      &state, &packet, SHROOM_DIRECTORY_LIST_PACKET_SIZE(1u),
      1000ull + SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS));
  TEST_ASSERT_EQUAL_size_t(0u, state.candidate_count);
}

void test_probe_response_measures_latency_and_authoritative_load(void) {
  ShroomServerProbeResponsePacket response;
  const ShroomDiscoveryCandidate* result;

  LoadOneCandidate();
  TEST_ASSERT_TRUE(ShroomServerDiscoveryStateStartProbe(&state, 0u, 900u, 1100ull));
  response = ProbeResponse(77u, 900u, 5u, 5000u);
  TEST_ASSERT_TRUE(
      ShroomServerDiscoveryStateAcceptProbe(&state, 0u, &response, sizeof(response), 1142ull));
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_COMPLETE, state.phase);
  TEST_ASSERT_EQUAL_size_t(1u, ShroomServerDiscoveryStateResultCount(&state));
  result = ShroomServerDiscoveryStateResult(&state, 0u);
  TEST_ASSERT_NOT_NULL(result);
  TEST_ASSERT_EQUAL_UINT16(42u, result->latency_ms);
  TEST_ASSERT_EQUAL_UINT16(5u, result->server.player_count);
  TEST_ASSERT_EQUAL_UINT16(SHROOM_SERVER_MAX_CLIENTS, result->server.capacity);
}

void test_probe_timeout_and_late_reply_do_not_publish_candidate(void) {
  ShroomServerProbeResponsePacket response;

  LoadOneCandidate();
  TEST_ASSERT_TRUE(ShroomServerDiscoveryStateStartProbe(&state, 0u, 901u, 1100ull));
  ShroomServerDiscoveryStateUpdate(&state, 1100ull + SHROOM_DISCOVERY_PROBE_TIMEOUT_MS);
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_COMPLETE, state.phase);
  TEST_ASSERT_EQUAL_size_t(0u, ShroomServerDiscoveryStateResultCount(&state));
  response = ProbeResponse(77u, 901u, 0u, 16u);
  TEST_ASSERT_FALSE(
      ShroomServerDiscoveryStateAcceptProbe(&state, 0u, &response, sizeof(response), 3200ull));
}

void test_full_probe_response_is_discarded(void) {
  ShroomServerProbeResponsePacket response;

  LoadOneCandidate();
  TEST_ASSERT_TRUE(ShroomServerDiscoveryStateStartProbe(&state, 0u, 902u, 1100ull));
  response = ProbeResponse(77u, 902u, 16u, 16u);
  TEST_ASSERT_TRUE(
      ShroomServerDiscoveryStateAcceptProbe(&state, 0u, &response, sizeof(response), 1110ull));
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_COMPLETE, state.phase);
  TEST_ASSERT_EQUAL_size_t(0u, ShroomServerDiscoveryStateResultCount(&state));
}

void test_overall_deadline_and_cancellation_are_terminal(void) {
  ShroomServerDiscoveryStateUpdate(&state, 1000ull + SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS);
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_FAILED, state.phase);
  ShroomServerDiscoveryStateBegin(&state, 78u, 9000ull);
  ShroomServerDiscoveryStateCancel(&state);
  TEST_ASSERT_EQUAL(SHROOM_DISCOVERY_CANCELLED, state.phase);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_directory_chunks_deduplicate_clamp_and_filter_full_entries);
  RUN_TEST(test_generation_mismatch_and_stale_list_are_ignored);
  RUN_TEST(test_probe_response_measures_latency_and_authoritative_load);
  RUN_TEST(test_probe_timeout_and_late_reply_do_not_publish_candidate);
  RUN_TEST(test_full_probe_response_is_discarded);
  RUN_TEST(test_overall_deadline_and_cancellation_are_terminal);
  return UNITY_END();
}
