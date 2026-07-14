#include <enet/enet.h>

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "shared/net_telemetry.h"
#include "shared/protocol.h"

typedef struct BenchmarkConfig {
  uint32_t clients;
  uint32_t participants;
  uint32_t split_pieces;
  uint32_t duration_ms;
  uint16_t port;
  double min_input_hz;
  double min_snapshot_hz;
  double max_drop_percent;
  uint32_t max_deadline_failures;
} BenchmarkConfig;

static uint64_t TimeNanos(void) {
  struct timespec value;
  clock_gettime(CLOCK_MONOTONIC, &value);
  return (uint64_t)value.tv_sec * 1000000000ull + (uint64_t)value.tv_nsec;
}

static bool ParseUnsigned(const char* text, uint32_t minimum, uint32_t maximum, uint32_t* value) {
  char* end = NULL;
  unsigned long parsed;

  if ((text == NULL) || (*text == '\0')) {
    return false;
  }
  errno = 0;
  parsed = strtoul(text, &end, 10);
  if ((errno == ERANGE) || (end == text) || (*end != '\0') || (parsed < minimum) ||
      (parsed > maximum)) {
    return false;
  }
  *value = (uint32_t)parsed;
  return true;
}

static bool ParseDouble(const char* text, double minimum, double* value) {
  char* end = NULL;
  double parsed;

  if ((text == NULL) || (*text == '\0')) {
    return false;
  }
  errno = 0;
  parsed = strtod(text, &end);
  if ((errno == ERANGE) || (end == text) || (*end != '\0') || !isfinite(parsed) ||
      (parsed < minimum)) {
    return false;
  }
  *value = parsed;
  return true;
}

static bool LoadConfig(BenchmarkConfig* config, int argc, char** argv) {
  uint32_t parsed;

  *config = (BenchmarkConfig){
      .clients = 1u,
      .participants = 1u,
      .split_pieces = 1u,
      .duration_ms = 1500u,
      .port = 39777u,
      .min_input_hz = 20.0,
      .min_snapshot_hz = 10.0,
      .max_drop_percent = 1.0,
      .max_deadline_failures = 5u,
  };
  for (int index = 1; index < argc; ++index) {
#define NEXT_VALUE()                                                                               \
  do {                                                                                             \
    if (++index >= argc) {                                                                         \
      return false;                                                                                \
    }                                                                                              \
  } while (0)
    if (strcmp(argv[index], "--clients") == 0) {
      NEXT_VALUE();
      if (!ParseUnsigned(argv[index], 1u, 256u, &config->clients)) {
        return false;
      }
    } else if (strcmp(argv[index], "--participants") == 0) {
      NEXT_VALUE();
      if (!ParseUnsigned(argv[index], 0u, SHROOM_MAX_PARTICIPANTS, &config->participants)) {
        return false;
      }
    } else if (strcmp(argv[index], "--split-pieces") == 0) {
      NEXT_VALUE();
      if (!ParseUnsigned(argv[index], 1u, SHROOM_MAX_SPLIT_PIECES, &config->split_pieces)) {
        return false;
      }
    } else if (strcmp(argv[index], "--duration-ms") == 0) {
      NEXT_VALUE();
      if (!ParseUnsigned(argv[index], 250u, 60000u, &config->duration_ms)) {
        return false;
      }
    } else if (strcmp(argv[index], "--port") == 0) {
      NEXT_VALUE();
      if (!ParseUnsigned(argv[index], 1u, UINT16_MAX, &parsed)) {
        return false;
      }
      config->port = (uint16_t)parsed;
    } else if (strcmp(argv[index], "--min-input-hz") == 0) {
      NEXT_VALUE();
      if (!ParseDouble(argv[index], 0.0, &config->min_input_hz)) {
        return false;
      }
    } else if (strcmp(argv[index], "--min-snapshot-hz") == 0) {
      NEXT_VALUE();
      if (!ParseDouble(argv[index], 0.0, &config->min_snapshot_hz)) {
        return false;
      }
    } else if (strcmp(argv[index], "--max-drop-percent") == 0) {
      NEXT_VALUE();
      if (!ParseDouble(argv[index], 0.0, &config->max_drop_percent)) {
        return false;
      }
    } else if (strcmp(argv[index], "--max-deadline-failures") == 0) {
      NEXT_VALUE();
      if (!ParseUnsigned(argv[index], 0u, UINT32_MAX, &config->max_deadline_failures)) {
        return false;
      }
    } else {
      return false;
    }
#undef NEXT_VALUE
  }
  if (config->participants > config->clients) {
    config->participants = config->clients;
  }
  return true;
}

