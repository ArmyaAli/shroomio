#ifndef SHROOM_TOOLS_SERVER_HEALTHCHECK_H
#define SHROOM_TOOLS_SERVER_HEALTHCHECK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "shared/protocol.h"

#define SHROOM_HEALTHCHECK_GENERATION 0x48454c54u
#define SHROOM_HEALTHCHECK_NONCE 0x48534843u

typedef struct ShroomHealthcheckConfig {
  const char* host;
  uint16_t port;
  uint32_t timeout_ms;
} ShroomHealthcheckConfig;

bool ShroomHealthcheckLoadConfig(ShroomHealthcheckConfig* config, int argc, char** argv);
bool ShroomHealthcheckValidateResponse(uint8_t channel, const uint8_t* data, size_t size,
                                       const ShroomServerProbeResponsePacket** response);

#endif
