#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "client/server_discovery.h"

static uint64_t NowMs(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000ull + (uint64_t)now.tv_nsec / 1000000ull;
}

static bool ParseUint16(const char* text, uint16_t* value) {
  char* end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);
  if ((text == end) || (*end != '\0') || (parsed == 0u) || (parsed > UINT16_MAX)) {
    return false;
  }
  *value = (uint16_t)parsed;
  return true;
}

int main(int argc, char** argv) {
  ShroomServerDiscovery discovery = {0};
  uint16_t directory_port;
  uint16_t expected_count;
  const uint64_t started_ms = NowMs();
  int result = 1;

  if ((argc != 4) || !ParseUint16(argv[2], &directory_port) ||
      !ParseUint16(argv[3], &expected_count) ||
      (expected_count > SHROOM_DIRECTORY_MAX_ENTRIES)) {
    fprintf(stderr, "usage: %s DIRECTORY_HOST DIRECTORY_PORT EXPECTED_COUNT\n", argv[0]);
    return 2;
  }
  if (!ShroomServerDiscoveryBegin(&discovery, argv[1], directory_port, started_ms)) {
    fprintf(stderr, "discovery could not start\n");
    return 1;
  }
  while (ShroomServerDiscoveryIsActive(&discovery) &&
         ((NowMs() - started_ms) <= SHROOM_DISCOVERY_OVERALL_TIMEOUT_MS + 1000ull)) {
    struct timespec delay = {.tv_nsec = 1000000};
    ShroomServerDiscoveryUpdate(&discovery, NowMs());
    nanosleep(&delay, NULL);
  }
  if (discovery.state.phase != SHROOM_DISCOVERY_COMPLETE) {
    fprintf(stderr, "discovery failed: phase=%d\n", (int)discovery.state.phase);
    goto cleanup;
  }
  if (ShroomServerDiscoveryStateResultCount(&discovery.state) != expected_count) {
    fprintf(stderr, "discovery count mismatch: expected=%u actual=%zu\n", expected_count,
            ShroomServerDiscoveryStateResultCount(&discovery.state));
    goto cleanup;
  }
  for (size_t index = 0u; index < expected_count; ++index) {
    const ShroomDiscoveryCandidate* candidate =
        ShroomServerDiscoveryStateResult(&discovery.state, index);
    if ((candidate == NULL) || (candidate->server.player_count != 0u) ||
        (candidate->server.capacity != SHROOM_SERVER_MAX_CLIENTS) ||
        (candidate->latency_ms > SHROOM_DISCOVERY_PROBE_TIMEOUT_MS)) {
      fprintf(stderr, "invalid live candidate at index %zu\n", index);
      goto cleanup;
    }
    printf("%s %s:%u players=%u/%u latency=%ums\n", candidate->server.name,
           candidate->server.host, candidate->server.port, candidate->server.player_count,
           candidate->server.capacity, candidate->latency_ms);
  }
  result = 0;

cleanup:
  ShroomServerDiscoveryShutdown(&discovery);
  return result;
}