static void RecordSend(ShroomNetTelemetry* telemetry, ENetPeer* peer, uint8_t channel,
                       ShroomPacketType type, ENetPacket* packet, uint64_t now_ms) {
  const size_t peer_index = peer != NULL ? peer->outgoingPeerID : 0u;
  const size_t bytes = packet != NULL ? packet->dataLength : 0u;

  if ((peer == NULL) || (packet == NULL) || (enet_peer_send(peer, channel, packet) != 0)) {
    ShroomNetTelemetryRecordDrop(telemetry, peer_index, channel, type, bytes, now_ms);
    if (packet != NULL) {
      enet_packet_destroy(packet);
    }
    return;
  }
  ShroomNetTelemetryRecordSent(telemetry, peer_index, channel, type, bytes, now_ms);
}

static void ServiceHost(ENetHost* host, ShroomNetTelemetry* telemetry, bool server_side,
                        uint32_t* connected, uint32_t* invalid_snapshot_packets,
                        uint64_t now_ms) {
  ENetEvent event;

  while (enet_host_service(host, &event, 0) > 0) {
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
      ++*connected;
      break;
    case ENET_EVENT_TYPE_RECEIVE: {
      const ShroomPacketHeader* header = (const ShroomPacketHeader*)event.packet->data;
      const size_t peer_index =
          server_side ? event.peer->incomingPeerID : event.peer->outgoingPeerID;
      const bool known_type = event.packet->dataLength >= sizeof(*header) &&
                              header->type >= SHROOM_PACKET_HELLO &&
                              header->type <= SHROOM_PACKET_INTERMISSION_STATUS;
      const size_t minimum_size = event.packet->dataLength >= sizeof(*header)
                                      ? ShroomPacketTypeMinimumSize((ShroomPacketType)header->type)
                                      : 0u;
      const bool snapshot_size_valid =
          (event.packet->dataLength < sizeof(*header)) ||
          (header->type != SHROOM_PACKET_SNAPSHOT) ||
          (event.packet->dataLength <= SHROOM_MAX_UNRELIABLE_PACKET_SIZE);
      if ((event.packet->dataLength >= sizeof(*header)) && known_type &&
          ShroomPacketHeaderUsesExpectedChannel(header, event.channelID) &&
          (minimum_size <= event.packet->dataLength) &&
          ((size_t)header->size == event.packet->dataLength) && snapshot_size_valid) {
        ShroomNetTelemetryRecordAccepted(telemetry, peer_index, event.channelID,
                                         (ShroomPacketType)header->type, event.packet->dataLength,
                                         now_ms);
      } else {
        if (!snapshot_size_valid) {
          ++*invalid_snapshot_packets;
        }
        ShroomNetTelemetryRecordDrop(telemetry, peer_index, event.channelID,
                                     event.packet->dataLength >= sizeof(*header)
                                         ? (ShroomPacketType)header->type
                                         : (ShroomPacketType)0,
                                     event.packet->dataLength, now_ms);
      }
      enet_packet_destroy(event.packet);
    } break;
    case ENET_EVENT_TYPE_DISCONNECT:
      if (*connected > 0u) {
        --*connected;
      }
      break;
    case ENET_EVENT_TYPE_NONE:
    default:
      break;
    }
  }
}

