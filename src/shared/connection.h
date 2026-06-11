#ifndef SHROOM_CONNECTION_H
#define SHROOM_CONNECTION_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum ShroomConnectionState {
  SHROOM_CONN_STATE_DISCONNECTED = 0,
  SHROOM_CONN_STATE_CONNECTING,
  SHROOM_CONN_STATE_CONNECTED,
  SHROOM_CONN_STATE_DISCONNECTING
} ShroomConnectionState;

typedef struct ShroomConnection {
  uint32_t peer_id;
  ShroomConnectionState state;
  uint64_t connect_time;
  uint64_t last_activity_time;
  uint64_t last_ping_time;
  uint32_t ping_nonce;
  uint32_t rtt_ms;
  uint32_t packets_sent;
  uint32_t packets_received;
  uint32_t bytes_sent;
  uint32_t bytes_received;
  bool authenticated;
  uint32_t session_id;
} ShroomConnection;

typedef struct ShroomConnectionManager {
  ShroomConnection* connections;
  uint32_t max_connections;
  uint32_t active_connections;
  uint64_t connection_timeout_ms;
  uint64_t ping_interval_ms;
} ShroomConnectionManager;

void ShroomConnectionManagerInit(ShroomConnectionManager* manager, uint32_t max_connections);
void ShroomConnectionManagerShutdown(ShroomConnectionManager* manager);

ShroomConnection* ShroomConnectionManagerAdd(ShroomConnectionManager* manager, uint32_t peer_id);
void ShroomConnectionManagerRemove(ShroomConnectionManager* manager, uint32_t peer_id);
ShroomConnection* ShroomConnectionManagerGet(ShroomConnectionManager* manager, uint32_t peer_id);

void ShroomConnectionSetState(ShroomConnection* conn, ShroomConnectionState state);
void ShroomConnectionUpdateActivity(ShroomConnection* conn, uint64_t current_time);
void ShroomConnectionUpdatePing(ShroomConnection* conn, uint32_t nonce, uint64_t current_time);
uint32_t ShroomConnectionCalculateRTT(ShroomConnection* conn, uint64_t pong_time);

bool ShroomConnectionIsTimedOut(const ShroomConnection* conn, uint64_t current_time,
                                uint64_t timeout_ms);
bool ShroomConnectionNeedsPing(const ShroomConnection* conn, uint64_t current_time,
                               uint64_t ping_interval_ms);

void ShroomConnectionReset(ShroomConnection* conn);

uint32_t ShroomConnectionManagerGetActiveCount(const ShroomConnectionManager* manager);
uint32_t ShroomConnectionManagerGetAvailableSlot(const ShroomConnectionManager* manager);

#endif
