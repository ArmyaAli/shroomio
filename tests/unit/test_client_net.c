#include "unity.h"

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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_client_net_records_fresh_pong);
  RUN_TEST(test_client_net_ignores_stale_pong_sample);
  RUN_TEST(test_client_net_clears_timed_out_pending_ping);
  return UNITY_END();
}