static void SampleTransport(ENetHost* host, ShroomNetTelemetry* telemetry, bool server_side) {
  for (size_t index = 0u; index < host->peerCount; ++index) {
    ENetPeer* peer = &host->peers[index];
    const bool active = peer->state == ENET_PEER_STATE_CONNECTED;
    const size_t peer_index = server_side ? peer->incomingPeerID : peer->outgoingPeerID;
    const size_t queued = active ? enet_list_size(&peer->outgoingCommands) +
                                       enet_list_size(&peer->outgoingSendReliableCommands)
                                 : 0u;
    const uint16_t loss =
        active ? (uint16_t)(((uint64_t)peer->packetLoss * 10000u) / ENET_PEER_PACKET_LOSS_SCALE)
               : 0u;
    ShroomNetTelemetrySetPeerTransport(
        telemetry, peer_index, queued > UINT32_MAX ? UINT32_MAX : (uint32_t)queued, loss, active);
  }
}

static int RunBenchmark(const BenchmarkConfig* config) {
  ENetAddress address = {.host = ENET_HOST_ANY, .port = config->port};
  ENetAddress connect_address = {.port = config->port};
  ENetHost* server = NULL;
  ENetHost* clients = NULL;
  ShroomNetTelemetry telemetry;
  uint32_t server_connected = 0u;
  uint32_t client_connected = 0u;
  uint32_t tick_deadline_failures = 0u;
  uint32_t invalid_snapshot_packets = 0u;
  uint64_t started;
  uint64_t next_tick;
  uint64_t tick_index = 0u;
  const uint64_t tick_interval_nanos = (uint64_t)(1000000000.0 / (double)SHROOM_SERVER_TICK_RATE);
  const uint32_t snapshot_interval_ticks =
      (uint32_t)(SHROOM_SERVER_TICK_RATE / (float)SHROOM_SNAPSHOT_RATE);
  ShroomSnapshotPacket snapshot = {0};
  const size_t snapshot_player_count = (size_t)config->participants * config->split_pieces;
  const size_t snapshot_chunk_count =
      snapshot_player_count == 0u
          ? 1u
          : (snapshot_player_count + SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK - 1u) /
                SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK;
  int exit_code = 1;

  ShroomNetTelemetryReset(&telemetry);
  if ((snapshot_player_count > SHROOM_MAX_SNAPSHOT_PLAYERS) ||
      (snapshot_chunk_count > SHROOM_SNAPSHOT_MAX_CHUNKS)) {
    goto cleanup;
  }
  snapshot.total_player_count = (uint16_t)snapshot_player_count;
  snapshot.chunk_count = (uint16_t)snapshot_chunk_count;
  server = enet_host_create(&address, config->clients, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  clients = enet_host_create(NULL, config->clients, SHROOM_ENET_CHANNEL_COUNT, 0u, 0u);
  if ((server == NULL) || (clients == NULL) ||
      (enet_address_set_host(&connect_address, "127.0.0.1") != 0)) {
    goto cleanup;
  }
  for (uint32_t index = 0u; index < config->clients; ++index) {
    if (enet_host_connect(clients, &connect_address, SHROOM_ENET_CHANNEL_COUNT, 0u) == NULL) {
      goto cleanup;
    }
  }
  started = TimeNanos();
  while (((server_connected < config->clients) || (client_connected < config->clients)) &&
         ((TimeNanos() - started) < 10000000000ull)) {
    const uint64_t now_ms = TimeNanos() / 1000000ull;
    ServiceHost(server, &telemetry, true, &server_connected, &invalid_snapshot_packets, now_ms);
    ServiceHost(clients, &telemetry, false, &client_connected, &invalid_snapshot_packets, now_ms);
    enet_host_flush(server);
    enet_host_flush(clients);
  }
  if ((server_connected != config->clients) || (client_connected != config->clients)) {
    fprintf(stderr, "loopback connection timeout: server=%u client=%u expected=%u\n",
            server_connected, client_connected, config->clients);
    goto cleanup;
  }

  started = TimeNanos();
  next_tick = started;
  while ((TimeNanos() - started) < (uint64_t)config->duration_ms * 1000000ull) {
    uint64_t now = TimeNanos();
    const uint64_t now_ms = now / 1000000ull;

    ServiceHost(server, &telemetry, true, &server_connected, &invalid_snapshot_packets, now_ms);
    ServiceHost(clients, &telemetry, false, &client_connected, &invalid_snapshot_packets, now_ms);
    if (now >= next_tick) {
      ShroomInputPacket input = {0};
      const uint64_t deadline = next_tick + tick_interval_nanos;
      ShroomPacketHeaderInit(&input.header, SHROOM_PACKET_INPUT, sizeof(input));
      input.sequence = (uint32_t)tick_index;
      for (size_t index = 0u; index < clients->peerCount; ++index) {
        ENetPeer* peer = &clients->peers[index];
        if (peer->state == ENET_PEER_STATE_CONNECTED) {
          RecordSend(&telemetry, peer, SHROOM_ENET_CHANNEL_INPUT, SHROOM_PACKET_INPUT,
                     enet_packet_create(&input, sizeof(input), 0u), now_ms);
        }
      }
      if ((tick_index % snapshot_interval_ticks) == 0u) {
        snapshot.tick = tick_index;
        for (size_t chunk_index = 0u; chunk_index < snapshot_chunk_count; ++chunk_index) {
          const size_t player_offset = chunk_index * SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK;
          const size_t remaining = snapshot_player_count - player_offset;
          const size_t snapshot_size =
              offsetof(ShroomSnapshotPacket, players) +
              (remaining < SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK
                   ? remaining
                   : SHROOM_SNAPSHOT_PLAYERS_PER_CHUNK) *
                  sizeof(ShroomSnapshotPlayerState);
          snapshot.chunk_index = (uint16_t)chunk_index;
          snapshot.player_count = (uint16_t)((snapshot_size -
                                              offsetof(ShroomSnapshotPacket, players)) /
                                             sizeof(ShroomSnapshotPlayerState));
          if (snapshot_size > SHROOM_MAX_UNRELIABLE_PACKET_SIZE) {
            ++invalid_snapshot_packets;
            continue;
          }
          ShroomPacketHeaderInit(&snapshot.header, SHROOM_PACKET_SNAPSHOT,
                                 (uint16_t)snapshot_size);
          for (size_t index = 0u; index < server->peerCount; ++index) {
            ENetPeer* peer = &server->peers[index];
            if (peer->state == ENET_PEER_STATE_CONNECTED) {
              RecordSend(&telemetry, peer, SHROOM_ENET_CHANNEL_SNAPSHOT,
                         SHROOM_PACKET_SNAPSHOT,
                         enet_packet_create(&snapshot, snapshot_size,
                                            ENET_PACKET_FLAG_UNSEQUENCED),
                         now_ms);
            }
          }
        }
      }
      enet_host_flush(clients);
      enet_host_flush(server);
      SampleTransport(server, &telemetry, true);
      SampleTransport(clients, &telemetry, false);
      if (TimeNanos() > deadline) {
        ++tick_deadline_failures;
      }
      ++tick_index;
      next_tick += tick_interval_nanos;
    }
  }
  for (uint32_t drain = 0u; drain < 100u; ++drain) {
    const uint64_t now_ms = TimeNanos() / 1000000ull;
    ServiceHost(server, &telemetry, true, &server_connected, &invalid_snapshot_packets, now_ms);
    ServiceHost(clients, &telemetry, false, &client_connected, &invalid_snapshot_packets, now_ms);
    enet_host_flush(server);
    enet_host_flush(clients);
  }
  {
    ShroomNetTelemetryWindow transport;
    const double elapsed_seconds = (double)(TimeNanos() - started) / 1000000000.0;
    const double input_rate =
        (double)telemetry.by_type[SHROOM_PACKET_INPUT].accepted.packets / elapsed_seconds;
    const double snapshot_rate =
        (double)telemetry.by_type[SHROOM_PACKET_SNAPSHOT].accepted.packets / elapsed_seconds;
    const uint64_t sent_packets = telemetry.totals.sent.packets;
    const uint64_t dropped_packets = telemetry.totals.dropped.packets;
    const double drop_percent =
        sent_packets + dropped_packets > 0u
            ? (double)dropped_packets * 100.0 / (double)(sent_packets + dropped_packets)
            : 0.0;
    const double input_per_client = input_rate / config->clients;
    const double snapshot_per_client = snapshot_rate / config->clients;
    ShroomNetTelemetryReadWindow(&telemetry, TimeNanos() / 1000000ull,
                                 SHROOM_NET_TELEMETRY_WINDOW_MS, &transport);
    printf("scenario,clients,participants,spectators,split_pieces,duration_ms,"
           "input_sent_messages,input_messages,input_messages_per_sec,input_bytes_per_sec,"
           "snapshot_sent_messages,snapshot_messages,snapshot_messages_per_sec,"
           "snapshot_bytes_per_sec,dropped_packets,drop_percent,queue_high_water,"
           "tick_deadline_failures,threshold_pass\n");
    exit_code = (input_per_client >= config->min_input_hz) &&
                        (snapshot_per_client >= config->min_snapshot_hz) &&
                        (drop_percent <= config->max_drop_percent) &&
                        (tick_deadline_failures <= config->max_deadline_failures) &&
                        (invalid_snapshot_packets == 0u)
                    ? 0
                    : 2;
    printf("enet_loopback,%u,%u,%u,%u,%u,%llu,%llu,%.3f,%.3f,%llu,%llu,%.3f,%.3f,%llu,"
           "%.3f,%u,%u,%s\n",
           config->clients, config->participants, config->clients - config->participants,
           config->split_pieces, config->duration_ms,
           (unsigned long long)telemetry.by_type[SHROOM_PACKET_INPUT].sent.packets,
           (unsigned long long)telemetry.by_type[SHROOM_PACKET_INPUT].accepted.packets, input_rate,
           (double)telemetry.by_type[SHROOM_PACKET_INPUT].accepted.bytes / elapsed_seconds,
           (unsigned long long)telemetry.by_type[SHROOM_PACKET_SNAPSHOT].sent.packets,
           (unsigned long long)telemetry.by_type[SHROOM_PACKET_SNAPSHOT].accepted.packets,
           snapshot_rate,
           (double)telemetry.by_type[SHROOM_PACKET_SNAPSHOT].accepted.bytes / elapsed_seconds,
           (unsigned long long)dropped_packets, drop_percent, transport.queue_high_water,
           tick_deadline_failures, exit_code == 0 ? "true" : "false");
    if (exit_code != 0) {
      fprintf(
          stderr,
          "threshold failure: input/client=%.2f snapshot/client=%.2f drops=%.2f%% deadlines=%u "
          "oversized_snapshots=%u\n",
          input_per_client, snapshot_per_client, drop_percent, tick_deadline_failures,
          invalid_snapshot_packets);
    }
  }

cleanup:
  if (clients != NULL) {
    enet_host_destroy(clients);
  }
  if (server != NULL) {
    enet_host_destroy(server);
  }
  return exit_code;
}

int main(int argc, char** argv) {
  BenchmarkConfig config;
  int result;

  if (!LoadConfig(&config, argc, argv)) {
    fprintf(stderr, "invalid network benchmark arguments\n");
    return 1;
  }
  if (enet_initialize() != 0) {
    fprintf(stderr, "ENet initialization failed\n");
    return 1;
  }
  result = RunBenchmark(&config);
  enet_deinitialize();
  return result;
}
