#include "unity.h"
#include "../src/shared/connection.h"

static ShroomConnectionManager manager;

void setUp(void) { ShroomConnectionManagerInit(&manager, 10); }

void tearDown(void) { ShroomConnectionManagerShutdown(&manager); }

void test_connection_manager_init(void) {
  ShroomConnectionManager local_manager;
  ShroomConnectionManagerInit(&local_manager, 5);

  TEST_ASSERT_NOT_NULL(local_manager.connections);
  TEST_ASSERT_EQUAL_UINT32(5, local_manager.max_connections);
  TEST_ASSERT_EQUAL_UINT32(0, local_manager.active_connections);
  TEST_ASSERT_EQUAL_UINT32(30000, local_manager.connection_timeout_ms);
  TEST_ASSERT_EQUAL_UINT32(5000, local_manager.ping_interval_ms);

  ShroomConnectionManagerShutdown(&local_manager);
}

void test_connection_manager_init_null(void) {
  ShroomConnectionManagerInit(NULL, 5);
  // Should not crash
}

void test_connection_manager_init_zero_max(void) {
  ShroomConnectionManager local_manager;
  ShroomConnectionManagerInit(&local_manager, 0);

  TEST_ASSERT_NULL(local_manager.connections);
  TEST_ASSERT_EQUAL_UINT32(0, local_manager.max_connections);
}

void test_connection_manager_shutdown(void) {
  ShroomConnectionManager local_manager;
  ShroomConnectionManagerInit(&local_manager, 5);

  ShroomConnectionManagerShutdown(&local_manager);

  TEST_ASSERT_NULL(local_manager.connections);
  TEST_ASSERT_EQUAL_UINT32(0, local_manager.max_connections);
  TEST_ASSERT_EQUAL_UINT32(0, local_manager.active_connections);
}

void test_connection_manager_shutdown_null(void) {
  ShroomConnectionManagerShutdown(NULL);
  // Should not crash
}

void test_connection_manager_add(void) {
  ShroomConnection* conn = ShroomConnectionManagerAdd(&manager, 0);

  TEST_ASSERT_NOT_NULL(conn);
  TEST_ASSERT_EQUAL_UINT32(0, conn->peer_id);
  TEST_ASSERT_EQUAL(SHROOM_CONN_STATE_CONNECTING, conn->state);
  TEST_ASSERT_EQUAL_UINT32(1, manager.active_connections);
}

void test_connection_manager_add_multiple(void) {
  ShroomConnection* conn1 = ShroomConnectionManagerAdd(&manager, 0);
  ShroomConnection* conn2 = ShroomConnectionManagerAdd(&manager, 1);
  ShroomConnection* conn3 = ShroomConnectionManagerAdd(&manager, 2);

  TEST_ASSERT_NOT_NULL(conn1);
  TEST_ASSERT_NOT_NULL(conn2);
  TEST_ASSERT_NOT_NULL(conn3);
  TEST_ASSERT_EQUAL_UINT32(3, manager.active_connections);
}

void test_connection_manager_add_duplicate(void) {
  ShroomConnection* conn1 = ShroomConnectionManagerAdd(&manager, 0);
  ShroomConnection* conn2 = ShroomConnectionManagerAdd(&manager, 0);

  TEST_ASSERT_NOT_NULL(conn1);
  TEST_ASSERT_NULL(conn2); // Should fail - slot already in use
  TEST_ASSERT_EQUAL_UINT32(1, manager.active_connections);
}

void test_connection_manager_add_invalid_peer_id(void) {
  ShroomConnection* conn = ShroomConnectionManagerAdd(&manager, 100);

  TEST_ASSERT_NULL(conn);
  TEST_ASSERT_EQUAL_UINT32(0, manager.active_connections);
}

void test_connection_manager_remove(void) {
  ShroomConnectionManagerAdd(&manager, 0);
  TEST_ASSERT_EQUAL_UINT32(1, manager.active_connections);

  ShroomConnectionManagerRemove(&manager, 0);
  TEST_ASSERT_EQUAL_UINT32(0, manager.active_connections);
}

