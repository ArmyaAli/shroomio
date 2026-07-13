#include "unity.h"

#include "shared/net_telemetry.h"

static ShroomNetTelemetry telemetry;

void setUp(void) { ShroomNetTelemetryReset(&telemetry); }

void tearDown(void) {}

void test_aggregates_peer_channel_and_packet_type(void) {
  ShroomNetTelemetryRecordAccepted(&telemetry, 3u, SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT,
                                   40u, 1000u);
  ShroomNetTelemetryRecordSent(&telemetry, 3u, SHROOM_ENET_CHANNEL_SNAPSHOT, SHROOM_PACKET_SNAPSHOT,
                               900u, 1000u);
  ShroomNetTelemetryRecordDrop(&telemetry, 3u, SHROOM_ENET_CHANNEL_CHAT, SHROOM_PACKET_CHAT, 20u,
                               1000u);

  TEST_ASSERT_EQUAL_UINT64(1u, telemetry.by_peer[3].accepted.packets);
  TEST_ASSERT_EQUAL_UINT64(40u, telemetry.by_channel[SHROOM_ENET_CHANNEL_INPUT].accepted.bytes);
  TEST_ASSERT_EQUAL_UINT64(900u, telemetry.by_type[SHROOM_PACKET_SNAPSHOT].sent.bytes);
  TEST_ASSERT_EQUAL_UINT64(1u, telemetry.by_type[SHROOM_PACKET_CHAT].dropped.packets);
}

void test_rolling_window_expires_and_wraps_ring(void) {
  ShroomNetTelemetryWindow window;

  ShroomNetTelemetryRecordAccepted(&telemetry, 0u, SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT,
                                   32u, 0u);
  ShroomNetTelemetryRecordAccepted(&telemetry, 0u, SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT,
                                   48u, 950u);
  ShroomNetTelemetryReadWindow(&telemetry, 999u, 1000u, &window);
  TEST_ASSERT_EQUAL_UINT64(2u, window.totals.accepted.packets);

  ShroomNetTelemetryRecordAccepted(&telemetry, 0u, SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT,
                                   64u, 2100u);
  ShroomNetTelemetryReadWindow(&telemetry, 2100u, 1000u, &window);
  TEST_ASSERT_EQUAL_UINT64(1u, window.totals.accepted.packets);
  TEST_ASSERT_EQUAL_UINT64(64u, window.totals.accepted.bytes);
  TEST_ASSERT_EQUAL_UINT64(3u, telemetry.totals.accepted.packets);
}

void test_reset_clears_counters_buckets_and_transport(void) {
  ShroomNetTelemetryWindow window;

  ShroomNetTelemetryRecordSent(&telemetry, 1u, SHROOM_ENET_CHANNEL_CONTROL, SHROOM_PACKET_WELCOME,
                               64u, 100u);
  ShroomNetTelemetrySetPeerTransport(&telemetry, 1u, 70u, 250u, true);
  ShroomNetTelemetryReset(&telemetry);
  ShroomNetTelemetryReadWindow(&telemetry, 100u, 1000u, &window);

  TEST_ASSERT_EQUAL_UINT64(0u, telemetry.totals.sent.packets);
  TEST_ASSERT_EQUAL_UINT64(0u, window.totals.sent.packets);
  TEST_ASSERT_EQUAL_UINT16(0u, window.active_peers);
}

void test_transport_reports_loss_queue_pressure_and_saturates_totals(void) {
  ShroomNetTelemetryWindow window;

  ShroomNetTelemetrySetPeerTransport(&telemetry, 1u, 63u, 125u, true);
  ShroomNetTelemetrySetPeerTransport(&telemetry, 2u, 64u, 12000u, true);
  ShroomNetTelemetrySetPeerTransport(&telemetry, 3u, UINT32_MAX, 50u, true);
  ShroomNetTelemetryReadWindow(&telemetry, 0u, 1000u, &window);

  TEST_ASSERT_EQUAL_UINT16(3u, window.active_peers);
  TEST_ASSERT_EQUAL_UINT16(2u, window.congested_peers);
  TEST_ASSERT_EQUAL_UINT16(10000u, window.maximum_loss_basis_points);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, window.queue_packets);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, window.queue_high_water);
}

void test_transport_reconnect_resets_stale_queue_high_water(void) {
  ShroomNetTelemetryWindow window;

  ShroomNetTelemetrySetPeerTransport(&telemetry, 4u, 90u, 100u, true);
  ShroomNetTelemetrySetPeerTransport(&telemetry, 4u, 0u, 0u, false);
  ShroomNetTelemetrySetPeerTransport(&telemetry, 4u, 2u, 25u, true);
  ShroomNetTelemetryReadWindow(&telemetry, 0u, 1000u, &window);

  TEST_ASSERT_EQUAL_UINT16(1u, window.active_peers);
  TEST_ASSERT_EQUAL_UINT32(2u, window.queue_packets);
  TEST_ASSERT_EQUAL_UINT32(2u, window.queue_high_water);
  TEST_ASSERT_EQUAL_UINT16(25u, window.maximum_loss_basis_points);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_aggregates_peer_channel_and_packet_type);
  RUN_TEST(test_rolling_window_expires_and_wraps_ring);
  RUN_TEST(test_reset_clears_counters_buckets_and_transport);
  RUN_TEST(test_transport_reports_loss_queue_pressure_and_saturates_totals);
  RUN_TEST(test_transport_reconnect_resets_stale_queue_high_water);
  return UNITY_END();
}
