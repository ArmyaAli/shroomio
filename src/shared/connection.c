#include "connection.h"
#include <stdlib.h>
#include <string.h>

void ShroomConnectionManagerInit(ShroomConnectionManager* manager, uint32_t max_connections) {
  if (manager == NULL) {
    return;
  }

  manager->max_connections = max_connections;
  manager->active_connections = 0;
  manager->connection_timeout_ms = 30000; // 30 seconds default timeout
  manager->ping_interval_ms = 5000;       // 5 seconds default ping interval

  if (max_connections == 0) {
    manager->connections = NULL;
    return;
  }

  manager->connections = (ShroomConnection*)calloc(max_connections, sizeof(ShroomConnection));
}

void ShroomConnectionManagerShutdown(ShroomConnectionManager* manager) {
  if (manager == NULL || manager->connections == NULL) {
    return;
  }

  free(manager->connections);
  manager->connections = NULL;
  manager->max_connections = 0;
  manager->active_connections = 0;
}

ShroomConnection* ShroomConnectionManagerAdd(ShroomConnectionManager* manager, uint32_t peer_id) {
  if (manager == NULL || manager->connections == NULL || peer_id >= manager->max_connections) {
    return NULL;
  }

  ShroomConnection* conn = &manager->connections[peer_id];
  if (conn->state != SHROOM_CONN_STATE_DISCONNECTED) {
    return NULL; // Slot already in use
  }

  ShroomConnectionReset(conn);
  conn->peer_id = peer_id;
  conn->state = SHROOM_CONN_STATE_CONNECTING;
  manager->active_connections++;

  return conn;
}

void ShroomConnectionManagerRemove(ShroomConnectionManager* manager, uint32_t peer_id) {
  if (manager == NULL || manager->connections == NULL || peer_id >= manager->max_connections) {
    return;
  }

  ShroomConnection* conn = &manager->connections[peer_id];
  if (conn->state == SHROOM_CONN_STATE_DISCONNECTED) {
    return; // Already disconnected
  }

  ShroomConnectionReset(conn);
  if (manager->active_connections > 0) {
    manager->active_connections--;
  }
}

ShroomConnection* ShroomConnectionManagerGet(ShroomConnectionManager* manager, uint32_t peer_id) {
  if (manager == NULL || manager->connections == NULL || peer_id >= manager->max_connections) {
    return NULL;
  }

  ShroomConnection* conn = &manager->connections[peer_id];
  if (conn->state == SHROOM_CONN_STATE_DISCONNECTED) {
    return NULL;
  }

  return conn;
}

void ShroomConnectionSetState(ShroomConnection* conn, ShroomConnectionState state) {
  if (conn == NULL) {
    return;
  }
  conn->state = state;
}

void ShroomConnectionUpdateActivity(ShroomConnection* conn, uint64_t current_time) {
  if (conn == NULL) {
    return;
  }
  conn->last_activity_time = current_time;
}

void ShroomConnectionUpdatePing(ShroomConnection* conn, uint32_t nonce, uint64_t current_time) {
  if (conn == NULL) {
    return;
  }
  conn->last_ping_time = current_time;
  conn->ping_nonce = nonce;
}

uint32_t ShroomConnectionCalculateRTT(ShroomConnection* conn, uint64_t pong_time) {
  if (conn == NULL || pong_time < conn->last_ping_time) {
    return 0;
  }

  uint64_t rtt = pong_time - conn->last_ping_time;
  conn->rtt_ms = (uint32_t)rtt;
  return conn->rtt_ms;
}

bool ShroomConnectionIsTimedOut(ShroomConnection* conn, uint64_t current_time,
                                uint64_t timeout_ms) {
  if (conn == NULL || conn->state == SHROOM_CONN_STATE_DISCONNECTED) {
    return false;
  }

  if (current_time < conn->last_activity_time) {
    return false;
  }

  uint64_t elapsed = current_time - conn->last_activity_time;
  return elapsed > timeout_ms;
}

bool ShroomConnectionNeedsPing(ShroomConnection* conn, uint64_t current_time,
                               uint64_t ping_interval_ms) {
  if (conn == NULL || conn->state != SHROOM_CONN_STATE_CONNECTED) {
    return false;
  }

  // If never pinged before (last_ping_time is 0), needs ping
  if (conn->last_ping_time == 0) {
    return true;
  }

  if (current_time < conn->last_ping_time) {
    return true; // Clock wrapped or invalid state
  }

  uint64_t elapsed = current_time - conn->last_ping_time;
  return elapsed >= ping_interval_ms;
}

void ShroomConnectionReset(ShroomConnection* conn) {
  if (conn == NULL) {
    return;
  }

  uint32_t peer_id = conn->peer_id;
  memset(conn, 0, sizeof(ShroomConnection));
  conn->peer_id = peer_id;
  conn->state = SHROOM_CONN_STATE_DISCONNECTED;
}

uint32_t ShroomConnectionManagerGetActiveCount(ShroomConnectionManager* manager) {
  if (manager == NULL) {
    return 0;
  }
  return manager->active_connections;
}

uint32_t ShroomConnectionManagerGetAvailableSlot(ShroomConnectionManager* manager) {
  if (manager == NULL || manager->connections == NULL) {
    return UINT32_MAX;
  }

  for (uint32_t i = 0; i < manager->max_connections; i++) {
    if (manager->connections[i].state == SHROOM_CONN_STATE_DISCONNECTED) {
      return i;
    }
  }

  return UINT32_MAX; // No available slots
}