void test_connection_manager_remove_nonexistent(void) {
  ShroomConnectionManagerRemove(&manager, 0);
  TEST_ASSERT_EQUAL_UINT32(0, manager.active_connections);
}

void test_connection_manager_get(void) {
  ShroomConnectionManagerAdd(&manager, 0);
  ShroomConnection* conn = ShroomConnectionManagerGet(&manager, 0);

  TEST_ASSERT_NOT_NULL(conn);
  TEST_ASSERT_EQUAL_UINT32(0, conn->peer_id);
}

void test_connection_manager_get_nonexistent(void) {
  ShroomConnection* conn = ShroomConnectionManagerGet(&manager, 0);
  TEST_ASSERT_NULL(conn);
}

void test_connection_set_state(void) {
  ShroomConnection conn = {0};
  ShroomConnectionSetState(&conn, SHROOM_CONN_STATE_CONNECTED);
  TEST_ASSERT_EQUAL(SHROOM_CONN_STATE_CONNECTED, conn.state);
}

void test_connection_set_state_null(void) {
  ShroomConnectionSetState(NULL, SHROOM_CONN_STATE_CONNECTED);
  // Should not crash
}

void test_connection_update_activity(void) {
  ShroomConnection conn = {0};
  ShroomConnectionUpdateActivity(&conn, 1000);
  TEST_ASSERT_EQUAL_UINT64(1000, conn.last_activity_time);
}

void test_connection_update_activity_null(void) {
  ShroomConnectionUpdateActivity(NULL, 1000);
  // Should not crash
}

void test_connection_update_ping(void) {
  ShroomConnection conn = {0};
  ShroomConnectionUpdatePing(&conn, 42, 5000);
  TEST_ASSERT_EQUAL_UINT32(42, conn.ping_nonce);
  TEST_ASSERT_EQUAL_UINT64(5000, conn.last_ping_time);
}

void test_connection_calculate_rtt(void) {
  ShroomConnection conn = {0};
  conn.last_ping_time = 1000;

  uint32_t rtt = ShroomConnectionCalculateRTT(&conn, 1050);
  TEST_ASSERT_EQUAL_UINT32(50, rtt);
  TEST_ASSERT_EQUAL_UINT32(50, conn.rtt_ms);
}

void test_connection_calculate_rtt_invalid(void) {
  ShroomConnection conn = {0};
  conn.last_ping_time = 1000;

  uint32_t rtt = ShroomConnectionCalculateRTT(&conn, 500); // pong_time < ping_time
  TEST_ASSERT_EQUAL_UINT32(0, rtt);
}

void test_connection_is_timed_out(void) {
  ShroomConnection conn = {0};
  conn.state = SHROOM_CONN_STATE_CONNECTED;
  conn.last_activity_time = 1000;

  TEST_ASSERT_FALSE(ShroomConnectionIsTimedOut(&conn, 5000, 30000));
  TEST_ASSERT_TRUE(ShroomConnectionIsTimedOut(&conn, 32000, 30000));
}

void test_connection_is_timed_out_disconnected(void) {
  ShroomConnection conn = {0};
  conn.state = SHROOM_CONN_STATE_DISCONNECTED;

  TEST_ASSERT_FALSE(ShroomConnectionIsTimedOut(&conn, 100000, 30000));
}

void test_connection_needs_ping(void) {
  ShroomConnection conn = {0};
  conn.state = SHROOM_CONN_STATE_CONNECTED;
  conn.last_ping_time = 1000;

  TEST_ASSERT_FALSE(ShroomConnectionNeedsPing(&conn, 2000, 5000));
  TEST_ASSERT_TRUE(ShroomConnectionNeedsPing(&conn, 7000, 5000));
}

void test_connection_needs_ping_never_pinged(void) {
  ShroomConnection conn = {0};
  conn.state = SHROOM_CONN_STATE_CONNECTED;
  conn.last_ping_time = 0;

  TEST_ASSERT_TRUE(ShroomConnectionNeedsPing(&conn, 1000, 5000));
}

void test_connection_needs_ping_not_connected(void) {
  ShroomConnection conn = {0};
  conn.state = SHROOM_CONN_STATE_CONNECTING;

  TEST_ASSERT_FALSE(ShroomConnectionNeedsPing(&conn, 10000, 5000));
}

void test_connection_reset(void) {
  ShroomConnection conn = {0};
  conn.peer_id = 5;
  conn.state = SHROOM_CONN_STATE_CONNECTED;
  conn.rtt_ms = 100;
  conn.packets_sent = 50;

  ShroomConnectionReset(&conn);

  TEST_ASSERT_EQUAL_UINT32(5, conn.peer_id); // peer_id should be preserved
  TEST_ASSERT_EQUAL(SHROOM_CONN_STATE_DISCONNECTED, conn.state);
  TEST_ASSERT_EQUAL_UINT32(0, conn.rtt_ms);
  TEST_ASSERT_EQUAL_UINT32(0, conn.packets_sent);
}

void test_connection_manager_get_active_count(void) {
  TEST_ASSERT_EQUAL_UINT32(0, ShroomConnectionManagerGetActiveCount(&manager));

  ShroomConnectionManagerAdd(&manager, 0);
  TEST_ASSERT_EQUAL_UINT32(1, ShroomConnectionManagerGetActiveCount(&manager));

  ShroomConnectionManagerAdd(&manager, 1);
  TEST_ASSERT_EQUAL_UINT32(2, ShroomConnectionManagerGetActiveCount(&manager));

  ShroomConnectionManagerRemove(&manager, 0);
  TEST_ASSERT_EQUAL_UINT32(1, ShroomConnectionManagerGetActiveCount(&manager));
}

void test_connection_manager_get_available_slot(void) {
  uint32_t slot = ShroomConnectionManagerGetAvailableSlot(&manager);
  TEST_ASSERT_EQUAL_UINT32(0, slot);

  ShroomConnectionManagerAdd(&manager, 0);
  slot = ShroomConnectionManagerGetAvailableSlot(&manager);
  TEST_ASSERT_EQUAL_UINT32(1, slot);

  ShroomConnectionManagerAdd(&manager, 1);
  slot = ShroomConnectionManagerGetAvailableSlot(&manager);
  TEST_ASSERT_EQUAL_UINT32(2, slot);
}

void test_connection_manager_get_available_slot_full(void) {
  for (uint32_t i = 0; i < 10; i++) {
    ShroomConnectionManagerAdd(&manager, i);
  }

  uint32_t slot = ShroomConnectionManagerGetAvailableSlot(&manager);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, slot);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_connection_manager_init);
  RUN_TEST(test_connection_manager_init_null);
  RUN_TEST(test_connection_manager_init_zero_max);
  RUN_TEST(test_connection_manager_shutdown);
  RUN_TEST(test_connection_manager_shutdown_null);
  RUN_TEST(test_connection_manager_add);
  RUN_TEST(test_connection_manager_add_multiple);
  RUN_TEST(test_connection_manager_add_duplicate);
  RUN_TEST(test_connection_manager_add_invalid_peer_id);
  RUN_TEST(test_connection_manager_remove);
  RUN_TEST(test_connection_manager_remove_nonexistent);
  RUN_TEST(test_connection_manager_get);
  RUN_TEST(test_connection_manager_get_nonexistent);
  RUN_TEST(test_connection_set_state);
  RUN_TEST(test_connection_set_state_null);
  RUN_TEST(test_connection_update_activity);
  RUN_TEST(test_connection_update_activity_null);
  RUN_TEST(test_connection_update_ping);
  RUN_TEST(test_connection_calculate_rtt);
  RUN_TEST(test_connection_calculate_rtt_invalid);
  RUN_TEST(test_connection_is_timed_out);
  RUN_TEST(test_connection_is_timed_out_disconnected);
  RUN_TEST(test_connection_needs_ping);
  RUN_TEST(test_connection_needs_ping_never_pinged);
  RUN_TEST(test_connection_needs_ping_not_connected);
  RUN_TEST(test_connection_reset);
  RUN_TEST(test_connection_manager_get_active_count);
  RUN_TEST(test_connection_manager_get_available_slot);
  RUN_TEST(test_connection_manager_get_available_slot_full);
  return UNITY_END();
}
